"""算法装饰器，提供 @algorithm 自动注册能力。"""

from alg.core import registry


def algorithm(name: str):
    """装饰器：将算法函数注册到全局注册表。

    Args:
        name: 算法对外暴露的名称。

    Returns:
        装饰器函数。
    """
    def decorator(func):
        registry.register_algorithm(name, func)
        return func
    return decorator
