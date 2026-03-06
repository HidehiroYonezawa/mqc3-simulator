# ruff: noqa: INP001,S101

"""Statistics tools."""

import numpy as np
from numpy.typing import NDArray
from scipy.stats import chi2, ks_2samp, sem, t


def get_mean_interval(
    data: list[float],
    significance: float = 0.05,
) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Calculate the confidence interval for the mean of one-dimensional data. Assume a Gaussian distribution.

    Args:
        data (list[float]): The 1-dimensional data
        significance (float, optional): Significance of the confidence interval. Defaults to 0.05.

    Returns:
        tuple[NDArray[np.float64], NDArray[np.float64]]: Tuple of lower bound and upper bound of the calculated confidence interval
    """  # noqa: E501
    mean = np.mean(data)
    standard_error = sem(data)
    return t.interval(1 - significance, len(data) - 1, loc=mean, scale=standard_error)


def get_variance_interval(
    data: list[float],
    significance: float = 0.05,
) -> tuple[NDArray[np.float64], NDArray[np.float64]]:
    """Calculate the confidence interval for the variance of one-dimensional data. Assume a Gaussian distribution.

    Args:
        data (list[float]): The 1-dimensional data
        significance (float, optional): Significance of the confidence interval. Defaults to 0.05.

    Returns:
        tuple[NDArray[np.float64], NDArray[np.float64]]: Tuple of lower bound and upper bound of the calculated confidence interval
    """  # noqa: E501
    sample_var = np.var(data, ddof=1)
    df = len(data) - 1
    chi2_lower = chi2.ppf(significance / 2, df)
    chi2_upper = chi2.ppf(1 - significance / 2, df)
    lower_bound = (df * sample_var) / chi2_upper
    upper_bound = (df * sample_var) / chi2_lower
    return lower_bound, upper_bound


def check_mean_value(data: list[float], expected: float, significance: float = 0.05) -> None:
    """Check if the expected average falls within the confidence interval of the mean for the one-dimensional data. Assume a Gaussian distribution.

    Args:
        data (list[float]): The 1-dimensional data
        expected (float): Expected mean
        significance (float, optional): Significance of the confidence interval. Defaults to 0.05.
    """  # noqa: E501
    lower_bound, upper_bound = get_mean_interval(data=data, significance=significance)
    assert lower_bound < expected
    assert expected < upper_bound


def check_variance_value(data: list[float], expected: float, significance: float = 0.05) -> None:
    """Check if the expected variance falls within the confidence interval of the variance for the one-dimensional data. Assume a Gaussian distribution.

    Args:
        data (list[float]): The 1-dimensional data
        expected (float): Expected variance
        significance (float, optional): Significance of the confidence interval. Defaults to 0.05.
    """  # noqa: E501
    lower_bound, upper_bound = get_variance_interval(data=data, significance=significance)
    assert lower_bound < expected
    assert expected < upper_bound


def compare_1d_data(data1, data2, alpha=0.05) -> None:
    """Performs a Kolmogorov-Smirnov test to compare two one-dimensional datasets.

    It raises assertion error if the p-value is less than or equal to alpha.

    Parameters:
        data1 (list or array-like): The first one-dimensional dataset.
        data2 (list or array-like): The second one-dimensional dataset.
        alpha (float, optional): The significance level. Default is 0.05.
    """
    _, p_value = ks_2samp(data1, data2)
    assert p_value > alpha
