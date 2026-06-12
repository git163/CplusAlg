import numpy as np
import pytest
from scipy.special import erf

from alg import curve_fit


def test_curve_fit_erf():
    x = np.linspace(-3, 3, 100)
    y = erf(x) + np.random.default_rng(42).normal(0, 0.05, size=x.shape)

    params = {
        "x": x.tolist(),
        "y": y.tolist(),
        "p0": [1.0, 1.0, 0.0, 0.0],
        "plot_path": "test_curve_fit.png",
    }

    result = curve_fit.run(None, params)

    assert result["success"], result.get("error", "")
    fitted = result["data"]["params"]
    assert len(fitted) == 4
    assert abs(fitted[0] - 1.0) < 0.1  # a
    assert abs(fitted[1] - 1.0) < 0.1  # b
    assert abs(fitted[2]) < 0.1       # c
    assert abs(fitted[3]) < 0.1       # d
    assert result["data"]["plot_path"] == "test_curve_fit.png"


def test_curve_fit_missing_data():
    result = curve_fit.run(None, {})
    assert not result["success"]


def test_curve_fit_mismatched_shape():
    result = curve_fit.run(None, {"x": [1, 2, 3], "y": [1, 2]})
    assert not result["success"]
