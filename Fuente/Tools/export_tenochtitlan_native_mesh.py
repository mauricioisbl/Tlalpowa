import bpy
import math
import shutil
import struct
import sys
import tempfile
from array import array
from mathutils import Vector
from pathlib import Path


HEADER_SIZE = 80
INDEX_STRIDE = 56
VERTEX_STRIDE = 12
GRID_X = 8
GRID_Y = 10
LOD_RATIOS = (0.08, 0.32, 1.0)
DEFAULT_MEMORY_BUDGET_MB = 700


def arguments():
    args = sys.argv
    if "--" not in args:
        raise SystemExit(
            "usage: -- output center_lon center_lat meters_per_unit "
            "rotation_deg vertical_scale [memory_budget_mb]"
        )
    extra = args[args.index("--") + 1 :]
    if len(extra) not in (6, 7):
        raise SystemExit("invalid arguments")
    return (
        Path(extra[0]),
        float(extra[1]),
        float(extra[2]),
        float(extra[3]),
        float(extra[4]),
        float(extra[5]),
        int(extra[6]) if len(extra) == 7 else DEFAULT_MEMORY_BUDGET_MB,
    )


def city_object(obj):
    return obj.type == "MESH" and obj.name.lower().endswith("_mid")


def image_for_material(material, token):
    if not material or not material.use_nodes or not material.node_tree:
        return None
    for node in material.node_tree.nodes:
        identity = f"{node.name} {node.label} {node.image.name if node.image else ''}".lower()
        if node.type == "TEX_IMAGE" and node.image and token in identity:
            return node.image
    return None


class ImageSampler:
    def __init__(self, image):
        self.width = int(image.size[0])
        self.height = int(image.size[1])
        self.pixels = None
        if self.width > 0 and self.height > 0:
            self.pixels = array("f", [0.0]) * (self.width * self.height * 4)
            image.pixels.foreach_get(self.pixels)

    def rgb(self, uv):
        if self.pixels is None:
            return 0.55, 0.52, 0.44
        u = uv.x - math.floor(uv.x)
        v = uv.y - math.floor(uv.y)
        x = min(self.width - 1, max(0, int(u * (self.width - 1) + 0.5)))
        y = min(self.height - 1, max(0, int(v * (self.height - 1) + 0.5)))
        i = (y * self.width + x) * 4
        return self.pixels[i], self.pixels[i + 1], self.pixels[i + 2]


def model_bounds(objects):
    lo = Vector((float("inf"), float("inf"), float("inf")))
    hi = Vector((float("-inf"), float("-inf"), float("-inf")))
    for obj in objects:
        for corner in obj.bound_box:
            point = obj.matrix_world @ Vector(corner)
            for axis in range(3):
                lo[axis] = min(lo[axis], point[axis])
                hi[axis] = max(hi[axis], point[axis])
    return lo, hi


def model_to_geo(point, model_center, center_lon, center_lat, meters_per_unit, rotation):
    dx = point.x - model_center.x
    dy = point.y - model_center.y
    east = meters_per_unit * (math.cos(rotation) * dx - math.sin(rotation) * dy)
    north = meters_per_unit * (math.sin(rotation) * dx + math.cos(rotation) * dy)
    lat = center_lat + north / 110574.0
    lon = center_lon + east / (111320.0 * math.cos(math.radians(center_lat)))
    return lon, lat


def geo_bounds_for_model_rect(x0, y0, x1, y1, model_center, center_lon, center_lat, meters_per_unit, rotation):
    points = [
        model_to_geo(Vector((x, y, 0.0)), model_center, center_lon, center_lat, meters_per_unit, rotation)
        for x in (x0, x1)
        for y in (y0, y1)
    ]
    return min(p[0] for p in points), min(p[1] for p in points), max(p[0] for p in points), max(p[1] for p in points)


def srgb_byte(value):
    value = max(0.0, min(1.0, value))
    if value <= 0.0031308:
        value *= 12.92
    else:
        value = 1.055 * pow(value, 1.0 / 2.4) - 0.055
    return max(0, min(255, int(round(value * 255.0))))


def signed_normal_byte(value):
    return max(-127, min(127, int(round(value * 127.0))))


def quantize(value, low, high):
    if high <= low:
        return 0
    return max(0, min(65535, int(round((value - low) * 65535.0 / (high - low)))))


