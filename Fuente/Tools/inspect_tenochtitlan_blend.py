import bpy
import json
import sys
from pathlib import Path


def output_path():
    args = sys.argv
    if "--" not in args:
        raise SystemExit("missing output path")
    extra = args[args.index("--") + 1 :]
    if not extra:
        raise SystemExit("missing output path")
    return Path(extra[0])


mesh_objects = 0
vertices = 0
polygons = 0
materials = set()
images = []
object_rows = []
bounds_min = [float("inf")] * 3
bounds_max = [float("-inf")] * 3

for obj in bpy.context.scene.objects:
    if obj.type == "MESH" and obj.data:
        mesh_objects += 1
        vertices += len(obj.data.vertices)
        polygons += len(obj.data.polygons)
        for slot in obj.material_slots:
            if slot.material:
                materials.add(slot.material.name)
        object_lo = [float("inf")] * 3
        object_hi = [float("-inf")] * 3
        for corner in obj.bound_box:
            point = obj.matrix_world @ __import__("mathutils").Vector(corner)
            for axis in range(3):
                object_lo[axis] = min(object_lo[axis], point[axis])
                object_hi[axis] = max(object_hi[axis], point[axis])
        object_rows.append(
            {
                "name": obj.name,
                "vertices": len(obj.data.vertices),
                "polygons": len(obj.data.polygons),
                "bounds_min": object_lo,
                "bounds_max": object_hi,
            }
        )
    if hasattr(obj, "bound_box"):
        for corner in obj.bound_box:
            world = obj.matrix_world @ __import__("mathutils").Vector(corner)
            for axis in range(3):
                bounds_min[axis] = min(bounds_min[axis], world[axis])
                bounds_max[axis] = max(bounds_max[axis], world[axis])

for image in bpy.data.images:
    images.append(
        {
            "name": image.name,
            "width": int(image.size[0]),
            "height": int(image.size[1]),
            "packed": bool(image.packed_file),
            "filepath": image.filepath,
        }
    )

result = {
    "objects": len(bpy.context.scene.objects),
    "mesh_objects": mesh_objects,
    "vertices": vertices,
    "polygons": polygons,
    "materials": len(materials),
    "images": images,
    "bounds_min": bounds_min,
    "bounds_max": bounds_max,
    "render_engine": bpy.context.scene.render.engine,
    "mesh_details": object_rows,
}

path = output_path()
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(result, indent=2), encoding="utf-8")
