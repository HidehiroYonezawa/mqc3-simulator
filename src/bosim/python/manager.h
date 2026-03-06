#pragma once

#include <Python.h>
#include <mutex>

#include "bosim/base/log.h"

namespace bosim::python {
/**
 * @brief Manages the initialization and finalization of the Python interpreter.
 *
 * This class ensures that the Python interpreter is initialized only once and
 * properly finalized when it is no longer needed. Additionally, it releases the
 * Global Interpreter Lock (GIL) after initialization to enable multi-threaded execution.
 *
 * @note This class is thread-safe and prevents multiple initializations.
 */
class PythonEnvironment {
public:
    /**
     * @brief Initializes the Python interpreter if it has not been initialized.
     *
     * The first instance of this class initializes the Python interpreter
     * using `Py_Initialize()`. It also releases the GIL via `PyEval_SaveThread()`
     * to allow other threads to acquire it when needed.
     */
    PythonEnvironment() {
        const auto lock = std::lock_guard<std::mutex>(Mutex);
        if (!Initialized) {
            Py_Initialize();
            MainThreadState = PyEval_SaveThread();  // Release GIL for other threads
            Initialized = true;
            LOG_INFO("[PythonEnvironment] Python interpreter initialized, GIL released.");
        }
    }

    /**
     * @brief Finalizes the Python interpreter.
     *
     * The last instance of this class restores the GIL (`PyEval_RestoreThread()`)
     * before finalizing Python (`Py_Finalize()`), ensuring proper cleanup.
     */
    ~PythonEnvironment() {
        const auto lock = std::lock_guard<std::mutex>(Mutex);
        if (Initialized) {
            PyEval_RestoreThread(MainThreadState);  // Restore GIL before finalizing
            Py_Finalize();
            Initialized = false;
            LOG_INFO("[PythonEnvironment] Python interpreter finalized.");
        }
    }

    /// Delete copy constructor and assignment operator to prevent multiple initializations.
    PythonEnvironment(const PythonEnvironment&) = delete;
    PythonEnvironment& operator=(const PythonEnvironment&) = delete;

private:
    // NOLINTBEGIN(readability-identifier-naming)
    static inline std::mutex Mutex;          ///< Mutex for thread safety.
    static inline bool Initialized = false;  ///< Indicates whether Python has been initialized.
    static inline PyThreadState* MainThreadState =
        nullptr;  ///< Stores the main thread state after releasing the GIL.
    // NOLINTEND(readability-identifier-naming)
};

/**
 * @brief Manages the Global Interpreter Lock (GIL) to ensure safe Python execution.
 *
 * This class ensures that the Global Interpreter Lock (GIL) is properly acquired
 * and released when executing Python code in a multi-threaded environment.
 *
 * @note An instance of this class **must** be created within the scope of any
 * Python code execution to ensure thread safety.
 */
class PythonGIL {
public:
    /**
     * @brief Acquires the Global Interpreter Lock (GIL).
     *
     * When an instance of this class is created, the calling thread
     * acquires the GIL using `PyGILState_Ensure()`, ensuring safe execution of Python code.
     */
    PythonGIL() { gil_state_ = PyGILState_Ensure(); }

    /**
     * @brief Releases the Global Interpreter Lock (GIL) upon destruction.
     *
     * The destructor ensures that the GIL is properly released using `PyGILState_Release()`
     * when the instance goes out of scope, preventing deadlocks and ensuring proper GIL management.
     */
    ~PythonGIL() { PyGILState_Release(gil_state_); }

    /// Delete copy constructor and assignment operator to prevent incorrect usage.
    PythonGIL(const PythonGIL&) = delete;
    PythonGIL& operator=(const PythonGIL&) = delete;

private:
    PyGILState_STATE gil_state_;  ///< Stores the GIL state of the current thread.
};
}  // namespace bosim::python
