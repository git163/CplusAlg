import numpy as np
import posix_ipc
import mmap

from alg.core.shm_io import read_image


def test_read_shm():
    arr = np.arange(60, dtype=np.uint8).reshape(3, 4, 5)
    name = "cplusalg_test_read"
    size = arr.nbytes

    shm = posix_ipc.SharedMemory(name, posix_ipc.O_CREX, size=size)
    try:
        buf = mmap.mmap(shm.fd, size)
        buf[:] = arr.tobytes()

        input_data = {
            "type": "shm",
            "handle": {
                "name": name,
                "shape": list(arr.shape),
                "dtype": str(arr.dtype),
                "size_bytes": size,
            },
        }

        result = read_image(input_data)
        assert result.shape == arr.shape
        assert result.dtype == arr.dtype
        assert np.array_equal(result, arr)
    finally:
        shm.close_fd()
        shm.unlink()


def test_read_buffer():
    arr = np.zeros((10, 10), dtype=np.uint8)
    input_data = {
        "type": "buffer",
        "array": arr,
        "shape": list(arr.shape),
        "dtype": str(arr.dtype),
        "size_bytes": arr.nbytes,
    }

    result = read_image(input_data)
    assert result.shape == arr.shape
    assert np.array_equal(result, arr)
