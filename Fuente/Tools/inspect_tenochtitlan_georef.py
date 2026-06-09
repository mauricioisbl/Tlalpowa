import bpy
import json
import sys
from mathutils import Vector
from pathlib import Path


def output_path():
    args = sys.argv
    if "--" not in args:
        raise SystemExit("missing output path")
    return Path(args[args.index("--") + 1])


scene = bpy.context.scene
objects = []
for obj in scene.objects:
    props = {}
    for key in obj.keys():
        if key != "_RNA_UI":
            try:
                props[key] = obj[key]
            except Exception:
                props[key] = "<unreadable>"
    objects.append(
        {
            "name": obj.name,
            "type": obj.type,
            "location": list(obj.location),
            "rotation": list(obj.rotation_euler),
            "scale": list(obj.scale),
            "properties": props,
        }
    )

peak = None
peak_object = ""
object_peaks = {}
for obj in scene.objects:
    if obj.type != "MESH" or not obj.name.lower().endswith("_mid"):
        continue
    for vertex in obj.data.vertices:
        point = obj.matrix_world @ vertex.co
        if peak is None or point.z > peak.z:
            peak = point.copy()
            peak_object = obj.name
        current = object_peaks.get(obj.name)
        if current is None or point.z > current[2]:
            object_peaks[obj.name] = list(point)

result = {
    "scene_unit_system": scene.unit_settings.system,
    "scene_unit_scale": scene.unit_settings.scale_length,
    "scene_properties": {key: scene[key] for key in scene.keys() if key != "_RNA_UI"},
    "peak_object": peak_object,
    "peak": list(peak) if peak else None,
    "object_peaks": object_peaks,
    "objects": objects,
}

path = output_path()
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
