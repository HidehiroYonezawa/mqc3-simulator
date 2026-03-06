#pragma once

#include <fmt/format.h>

#include <Python.h>
#include <string>
#include <vector>

#include "bosim/circuit.h"
#include "bosim/exception.h"
#include "bosim/python/manager.h"

namespace bosim::python {
/**
 * @brief A C++ class that executes Python functions in sequence.
 *
 * This class takes a list of Python function definitions as input,
 * compiles them, and executes them sequentially with numerical input.
 * It ensures safe interaction with the Python interpreter using the GIL.
 */
class PythonFeedForward {
private:
    std::vector<PyObject*> funcs_;  ///< List of compiled Python function objects.

public:
    /**
     * @brief Constructs a PythonFeedForward object and compiles the given Python functions.
     * @param codes A vector of Python function definitions as strings.
     *
     * @details This constructor compiles the given Python function definitions and stores them
     * for later execution. Each function is verified to be callable before being added to the
     * execution list.
     *
     * @throws std::runtime_error If the Python code cannot be parsed or the function is not
     * callable.
     */
    explicit PythonFeedForward(const std::vector<std::string>& codes) {
        const auto gil = PythonGIL();

        for (const auto& code : codes) {
            // Create a new execution scope.
            PyObject* globals = PyDict_New();
            PyObject* scope = PyDict_New();

            const auto buffer = "# -*- coding: utf-8 -*-\n" + code;
            if (PyRun_String(buffer.c_str(), Py_file_input, globals, scope) == nullptr) {
                PyErr_Print();
                Py_DECREF(scope);
                Py_DECREF(globals);
                throw SimulationError(error::InvalidFF,
                                      "Failed to parse Python script:\n\n" + code);
            }

            // Retrieve the first function defined in the scope.
            PyObject* keys = PyDict_Keys(scope);
            PyObject* func_name = PyList_GetItem(keys, 0);  // Get the first key (function name).
            PyObject* func = PyDict_GetItem(scope, func_name);

            if (PyCallable_Check(func) == 0) {
                PyErr_Print();
                Py_DECREF(keys);
                Py_DECREF(scope);
                Py_DECREF(globals);
                throw SimulationError(error::InvalidFF, "Python function is not callable");
            }

            Py_INCREF(func);
            funcs_.push_back(func);  // Store the function for later execution.

            // Release unused Python objects.
            Py_DECREF(keys);
            Py_DECREF(scope);
            Py_DECREF(globals);
        }
    }

    /**
     * @brief Copy constructor (performs a deep copy of Python objects).
     * @param other The PythonFeedForward instance to copy.
     */
    PythonFeedForward(const PythonFeedForward& other) {
        funcs_ = other.funcs_;

        const auto gil = PythonGIL();
        for (auto* func : funcs_) { Py_INCREF(func); }
    }

    /**
     * @brief Move constructor.
     * @param other The PythonFeedForward instance to move.
     */
    PythonFeedForward(PythonFeedForward&& other) noexcept {
        funcs_.swap(other.funcs_);
        other.funcs_.clear();
    }

    PythonFeedForward& operator=(const PythonFeedForward& other) = delete;
    PythonFeedForward& operator=(PythonFeedForward&& other) = delete;

    /**
     * @brief Destructor that releases allocated Python objects.
     *
     * Ensures that all stored Python function objects are properly
     * deallocated to prevent memory leaks.
     */
    ~PythonFeedForward() {
        const auto gil = PythonGIL();
        for (auto* func : funcs_) {
            // Decrease reference count to properly manage memory.
            Py_DECREF(func);
        }
    }

    /**
     * @brief Executes the stored Python functions sequentially with the given inputs.
     * @param inputs A vector of double values representing function inputs.
     * @return A single double value computed from the sequence of Python function calls.
     *
     * This method converts the input vector into a Python tuple, calls each
     * stored Python function in sequence, and updates the tuple with the function's output.
     * If a function returns a tuple, it is used as the input for the next function.
     * The final function's return value is converted to `double`.
     *
     * @throws std::runtime_error If a Python function call fails or returns an unsupported type.
     */
    double operator()(const std::vector<double>& inputs) const {
        const auto gil = PythonGIL();

        // Convert the input vector into a Python tuple (created once and reused).
        PyObject* args = PyTuple_New(static_cast<std::int64_t>(inputs.size()));
        for (auto i = std::size_t{0}; i < inputs.size(); ++i) {
            PyTuple_SET_ITEM(args, static_cast<std::int64_t>(i), PyFloat_FromDouble(inputs[i]));
            // Do NOT decrement the reference count of PyFloat_FromDouble.
            // PyTuple_SET_ITEM() transfers ownership to the tuple.
        }

        PyObject* py_ret = nullptr;

        // Execute each stored Python function in sequence.
        for (auto* func : funcs_) {
            py_ret = PyObject_CallObject(func, args);
            Py_DECREF(args);  // Release the previous input.

            if (py_ret == nullptr) {
                PyErr_Print();
                throw SimulationError(error::InvalidFF, "Python function call failed");
            }
            if (func == funcs_.back()) { break; }

            // If the return value is a tuple, use it as the next input.
            if (PyTuple_Check(py_ret)) {
                args = py_ret;
            }
            // If the return value is a single float, wrap it in a tuple.
            else if (PyFloat_Check(py_ret)) {
                args = PyTuple_New(1);
                PyTuple_SET_ITEM(args, 0, py_ret);  // Ownership transferred to args
            } else {
                Py_DECREF(py_ret);
                throw SimulationError(error::InvalidFF,
                                      "Python function returned an unsupported type");
            }
        }

        // Convert the final value to a double and return it.
        const auto result = PyFloat_AsDouble(py_ret);
        if (PyErr_Occurred() != nullptr) {
            Py_DECREF(py_ret);
            throw SimulationError(error::InvalidFF,
                                  "Failed to convert Python return value to double");
        }
        Py_DECREF(py_ret);

        return result;
    }
};

/**
 * @brief Convert `PythonFeedForward` to `bosim::FFFunc`.
 *
 * @tparam Float The floating point type.
 * @param python_function Python feedforward function.
 * @return Non-linear feedforward function.
 */
template <std::floating_point Float>
bosim::FFFunc<Float> ConvertToFFFunc(const PythonFeedForward& python_function) {
    return bosim::FFFunc<Float>{[python_function](const std::vector<Float>& args) -> Float {
        // Cast `args` to `double`, call the Python functions, and cast the result back to `Float`.
        const auto double_args = [](const auto& args) {
            std::vector<double> double_args;
            double_args.reserve(args.size());
            std::transform(args.begin(), args.end(), std::back_inserter(double_args),
                           [](const auto& arg) { return static_cast<double>(arg); });
            return double_args;
        }(args);
        return static_cast<Float>(python_function(double_args));
    }};
}
}  // namespace bosim::python
