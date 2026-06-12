"""简单的输入回显模块，用于测试 C++ 接口的各种调用重载。"""

import numpy as np


def run(input_data, params):
    """返回输入数据信息和部分参数。

    input_data:
        - None: 无数据输入
        - {"type": "buffer", "array": np.ndarray, ...}
        - {"type": "shm", "handle": {...}}
    params: 参数字典，可包含 "value" 等测试字段
    """
    result = {
        "success": True,
        "data": {
            "has_input": input_data is not None,
            "input_type": input_data.get("type") if isinstance(input_data, dict) else None,
            "value": params.get("value"),
        },
    }

    if isinstance(input_data, dict) and input_data.get("type") == "buffer":
        arr = input_data["array"]
        if arr is not None:
            result["data"]["shape"] = list(arr.shape)
            result["data"]["dtype"] = str(arr.dtype)
            result["data"]["first_element"] = int(arr.flat[0]) if arr.size > 0 else None

    return result