def tile_for_point(point, lo, hi):
    u = (point.x - lo.x) / max(1.0e-9, hi.x - lo.x)
    v = (point.y - lo.y) / max(1.0e-9, hi.y - lo.y)
    return (
        min(GRID_X - 1, max(0, int(u * GRID_X))),
        min(GRID_Y - 1, max(0, int(v * GRID_Y))),
    )


def evaluated_lod_object(source, ratio, lod):
    if ratio >= 0.999:
        source.data.calc_loop_triangles()
        return source, False
    obj = source.copy()
    obj.data = source.data.copy()
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    modifier = obj.modifiers.new(f"Tlalpowa_LOD_{lod}", "DECIMATE")
    modifier.decimate_type = "COLLAPSE"
    modifier.ratio = ratio
    modifier.use_collapse_triangulate = True
    modifier.use_symmetry = False
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.data.calc_loop_triangles()
    return obj, True


def write_mesh(path, center_lon, center_lat, meters_per_unit, rotation_deg, vertical_scale, budget_mb):
    objects = sorted((obj for obj in bpy.context.scene.objects if city_object(obj)), key=lambda obj: obj.name)
    if not objects:
        raise RuntimeError("no city geometry exported")
    lo, hi = model_bounds(objects)
    center = (lo + hi) * 0.5
    rotation = math.radians(rotation_deg)
    lon_min, lat_min, lon_max, lat_max = geo_bounds_for_model_rect(
        lo.x, lo.y, hi.x, hi.y, center, center_lon, center_lat, meters_per_unit, rotation
    )
    height_min = lo.z * meters_per_unit * vertical_scale
    height_max = max(height_min + 0.01, hi.z * meters_per_unit * vertical_scale)

    temp_root = Path(tempfile.mkdtemp(prefix="tlalpowa3d_"))
    handles = {}
    triangle_counts = {}
    source_triangles = 0

    def handle_for(tx, ty, lod):
        key = (tx, ty, lod)
        if key not in handles:
            handles[key] = (temp_root / f"{ty:02d}_{tx:02d}_{lod}.bin").open("wb", buffering=65536)
        return handles[key]

    try:
        for source in objects:
            for lod, ratio in enumerate(LOD_RATIOS):
                obj, temporary = evaluated_lod_object(source, ratio, lod)
                mesh = obj.data
                uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None
                matrix = obj.matrix_world
                normal_matrix = matrix.to_3x3().inverted().transposed()
                material_samplers = []
                has_normal_map = False
                for slot in obj.material_slots:
                    material = slot.material
                    color_image = image_for_material(material, "_color")
                    ao_image = image_for_material(material, "_ao")
                    normal_image = image_for_material(material, "_normal")
                    material_samplers.append(
                        (
                            ImageSampler(color_image) if color_image else None,
                            ImageSampler(ao_image) if ao_image else None,
                            ImageSampler(normal_image) if normal_image else None,
                            tuple(material.diffuse_color[:3]) if material else (0.55, 0.52, 0.44),
                        )
                    )
                    has_normal_map = has_normal_map or normal_image is not None
                if has_normal_map and uv_layer:
                    mesh.calc_tangents(uvmap=mesh.uv_layers.active.name)

                for tri in mesh.loop_triangles:
                    material_index = min(max(0, tri.material_index), len(material_samplers) - 1)
                    if material_samplers:
                        color, ao, normal_map, diffuse = material_samplers[material_index]
                    else:
                        color, ao, normal_map, diffuse = None, None, None, (0.55, 0.52, 0.44)
                    points = [matrix @ mesh.vertices[mesh.loops[index].vertex_index].co for index in tri.loops]
                    centroid = (points[0] + points[1] + points[2]) / 3.0
                    tx, ty = tile_for_point(centroid, lo, hi)
                    packed = bytearray()
                    for loop_index, point in zip(tri.loops, points):
                        loop = mesh.loops[loop_index]
                        uv = uv_layer[loop_index].uv if uv_layer else Vector((0.0, 0.0))
                        rgb = color.rgb(uv) if color else diffuse
                        if ao:
                            occurrence = max(0.0, min(1.0, ao.rgb(uv)[0]))
                            rgb = tuple(component * (0.58 + 0.42 * occurrence) for component in rgb)
                        normal = normal_matrix @ loop.normal
                        normal.normalize()
                        if normal_map and uv_layer:
                            sampled = normal_map.rgb(uv)
                            tangent = matrix.to_3x3() @ loop.tangent
                            tangent.normalize()
                            bitangent = normal.cross(tangent) * loop.bitangent_sign
                            mapped = (
                                tangent * (sampled[0] * 2.0 - 1.0)
                                + bitangent * (sampled[1] * 2.0 - 1.0)
                                + normal * (sampled[2] * 2.0 - 1.0)
                            )
                            if mapped.length_squared > 1.0e-12:
                                normal = mapped.normalized()
                        lon, lat = model_to_geo(point, center, center_lon, center_lat, meters_per_unit, rotation)
                        height = point.z * meters_per_unit * vertical_scale
                        packed.extend(
                            struct.pack(
                                "<HHHbbbBBB",
                                quantize(lon, lon_min, lon_max),
                                quantize(lat, lat_min, lat_max),
                                quantize(height, height_min, height_max),
                                signed_normal_byte(normal.x),
                                signed_normal_byte(normal.y),
                                signed_normal_byte(normal.z),
                                srgb_byte(rgb[0]),
                                srgb_byte(rgb[1]),
                                srgb_byte(rgb[2]),
                            )
                        )
                    handle_for(tx, ty, lod).write(packed)
                    triangle_counts[(tx, ty, lod)] = triangle_counts.get((tx, ty, lod), 0) + 1
                if lod == len(LOD_RATIOS) - 1:
                    source_triangles += len(mesh.loop_triangles)
                print(f"{source.name} LOD {lod}: {len(mesh.loop_triangles)} triangles")
                del material_samplers
                if temporary:
                    bpy.data.objects.remove(obj, do_unlink=True)

        for handle in handles.values():
            handle.close()
        handles.clear()

        entries = []
        for ty in range(GRID_Y):
            for tx in range(GRID_X):
                x0 = lo.x + (hi.x - lo.x) * tx / GRID_X
                x1 = lo.x + (hi.x - lo.x) * (tx + 1) / GRID_X
                y0 = lo.y + (hi.y - lo.y) * ty / GRID_Y
                y1 = lo.y + (hi.y - lo.y) * (ty + 1) / GRID_Y
                tile_geo = geo_bounds_for_model_rect(
                    x0, y0, x1, y1, center, center_lon, center_lat, meters_per_unit, rotation
                )
                for lod in range(len(LOD_RATIOS)):
                    triangles = triangle_counts.get((tx, ty, lod), 0)
                    part = temp_root / f"{ty:02d}_{tx:02d}_{lod}.bin"
                    if triangles and part.exists():
                        entries.append(
                            {
                                "tx": tx,
                                "ty": ty,
                                "lod": lod,
                                "triangles": triangles,
                                "vertices": triangles * 3,
                                "bytes": part.stat().st_size,
                                "bounds": tile_geo,
                                "path": part,
                            }
                        )

        data_offset = HEADER_SIZE + len(entries) * INDEX_STRIDE
        cursor = data_offset
        for entry in entries:
            entry["offset"] = cursor
            cursor += entry["bytes"]
        if cursor > budget_mb * 1024 * 1024:
            raise RuntimeError(f"container exceeds budget: {cursor / 1048576.0:.1f} MB > {budget_mb} MB")

        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("wb") as out:
            out.write(b"TLP3D003")
            out.write(struct.pack("<II", len(entries), source_triangles))
            out.write(struct.pack("<ffffff", lon_min, lat_min, lon_max, lat_max, height_min, height_max))
            out.write(struct.pack("<IIIIII", VERTEX_STRIDE, 7, len(LOD_RATIOS), GRID_X, GRID_Y, budget_mb))
            out.write(struct.pack("<QQ", HEADER_SIZE, data_offset))
            for entry in entries:
                out.write(
                    struct.pack(
                        "<ffffffIIQIIII",
                        entry["bounds"][0],
                        entry["bounds"][1],
                        entry["bounds"][2],
                        entry["bounds"][3],
                        height_min,
                        height_max,
                        entry["vertices"],
                        entry["triangles"],
                        entry["offset"],
                        entry["bytes"],
                        entry["lod"],
                        entry["tx"],
                        entry["ty"],
                    )
                )
            for entry in entries:
                with entry["path"].open("rb") as source:
                    shutil.copyfileobj(source, out, length=4 * 1024 * 1024)
        print(
            f"wrote {path}: {source_triangles} full-detail triangles, {len(entries)} chunks, "
            f"{cursor / 1048576.0:.1f} MB container"
        )
    finally:
        for handle in handles.values():
            handle.close()
        shutil.rmtree(temp_root, ignore_errors=True)


write_mesh(*arguments())
