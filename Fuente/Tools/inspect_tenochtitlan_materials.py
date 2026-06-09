import bpy
import json
import sys
from pathlib import Path


def output_path():
    args = sys.argv
    if "--" not in args:
        raise SystemExit("missing output path")
    return Path(args[args.index("--") + 1])


rows = []
for obj in bpy.context.scene.objects:
    if obj.type != "MESH" or not obj.name.lower().endswith("_mid"):
        continue
    materials = []
    for slot in obj.material_slots:
        material = slot.material
        images = []
        if material and material.use_nodes and material.node_tree:
            for node in material.node_tree.nodes:
                if node.type == "TEX_IMAGE" and node.image:
                    images.append(
                        {
                            "node": node.name,
                            "image": node.image.name,
                            "size": list(node.image.size),
                            "packed": bool(node.image.packed_file),
                        }
                    )
        materials.append(
            {
                "name": material.name if material else "",
                "diffuse": list(material.diffuse_color) if material else [],
                "images": images,
            }
        )
    rows.append(
        {
            "object": obj.name,
            "vertices": len(obj.data.vertices),
            "polygons": len(obj.data.polygons),
            "loops": len(obj.data.loops),
            "uv_layers": [layer.name for layer in obj.data.uv_layers],
            "materials": materials,
        }
    )

path = output_path()
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(rows, indent=2), encoding="utf-8")
