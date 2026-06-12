"""算法路由注册表。"""

import importlib

from alg.core import discovery

_ALGORITHMS = {}


def register_algorithm(name: str, func):
    """注册算法到全局注册表。

    Args:
        name: 算法名称。
        func: 算法入口函数，签名应为 run(input_data, params)。
    """
    _ALGORITHMS[name] = func


def dispatch(module_name: str, input_data, params: dict) -> dict:
    """根据模块名分发到对应算法。

    Args:
        module_name: 算法模块名。
        input_data: 输入数据。
        params: 参数字典。

    Returns:
        算法返回的统一 JSON 结构。
    """
    if module_name in _ALGORITHMS:
        try:
            return _ALGORITHMS[module_name](input_data, params)
        except Exception as e:
            return {"success": False, "error": str(e), "error_type": type(e).__name__}

    # 未注册时尝试懒加载 fallback
    try:
        mod = importlib.import_module(f"alg.{module_name}")
    except ModuleNotFoundError:
        return {
            "success": False,
            "error": f"unknown module: {module_name}",
            "error_type": "ModuleNotFoundError",
        }
    except Exception as e:
        return {"success": False, "error": str(e), "error_type": type(e).__name__}

    if not hasattr(mod, "run"):
        return {
            "success": False,
            "error": f"module '{module_name}' has no run() function",
            "error_type": "AttributeError",
        }

    try:
        return mod.run(input_data, params)
    except Exception as e:
        return {"success": False, "error": str(e), "error_type": type(e).__name__}


# 模块导入时自动发现注册
discovery.discover_and_register()
