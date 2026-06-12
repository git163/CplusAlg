import numpy as np
import pytest

from alg import template_match


def test_template_match_buffer():
    image = np.zeros((100, 100), dtype=np.uint8)
    image[30:40, 20:30] = 255

    template = np.ones((10, 10), dtype=np.uint8) * 255

    input_data = {
        "type": "buffer",
        "array": image,
        "shape": list(image.shape),
        "dtype": str(image.dtype),
        "size_bytes": image.nbytes,
    }
    params = {"template": template, "method": "ccorr_normed"}

    result = template_match.run(input_data, params)

    assert result["success"]
    assert result["data"]["x"] == 20
    assert result["data"]["y"] == 30
    assert abs(result["data"]["score"] - 1.0) < 1e-6


def test_template_match_missing_template():
    image = np.zeros((10, 10), dtype=np.uint8)
    input_data = {
        "type": "buffer",
        "array": image,
        "shape": list(image.shape),
        "dtype": str(image.dtype),
        "size_bytes": image.nbytes,
    }
    result = template_match.run(input_data, {})

    assert not result["success"]
    assert "template" in result["error"]


def test_template_match_invalid_method():
    image = np.zeros((10, 10), dtype=np.uint8)
    input_data = {
        "type": "buffer",
        "array": image,
        "shape": list(image.shape),
        "dtype": str(image.dtype),
        "size_bytes": image.nbytes,
    }
    params = {"template": image, "method": "invalid"}
    result = template_match.run(input_data, params)

    assert not result["success"]
