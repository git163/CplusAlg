"""共享内存读写工具。"""

import mmap

import numpy as np


def read_image(input_data):
    """根据 input_data 类型读取数据。

    Args:
        input_data: None, {"type": "buffer", ...} 或 {"type": "shm", "handle": ...}。

    Returns:
        numpy.ndarray
    """
    if input_data is None:
        raise ValueError("input_data is None but this algorithm requires input")

    t = input_data.get("type")
    if t == "buffer":
        from alg.core.mat_io import to_numpy

        return to_numpy(input_data)

    if t == "shm":
        return _read_shm(input_data["handle"])

    raise ValueError(f"unknown input type: {t}")


def _read_shm(handle: dict) -> np.ndarray:
    """从 POSIX 共享内存读取 numpy array。"""
    try:
        import posix_ipc
    except ImportError as e:
        raise ImportError("posix_ipc is required for shared memory input") from e

    name = handle["name"]
    shape = handle["shape"]
    dtype = handle["dtype"]
    size_bytes = handle["size_bytes"]

    shm = posix_ipc.SharedMemory(name)
    buf = mmap.mmap(shm.fd, size_bytes)
    arr = np.frombuffer(buf, dtype=dtype).reshape(shape)
    return arr
