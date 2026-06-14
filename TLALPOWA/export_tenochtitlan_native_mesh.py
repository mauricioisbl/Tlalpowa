import bpy
import json
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
GRID_X = 1024
GRID_Y = 1024
# Dos anillos estrictos: lod 0 es lejano deliberadamente burdo; lod 1 conserva la malla completa para el anillo cercano.
LOD_RATIOS = (0.001, 1.0)
MAX_OPEN_HANDLES = 16
DEFAULT_MEMORY_BUDGET_MB = 75


def arguments():
    args = sys.argv
    if "--" not in args:
        raise SystemExit(
            "usage: -- output center_lon center_lat meters_per_unit "
            "rotation_deg vertical_scale [memory_budget_mb] [progress_json]"
        )
    extra = args[args.index("--") + 1 :]
    if len(extra) not in (6, 7, 8):
        raise SystemExit("invalid arguments")
    return (
        Path(extra[0]),
        float(extra[1]),
        float(extra[2]),
        float(extra[3]),
        float(extra[4]),
        float(extra[5]),
        int(extra[6]) if len(extra) >= 7 else DEFAULT_MEMORY_BUDGET_MB,
        Path(extra[7]) if len(extra) == 8 else None,
    )


def wgs84_lon_meters_per_degree(lat_deg):
    lat = math.radians(lat_deg)
    return max(1.0e-9, 111412.84 * math.cos(lat) - 93.5 * math.cos(3.0 * lat) + 0.118 * math.cos(5.0 * lat))


def wgs84_lat_meters_per_degree(lat_deg):
    lat = math.radians(lat_deg)
    return max(1.0e-9, 111132.954 - 559.822 * math.cos(2.0 * lat) + 1.175 * math.cos(4.0 * lat) - 0.0023 * math.cos(6.0 * lat))


class ProgressWriter:
    def __init__(self, path):
        self.path = path
        self.total = 1
        self.done = 0
        if self.path:
            self.path.parent.mkdir(parents=True, exist_ok=True)

    def set_total(self, total):
        self.total = max(1, int(total))
        self.done = min(self.done, self.total)

    def pulse(self, phase, done=None, total=None, bytes_written=0):
        if total is not None:
            self.set_total(total)
        if done is not None:
            self.done = max(0, min(int(done), self.total))
        if not self.path:
            return
        progress = max(0.0, min(0.985, float(self.done) / float(max(1, self.total))))
        payload = {
            "phase": str(phase),
            "done": self.done,
            "total": self.total,
            "progress": progress,
            "bytes": int(max(0, bytes_written)),
        }
        tmp = self.path.with_suffix(self.path.suffix + ".tmp")
        tmp.write_text(json_dumps(payload), encoding="utf-8")
        tmp.replace(self.path)


def json_dumps(payload):
    return json.dumps(payload, ensure_ascii=False, separators=(",", ":"))


def city_object(obj):
    if obj.type != "MESH":
        return False
    name = obj.name.lower()
    return name.endswith("_high") or name.endswith("_hi") or name.endswith("_full") or name.endswith("_mid")


def exportable_mesh_objects():
    high = []
    mid = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        name = obj.name.lower()
        if name.endswith("_high") or name.endswith("_hi") or name.endswith("_full"):
            high.append(obj)
        elif name.endswith("_mid"):
            mid.append(obj)
    if high:
        return high
    if mid:
        return mid
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


def image_for_material(material, token):
    if not material or not material.use_nodes or not material.node_tree:
        return None
    for node in material.node_tree.nodes:
        image = node.image if node.type == "TEX_IMAGE" and getattr(node, "image", None) else None
        identity = f"{node.name} {node.label} {image.name if image else ''}".lower()
        if image and token in identity:
            return image
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
    lat = center_lat + north / wgs84_lat_meters_per_degree(center_lat)
    lon = center_lon + east / wgs84_lon_meters_per_degree(center_lat)
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


def center_out_key_xy(tx, ty, center_tx, center_ty):
    dx = int(tx) - int(center_tx)
    dy = int(ty) - int(center_ty)
    return (dx * dx + dy * dy, abs(dx) + abs(dy), int(ty), int(tx))


def center_out_key_lod(key, center_tx, center_ty):
    tx, ty, lod = key
    # Dentro de una misma tesela se conserva primero el LOD cercano completo
    # para que el primer detalle visible sea el hiperrealista; el LOD externo
    # queda como respaldo liviano.
    return center_out_key_xy(tx, ty, center_tx, center_ty) + (-int(lod),)


