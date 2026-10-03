"""Export a deterministic morph reference to ONNX, not an untrained neural model.

This small graph verifies the serving contract without Torch or training data.
Replace it with your trained decoder using the same input/output names and shapes.
"""
import argparse
from pathlib import Path
import numpy as np


def export_to_onnx(model_path="forge_synth/models/forge_reference.onnx"):
    import onnx
    from onnx import TensorProto, helper, numpy_helper
    from .ai_routing import ForgeSynthAI, Settings, TABLE_SIZE

    engine = ForgeSynthAI(Settings())
    # Bilinear interpolation: c0 + x*(c1-c0) + y*(c2-c0) + xy*(c0-c1-c2+c3).
    c0, c1, c2, c3 = engine.corners
    constants = {"base": c0[None, :], "dx": (c1-c0)[None, :], "dy": (c2-c0)[None, :],
                 "dxy": (c0-c1-c2+c3)[None, :], "ix": np.array([0], dtype=np.int64), "iy": np.array([1], dtype=np.int64)}
    nodes = [helper.make_node("Gather", ["latent_input", "ix"], ["x"], axis=1),
             helper.make_node("Gather", ["latent_input", "iy"], ["y"], axis=1),
             helper.make_node("Mul", ["x", "y"], ["xy"]),
             helper.make_node("Mul", ["x", "dx"], ["vx"]),
             helper.make_node("Mul", ["y", "dy"], ["vy"]),
             helper.make_node("Mul", ["xy", "dxy"], ["vxy"]),
             helper.make_node("Add", ["base", "vx"], ["a"]),
             helper.make_node("Add", ["a", "vy"], ["b"]),
             helper.make_node("Add", ["b", "vxy"], ["wavetable_output"])]
    graph = helper.make_graph(nodes, "ForgeReferenceMorph",
        [helper.make_tensor_value_info("latent_input", TensorProto.FLOAT, [1, 2])],
        [helper.make_tensor_value_info("wavetable_output", TensorProto.FLOAT, [1, TABLE_SIZE])],
        initializer=[numpy_helper.from_array(value, name) for name, value in constants.items()])
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)], producer_name="FORGE reference exporter", ir_version=9)
    onnx.checker.check_model(model)
    target = Path(model_path)
    target.parent.mkdir(parents=True, exist_ok=True)
    onnx.save(model, target)
    return target


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="forge_synth/models/forge_reference.onnx")
    args = parser.parse_args()
    print(f"Exported reference morph (not a trained model): {export_to_onnx(args.output)}")
