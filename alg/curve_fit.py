"""使用 scipy 对 erf 误差函数进行 curve_fit 拟合并绘图。"""

import numpy as np
from scipy.special import erf
from scipy.optimize import curve_fit

import matplotlib
matplotlib.use("Agg")  # 无 GUI 后端
import matplotlib.pyplot as plt


def _erf_model(x, a, b, c, d):
    """a * erf(b * (x - c)) + d"""
    return a * erf(b * (x - c)) + d


from alg.core.decorators import algorithm


@algorithm("curve_fit")
def run(input_data, params):
    """拟合 erf 曲线并保存图像。

    Args:
        input_data: 未使用，可传入 None。
        params: 必须包含 "x" 和 "y"（list 或 numpy array）。
                可选 "p0" 初始参数、[a, b, c, d]。
                可选 "plot_path"，默认 "curve_fit.png"。

    Returns:
        {"success": True, "data": {"params": [...], "plot_path": "..."}}
        或 {"success": False, "error": ...}
    """
    try:
        x = np.asarray(params["x"], dtype=np.float64)
        y = np.asarray(params["y"], dtype=np.float64)
    except Exception as e:
        return {"success": False, "error": f"invalid x/y data: {e}", "error_type": type(e).__name__}

    if x.shape != y.shape:
        return {"success": False, "error": "x and y must have the same shape"}

    p0 = params.get("p0", [1.0, 1.0, 0.0, 0.0])
    plot_path = params.get("plot_path", "curve_fit.png")

    try:
        popt, _ = curve_fit(_erf_model, x, y, p0=p0)
    except Exception as e:
        return {"success": False, "error": str(e), "error_type": type(e).__name__}

    try:
        plt.figure(figsize=(8, 5))
        plt.scatter(x, y, label="data", s=20)
        x_fit = np.linspace(float(x.min()), float(x.max()), 200)
        plt.plot(x_fit, _erf_model(x_fit, *popt), "r-", label="erf fit")
        plt.xlabel("x")
        plt.ylabel("y")
        plt.title("erf curve fit")
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(plot_path)
        plt.close()
    except Exception as e:
        return {"success": False, "error": f"plot failed: {e}", "error_type": type(e).__name__}

    return {
        "success": True,
        "data": {
            "params": popt.tolist(),
            "plot_path": plot_path,
        },
    }