def evaluated_lod_object(source, ratio, lod):
    if ratio >= 0.999:
        source.data.calc_loop_triangles()
        return source, False
    obj = source.copy()
    obj.data = source.data.copy()
    bpy.context.collection.objects.link(obj)
    for selected in tuple(bpy.context.selected_objects):
        selected.select_set(False)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    modifier = obj.modifiers.new(f"Tlalpowa_LOD_{lod}", "DECIMATE")
    modifier.decimate_type = "COLLAPSE"
    modifier.ratio = ratio
    if hasattr(modifier, "use_collapse_triangulate"):
        modifier.use_collapse_triangulate = True
    if hasattr(modifier, "use_symmetry"):
        modifier.use_symmetry = False
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.data.calc_loop_triangles()
    return obj, True


def write_mesh(path, center_lon, center_lat, meters_per_unit, rotation_deg, vertical_scale, budget_mb, progress_path=None):
    budget_mb = max(1, min(int(budget_mb), DEFAULT_MEMORY_BUDGET_MB))
    progress = ProgressWriter(progress_path)
    objects = sorted(exportable_mesh_objects(), key=lambda obj: obj.name)
    if not objects:
        raise RuntimeError("no Blender mesh geometry exported")
    lo, hi = model_bounds(objects)
    center = (lo + hi) * 0.5
    center_tx, center_ty = tile_for_point(center, lo, hi)
    rotation = math.radians(rotation_deg)
    lon_min, lat_min, lon_max, lat_max = geo_bounds_for_model_rect(
        lo.x, lo.y, hi.x, hi.y, center, center_lon, center_lat, meters_per_unit, rotation
    )
    height_min = lo.z * meters_per_unit * vertical_scale
    height_max = max(height_min + 0.01, hi.z * meters_per_unit * vertical_scale)

    total_mesh_steps = max(16, len(objects) * len(LOD_RATIOS) + 96)
    progress.set_total(total_mesh_steps)
    progress.pulse("IXIPTLAH3D: preparando geometria Blender", 0, total_mesh_steps)

    temp_root = Path(tempfile.mkdtemp(prefix="tlalpowa3d_"))
    handles = {}
    handle_ticks = {}
    handle_tick = 0
    triangle_counts = {}
    source_triangles = 0
    packed_triangles = 0
    packed_bytes = 0

    def part_path(tx, ty, lod):
        return temp_root / f"{ty:02d}_{tx:02d}_{lod}.bin"

    def handle_for(tx, ty, lod):
        nonlocal handle_tick
        key = (tx, ty, lod)
        handle_tick += 1
        handle_ticks[key] = handle_tick
        if key in handles:
            return handles[key]
        if len(handles) >= MAX_OPEN_HANDLES:
            stale = min(handles.keys(), key=lambda k: handle_ticks.get(k, 0))
            old_handle = handles.pop(stale, None)
            if old_handle is not None:
                old_handle.close()
            handle_ticks.pop(stale, None)
        handles[key] = part_path(tx, ty, lod).open("ab", buffering=131072)
        return handles[key]

    try:
        step = 0
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
                    packed_triangles += 1
                    packed_bytes += len(packed)
                    if (packed_triangles & 0x3fff) == 0:
                        progress.pulse(f"IXIPTLAH3D: triangulando geometria nativa LOD {lod}", step, total_mesh_steps, packed_bytes)
                if lod == len(LOD_RATIOS) - 1:
                    source_triangles += len(mesh.loop_triangles)
                step += 1
                progress.pulse(f"IXIPTLAH3D: anillos fisicos 0-15m LOD {lod} · {source.name}", step, total_mesh_steps)
                print(f"{source.name} LOD {lod}: {len(mesh.loop_triangles)} triangles", flush=True)
                del material_samplers
                if temporary:
                    bpy.data.objects.remove(obj, do_unlink=True)

        for handle in handles.values():
            handle.close()
        handles.clear()

        entries = []
        occupied_keys = sorted(triangle_counts.keys(), key=lambda key: center_out_key_lod(key, center_tx, center_ty))
        total_mesh_steps = max(total_mesh_steps, step + len(occupied_keys) + 96)
        progress.set_total(total_mesh_steps)
        progress.pulse("IXIPTLAH3D: construyendo indice espacial de teselas ocupadas", step, total_mesh_steps)
        for key_index, (tx, ty, lod) in enumerate(occupied_keys, 1):
            triangles = triangle_counts.get((tx, ty, lod), 0)
            part = part_path(tx, ty, lod)
            if triangles and part.exists():
                x0 = lo.x + (hi.x - lo.x) * tx / GRID_X
                x1 = lo.x + (hi.x - lo.x) * (tx + 1) / GRID_X
                y0 = lo.y + (hi.y - lo.y) * ty / GRID_Y
                y1 = lo.y + (hi.y - lo.y) * (ty + 1) / GRID_Y
                tile_geo = geo_bounds_for_model_rect(
                    x0, y0, x1, y1, center, center_lon, center_lat, meters_per_unit, rotation
                )
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
            if (key_index & 4095) == 0 or key_index == len(occupied_keys):
                progress.pulse(
                    f"IXIPTLAH3D: indexando teselas ocupadas {key_index}/{len(occupied_keys)}",
                    min(total_mesh_steps - 1, step + key_index),
                    total_mesh_steps,
                    packed_bytes,
                )
        step += len(occupied_keys)

        def strip_partial_name(name):
            return name[:-8] if name.lower().endswith(".partial") else name

        final_name = strip_partial_name(path.name)
        final_stem = Path(final_name).stem or "modelo"
        # Contrato de almacenamiento: una sola carpeta de modelo dentro de Datos\,
        # sin subcarpetas internas. El archivo raiz es la tesela central y todas
        # las teselas hermanas viven junto a el en el mismo directorio.
        tile_dir = path.parent
        tile_dir.mkdir(parents=True, exist_ok=True)
        path.parent.mkdir(parents=True, exist_ok=True)
        stale_prefix = f"{final_stem}__ixiptlah3d_tile_"
        for stale in tuple(tile_dir.glob(f"{final_stem}__ixiptlah3d_tile_*.ixiptlah3d")):
            try:
                stale.unlink()
            except OSError:
                pass

        groups = {}
        for entry in entries:
            groups.setdefault((entry["tx"], entry["ty"]), []).append(entry)
        full_lod = len(LOD_RATIOS) - 1
        groups = {
            key: value for key, value in groups.items()
            if any(entry["lod"] == full_lod and entry["triangles"] > 0 for entry in value)
        }
        if not groups:
            raise RuntimeError("IXIPTLAH3D sin teselas utiles de fidelidad cercana")
        total_mesh_steps = max(total_mesh_steps, step + len(groups) + 4)
        progress.set_total(total_mesh_steps)
        center_key = min(groups.keys(), key=lambda key: center_out_key_xy(key[0], key[1], center_tx, center_ty))
        budget_bytes = int(budget_mb) * 1024 * 1024
        written_files = []
        total_container_bytes = 0

        def tile_filename(tx, ty):
            return f"{final_stem}__ixiptlah3d_tile_y{ty:04d}_x{tx:04d}.ixiptlah3d"

        def write_container(container_path, container_entries):
            nonlocal total_container_bytes
            container_entries = sorted(container_entries, key=lambda e: (-int(e["lod"]), int(e["ty"]), int(e["tx"])))
            if not container_entries:
                raise RuntimeError(f"tesela IXIPTLAH3D vacia: {container_path.name}")
            data_offset = HEADER_SIZE + len(container_entries) * INDEX_STRIDE
            cursor = data_offset + sum(int(entry["bytes"]) for entry in container_entries)
            if cursor > budget_bytes and any(entry["lod"] == 0 for entry in container_entries):
                # El LOD cercano completo nunca se toca; si el contenedor rebasa por muy poco,
                # se descarta sólo el LOD externo ultraligero de esa tesela para conservar
                # fidelidad cercana y mantener el contrato <75 MB por archivo.
                container_entries = [entry for entry in container_entries if entry["lod"] != 0]
                if not container_entries:
                    raise RuntimeError(f"tesela IXIPTLAH3D sin LOD cercano tras recorte: {container_path.name}")
                data_offset = HEADER_SIZE + len(container_entries) * INDEX_STRIDE
                cursor = data_offset + sum(int(entry["bytes"]) for entry in container_entries)
            cursor = data_offset
            for entry in container_entries:
                entry["offset"] = cursor
                cursor += entry["bytes"]
            if cursor > budget_bytes:
                raise RuntimeError(
                    f"tesela IXIPTLAH3D rebasa limite aun con reticula 1024: {container_path.name} {cursor / 1048576.0:.1f} MB > {budget_mb} MB"
                )
            tmp_container = container_path.with_suffix(container_path.suffix + ".writing")
            try:
                if tmp_container.exists():
                    tmp_container.unlink()
                with tmp_container.open("wb") as out:
                    out.write(b"TLP3D003")
                    out.write(struct.pack("<II", len(container_entries), source_triangles))
                    out.write(struct.pack("<ffffff", lon_min, lat_min, lon_max, lat_max, height_min, height_max))
                    out.write(struct.pack("<IIIIII", VERTEX_STRIDE, 7, len(LOD_RATIOS), GRID_X, GRID_Y, budget_mb))
                    out.write(struct.pack("<QQ", HEADER_SIZE, data_offset))
                    for entry in container_entries:
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
                    for entry in container_entries:
                        with entry["path"].open("rb") as source:
                            shutil.copyfileobj(source, out, length=4 * 1024 * 1024)
                    out.flush()
                    os.fsync(out.fileno())
                size = tmp_container.stat().st_size
                if size != cursor:
                    raise RuntimeError(f"tesela IXIPTLAH3D con tamano inesperado: {container_path.name}")
                if size > budget_bytes:
                    raise RuntimeError(f"tesela IXIPTLAH3D fuera de presupuesto: {container_path.name}")
                tmp_container.replace(container_path)
            finally:
                if tmp_container.exists():
                    try:
                        tmp_container.unlink()
                    except OSError:
                        pass
            total_container_bytes += size
            written_files.append(container_path)
            return size

        progress.pulse("IXIPTLAH3D: escribiendo teselas nativas <75MB del centro hacia afuera", min(step, total_mesh_steps - 1), total_mesh_steps, packed_bytes)
        root_size = write_container(path, groups[center_key])
        sibling_files = []
        done_tiles = 1
        for key in sorted(groups.keys(), key=lambda k: center_out_key_xy(k[0], k[1], center_tx, center_ty)):
            if key == center_key:
                continue
            tx, ty = key
            tile_path = tile_dir / tile_filename(tx, ty)
            size = write_container(tile_path, groups[key])
            sibling_files.append(tile_path)
            done_tiles += 1
            if (done_tiles & 31) == 0 or done_tiles == len(groups):
                progress.pulse(
                    f"IXIPTLAH3D: escribiendo tesela centro-afuera {done_tiles}/{len(groups)}",
                    min(total_mesh_steps - 1, step + done_tiles),
                    total_mesh_steps,
                    total_container_bytes,
                )
        sidecar = path.with_suffix(path.suffix + ".tiles.json")
        manifest = {
            "format": "TLP3D_TILESET_V1",
            "tile_contract": "one_ixiptlah3d_file_per_spatial_tile",
            "root_tile": final_name,
            "tile_directory": ".",
            "tile_files": [f.name for f in sibling_files],
            "tile_sequence": [final_name] + [f.name for f in sibling_files],
            "storage_layout": "single_model_folder_flat_tiles",
            "tile_count": len(written_files),
            "max_file_mb": budget_mb,
            "lod_rings": ["outer_ultralight", "near_full_fidelity"],
            "near_ring_m": 15.0,
            "stream_order": "center_out",
            "center_tile_x": center_tx,
            "center_tile_y": center_ty,
            "grid_x": GRID_X,
            "grid_y": GRID_Y,
            "outer_lod_ratio": LOD_RATIOS[0],
            "near_lod_ratio": LOD_RATIOS[-1],
            "source_triangles": source_triangles,
            "packed_triangles": packed_triangles,
            "total_bytes": total_container_bytes,
        }
        expected_sequence = [final_name] + [f.name for f in sibling_files]
        if manifest["tile_sequence"] != expected_sequence or len(written_files) != len(expected_sequence):
            raise RuntimeError("IXIPTLAH3D secuencia centro-afuera inconsistente antes de cerrar manifiesto")
        for written in written_files:
            if written.parent != tile_dir or not written.exists() or written.stat().st_size > budget_bytes:
                raise RuntimeError(f"IXIPTLAH3D tesela invalida al cerrar: {written.name}")
        tmp_sidecar = sidecar.with_suffix(sidecar.suffix + ".tmp")
        tmp_sidecar.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        tmp_sidecar.replace(sidecar)
        progress.pulse("IXIPTLAH3D: teselas listas", total_mesh_steps, total_mesh_steps, total_container_bytes)
        print(
            f"wrote tileset {path}: {len(written_files)} ixiptlah3d tile files, "
            f"root {root_size / 1048576.0:.1f} MB, total {total_container_bytes / 1048576.0:.1f} MB",
            flush=True,
        )
    finally:
        for handle in handles.values():
            handle.close()
        shutil.rmtree(temp_root, ignore_errors=True)


def main():
    parsed = arguments()
    progress_path = parsed[7]
    try:
        write_mesh(*parsed)
    except Exception as exc:
        if progress_path:
            try:
                progress_path.parent.mkdir(parents=True, exist_ok=True)
                tmp = progress_path.with_suffix(progress_path.suffix + ".tmp")
                tmp.write_text(
                    json_dumps(
                        {
                            "phase": "IXIPTLAH3D ERROR: " + str(exc),
                            "done": 1,
                            "total": 1,
                            "progress": 0.985,
                            "bytes": 0,
                            "error": str(exc),
                        }
                    ),
                    encoding="utf-8",
                )
                tmp.replace(progress_path)
            except Exception:
                pass
        print("IXIPTLAH3D ERROR:", exc, file=sys.stderr, flush=True)
        raise


main()
