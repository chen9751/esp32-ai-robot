#!/usr/bin/env python3
"""Inspect TFLite FlatBuffer operators and streaming variable tensors."""
from pathlib import Path
import hashlib
import tflite

p = Path(__file__).resolve().parents[1] / "models/wakeword/hello_robot_v1/hello_robot_v1.tflite"
data = p.read_bytes()
model = tflite.Model.GetRootAsModel(data, 0)
print("SHA256:", hashlib.sha256(data).hexdigest())
print("Schema:", model.Version())
for i in range(model.OperatorCodesLength()):
    op = model.OperatorCodes(i)
    builtin = op.BuiltinCode()
    name = tflite.BuiltinOperator.Name(builtin) if hasattr(tflite.BuiltinOperator, "Name") else str(builtin)
    print(f"OPCODE {i}: {name} builtin={builtin} version={op.Version()} custom={op.CustomCode()}")
for s in range(model.SubgraphsLength()):
    graph = model.Subgraphs(s)
    print(f"SUBGRAPH {s}: operations={graph.OperatorsLength()}, tensors={graph.TensorsLength()}")
    for i in range(graph.TensorsLength()):
        tensor = graph.Tensors(i)
        if tensor.IsVariable():
            print(f"VARIABLE {i}: name={tensor.Name()} shape={tensor.ShapeAsNumpy().tolist()} type={tensor.Type()}")
    for i in range(graph.InputsLength()):
        t = graph.Tensors(graph.Inputs(i))
        print(f"INPUT {i}: {t.Name()} shape={t.ShapeAsNumpy().tolist()} type={t.Type()}")
    for i in range(graph.OutputsLength()):
        t = graph.Tensors(graph.Outputs(i))
        print(f"OUTPUT {i}: {t.Name()} shape={t.ShapeAsNumpy().tolist()} type={t.Type()}")
