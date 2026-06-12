"""算法路由注册表。"""

import importlib

_ALGORITHMS = {
    "template_match": "alg.template_match",
}


def dispatch(module_name: str, input_data, params: dict) -> dict:
    """根据模块名分发到对应算法。

    Args:
        module_name: 算法模块名。
        input_data: 输入数据。
        params: 参数字典。

    Returns:
        算法返回的统一 JSON 结构。
    """
    if module_name not in _ALGORITHMS:
        return {
            "success": False,
            "error": f"unknown module: {module_name}",
            "error_type": "ModuleNotFoundError",
        }
    try:
        mod = importlib.import_module(_ALGORITHMS[module_name])
        return mod.run(input_data, params)
    except Exception as e:
        return {"success": False, "error": str(e), "error_type": type(e).__name__}
