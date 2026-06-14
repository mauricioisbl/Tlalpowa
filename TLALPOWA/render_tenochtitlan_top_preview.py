import bpy
import json
import math
import sys
from mathutils import Vector
from pathlib import Path


def arguments():
    args = sys.argv
    if "--" not in args:
        raise SystemExit("usage: -- preview.png metadata.json")
    extra = args[args.index("--") + 1 :]
    if len(extra) != 2:
        raise SystemExit("usage: -- preview.png metadata.json")
    return Path(extra[0]), Path(extra[1])


def city_object(obj):
    return obj.type == "MESH" and obj.name.lower().endswith("_mid")


def preview_mesh_objects():
    preferred = [obj for obj in bpy.context.scene.objects if city_object(obj)]
    if preferred:
        return preferred
    fallback = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        if getattr(obj, "hide_get", lambda: False)():
            continue
        if obj.hide_viewport or obj.hide_render:
            continue
        if not obj.data or len(obj.data.vertices) == 0:
            continue
        fallback.append(obj)
    return fallback




def set_compatible_preview_engine(scene):
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "BLENDER_WORKBENCH"):
        try:
            scene.render.engine = engine
            return engine
        except Exception:
            continue
    return scene.render.engine


preview_path, metadata_path = arguments()
objects = preview_mesh_objects()
if not objects:
    raise RuntimeError("no Blender mesh geometry available for preview")

lo = Vector((float("inf"), float("inf"), float("inf")))
hi = Vector((float("-inf"), float("-inf"), float("-inf")))
for obj in objects:
    matrix = obj.matrix_world
    for corner in obj.bound_box:
        point = matrix @ Vector(corner)
        for axis in range(3):
            lo[axis] = min(lo[axis], point[axis])
            hi[axis] = max(hi[axis], point[axis])

for obj in bpy.context.scene.objects:
    obj.hide_render = obj not in objects

center = (lo + hi) * 0.5
width = max(0.001, hi.x - lo.x)
height = max(0.001, hi.y - lo.y)
aspect = width / height

camera_data = bpy.data.cameras.new("Tlalpowa_Top_Camera")
camera = bpy.data.objects.new("Tlalpowa_Top_Camera", camera_data)
bpy.context.scene.collection.objects.link(camera)
camera.location = (center.x, center.y, hi.z + max(width, height) * 1.25)
camera.rotation_mode = "QUATERNION"
camera.rotation_quaternion = Vector((0.0, 0.0, -1.0)).to_track_quat("-Z", "Y")
camera.data.type = "ORTHO"
camera.data.ortho_scale = height
bpy.context.scene.camera = camera

sun_data = bpy.data.lights.new("Tlalpowa_Preview_Sun", "SUN")
sun_data.energy = 3.0
sun_data.angle = math.radians(7.0)
sun = bpy.data.objects.new("Tlalpowa_Preview_Sun", sun_data)
bpy.context.scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(32.0), math.radians(-18.0), math.radians(-28.0))

world = bpy.context.scene.world or bpy.data.worlds.new("Tlalpowa_Preview_World")
bpy.context.scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.16, 0.20, 0.24, 1.0)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.65

scene = bpy.context.scene
set_compatible_preview_engine(scene)
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.film_transparent = True
scene.render.resolution_percentage = 100
if aspect >= 1.0:
    scene.render.resolution_x = 4096
    scene.render.resolution_y = max(1024, int(round(4096 / aspect)))
else:
    scene.render.resolution_y = 4096
    scene.render.resolution_x = max(1024, int(round(4096 * aspect)))
render_aspect = max(1.0e-12, float(scene.render.resolution_x) / float(scene.render.resolution_y))
camera.data.ortho_scale = max(height, width / render_aspect)
preview_model_height = camera.data.ortho_scale
preview_model_width = preview_model_height * render_aspect
scene.render.filepath = str(preview_path)
scene.view_settings.look = "Medium High Contrast"

preview_path.parent.mkdir(parents=True, exist_ok=True)
metadata_path.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.render.render(write_still=True)
metadata_path.write_text(
    json.dumps(
        {
            "model_min": list(lo),
            "model_max": list(hi),
            "model_center": list(center),
            "width": width,
            "height": height,
            "preview_model_width": preview_model_width,
            "preview_model_height": preview_model_height,
            "camera_width": preview_model_width,
            "camera_height": preview_model_height,
            "preview_width": scene.render.resolution_x,
            "preview_height": scene.render.resolution_y,
            "pixel_model_x": preview_model_width / max(1, scene.render.resolution_x),
            "pixel_model_y": preview_model_height / max(1, scene.render.resolution_y),
        },
        indent=2,
    ),
    encoding="utf-8",
)
