"""自动发现机制的验证算法。"""

from alg.core.decorators import algorithm


@algorithm("test_auto_discovery")
def run(input_data, params):
    """返回注册名称与传入参数，用于验证 @algorithm 自动发现可用。"""
    return {
        "success": True,
        "data": {
            "registered_name": "test_auto_discovery",
            "value": params.get("value"),
        },
    }
