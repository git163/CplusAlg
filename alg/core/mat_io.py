"""小数据直接内存转换工具。"""

import numpy as np


def to_numpy(input_data: dict) -> np.ndarray:
    """把 C++ 直传的 buffer 转成 numpy array。"""
    if input_data.get("type") != "buffer":
        raise ValueError(f"expected type 'buffer', got {input_data.get('type')}")

    array = input_data["array"]
    shape = input_data["shape"]
    dtype = input_data["dtype"]

    if not isinstance(array, np.ndarray):
        array = np.array(array, dtype=dtype)

    return array.reshape(shape)
