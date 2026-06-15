#!/usr/bin/env python3
import json
import struct
import sys
from pathlib import Path

import shapefile
from pyproj import CRS, Transformer


STATE_CODES = {"09", "12", "13", "15", "17", "21"}


def polygon_coordinates(shape, transform):
    points = [transform.transform(x, y) for x, y in shape.points]
    parts = list(shape.parts) + [len(points)]
    rings = []
    for start, end in zip(parts, parts[1:]):
        ring = [[round(lon, 8), round(lat, 8)] for lon, lat in points[start:end]]
        if len(ring) < 3:
            continue
        if ring[0] != ring[-1]:
            ring.append(ring[0])
        rings.append(ring)
    return rings


def write_binary(features, output):
    binary = output.with_suffix(".tlalgeo")
    with binary.open("wb") as stream:
        stream.write(b"TLALGEO1")
        stream.write(struct.pack("<I", len(features)))
        for feature in features:
            props = feature["properties"]
            values = (
                str(feature["id"]),
                str(props["NOMGEO"]),
                str(props["CVE_ENT"]),
                str(props.get("CVEGEO", "")),
            )
            for value in values:
                encoded = value.encode("utf-8")
                if len(encoded) > 65535:
                    raise ValueError("Texto territorial demasiado largo")
                stream.write(struct.pack("<H", len(encoded)))
                stream.write(encoded)
            geometry = feature["geometry"]
            polygons = (
                [geometry["coordinates"]]
                if geometry["type"] == "Polygon"
                else geometry["coordinates"]
            )
            rings = [ring for polygon in polygons for ring in polygon]
            stream.write(struct.pack("<I", len(rings)))
            for ring in rings:
                points = ring[:-1] if len(ring) > 1 and ring[0] == ring[-1] else ring
                stream.write(struct.pack("<I", len(points)))
                for lon, lat in points:
                    stream.write(
                        struct.pack(
                            "<ii",
                            round(float(lon) * 10_000_000),
                            round(float(lat) * 10_000_000),
                        )
                    )
    return binary


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Uso: generar_territorio_inegi.py 00mun.shp salida.geojson")

    source = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]).resolve()
    source_crs = CRS.from_wkt(source.with_suffix(".prj").read_text(encoding="utf-8"))
    transform = Transformer.from_crs(source_crs, "EPSG:4326", always_xy=True)
    reader = shapefile.Reader(str(source), encoding="cp1252")
    state_layer = source.stem.lower() == "00ent"

    features = []
    for item in reader.iterShapeRecords():
        props = item.record.as_dict()
        state = str(props.get("CVE_ENT", "")).zfill(2)
        if state not in STATE_CODES:
            continue
        rings = polygon_coordinates(item.shape, transform)
        if not rings:
            continue
        geometry = {
            "type": "Polygon" if len(rings) == 1 else "MultiPolygon",
            "coordinates": rings if len(rings) == 1 else [[ring] for ring in rings],
        }
        features.append(
            {
                "type": "Feature",
                "id": str(props.get("CVEGEO", "")),
                "properties": {
                    "CVEGEO": str(props.get("CVEGEO", "")),
                    "CVE_ENT": state,
                    "CVE_MUN": "" if state_layer else str(props.get("CVE_MUN", "")).zfill(3),
                    "NOMGEO": str(props.get("NOMGEO", "")).strip(),
                    "fuente": "INEGI Marco Geoestadistico 2025",
                },
                "geometry": geometry,
            }
        )

    features.sort(key=lambda f: f["properties"]["CVEGEO"])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {
                "type": "FeatureCollection",
                "name": "territorio_centro_mexico_inegi_2025",
                "source": "INEGI Marco Geoestadistico 2025, capa 00mun",
                "features": features,
            },
            ensure_ascii=False,
            separators=(",", ":"),
        ),
        encoding="utf-8",
    )
    binary = write_binary(features, output)
    if not state_layer:
        catalog = output.with_name(output.stem + "_catalogo.json")
        catalog.write_text(
            json.dumps(
                [
                    {
                        "CVE_ENT": feature["properties"]["CVE_ENT"],
                        "CVEGEO": feature["properties"]["CVEGEO"],
                        "NOMGEO": feature["properties"]["NOMGEO"],
                    }
                    for feature in features
                ],
                ensure_ascii=False,
                separators=(",", ":"),
            ),
            encoding="utf-8",
        )
    kind = "estados" if state_layer else "municipios/alcaldias"
    print(f"{len(features)} {kind} -> {output} + {binary}")


if __name__ == "__main__":
    main()
