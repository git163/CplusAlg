"""自动发现 alg 包下的算法模块并触发装饰器注册。"""

import importlib
import pkgutil

import alg


def discover_and_register():
    """扫描 alg 包下的非包模块并导入，触发 @algorithm 装饰器注册。"""
    for _, module_name, is_pkg in pkgutil.iter_modules(alg.__path__, alg.__name__ + "."):
        if not is_pkg:
            try:
                importlib.import_module(module_name)
            except Exception:
                # 允许部分模块因依赖缺失而跳过，不影响其他模块注册
                pass
