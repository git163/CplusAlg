import cv2
import numpy as np

from alg.core.shm_io import read_image


def _method_from_str(method: str) -> int:
    mapping = {
        "sqdiff": cv2.TM_SQDIFF,
        "sqdiff_normed": cv2.TM_SQDIFF_NORMED,
        "ccorr": cv2.TM_CCORR,
        "ccorr_normed": cv2.TM_CCORR_NORMED,
        "ccoef": cv2.TM_CCOEFF,
        "ccoef_normed": cv2.TM_CCOEFF_NORMED,
    }
    if method not in mapping:
        raise ValueError(f"unknown match method: {method}")
    return mapping[method]


def run(input_data, params: dict) -> dict:
    """模板匹配算法入口。

    Args:
        input_data: C++ 传入的图像数据，可能是 buffer 或 shm handle。
        params: 参数字典，必须包含 "template"，可选 "method"。

    Returns:
        {"success": True, "data": {"x", "y", "score"}}
        或 {"success": False, "error": "...", "error_type": "..."}
    """
    try:
        image = read_image(input_data)
        template = params.get("template")
        if template is None:
            return {"success": False, "error": "missing param: template"}

        method = params.get("method", "ccorr_normed")
        cv_method = _method_from_str(method)

        result = cv2.matchTemplate(image, template, cv_method)
        _, max_val, _, max_loc = cv2.minMaxLoc(result)

        return {
            "success": True,
            "data": {
                "x": int(max_loc[0]),
                "y": int(max_loc[1]),
                "score": float(max_val),
            },
        }
    except Exception as e:
        return {"success": False, "error": str(e), "error_type": type(e).__name__}
