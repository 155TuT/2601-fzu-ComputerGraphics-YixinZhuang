"""Compose the supplied F/Z/U meshes with a Cornell box and area light."""
from copy import deepcopy
from collections import Counter
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

TASK = Path(__file__).resolve().parents[1]
NS = "http://www.collada.org/2005/11/COLLADASchema"
ET.register_namespace("", NS)
def tag(name):
    return f"{{{NS}}}{name}"


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def subtract(a, b):
    return tuple(x-y for x, y in zip(a, b))


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def support_hard_edges(points, faces, fraction=0.002):
    # Keep a closed shared topology: the loader derives normals from this mesh.
    # Each original triangle has a planar inset triangle and a narrow support ring.
    # Shared edge midpoints retain all twin edges; the original surface is unchanged.
    output = list(points)
    midpoint = {}
    refined = []
    planar_vertices = []
    max_width = 0.0
    for face in faces:
        pa, pb, pc = [points[v] for v in face]
        geometric = cross(subtract(pb, pa), subtract(pc, pa))
        length = math.sqrt(dot(geometric, geometric))
        assert length > 1e-14, "The source letter contains a degenerate triangle"
        direction = tuple(n/length for n in geometric)
        inset = []
        for i, a in enumerate(face):
            b, c = face[(i+1) % 3], face[(i+2) % 3]
            point = tuple((1-2*fraction)*x + fraction*(y+z)
                          for x, y, z in zip(points[a], points[b], points[c]))
            inset.append(len(output))
            planar_vertices.append((len(output), direction))
            max_width = max(max_width, math.dist(point, points[a]))
            output.append(point)
        refined.append(tuple(inset))
        for i, a in enumerate(face):
            b = face[(i+1) % 3]
            key = tuple(sorted((a, b)))
            if key not in midpoint:
                midpoint[key] = len(output)
                output.append(tuple((x+y)/2 for x, y in zip(points[a], points[b])))
            m, ia, ib = midpoint[key], inset[i], inset[(i+1) % 3]
            refined.extend(((a, m, ia), (m, ib, ia), (m, b, ib)))

    # Match the framework's area-weighted normal calculation for closed meshes.
    directed = Counter()
    accum = [[0.0, 0.0, 0.0] for _ in output]
    volume6 = 0.0
    normals = []
    for a, b, c in refined:
        pa, pb, pc = output[a], output[b], output[c]
        vector = cross(subtract(pb, pa), subtract(pc, pa))
        length = math.sqrt(dot(vector, vector))
        assert length > 1e-15, "A support triangle is degenerate"
        normals.append(tuple(n/length for n in vector))
        volume6 += dot(pa, cross(pb, pc))
        for x, y in ((a, b), (b, c), (c, a)):
            directed[x, y] += 1
            accum[x] = [old+value for old, value in zip(accum[x], vector)]
    assert all(n == 1 and directed[b, a] == 1 for (a, b), n in directed.items()), "Letter surface must remain closed and consistently wound"
    assert volume6 > 0, "Letter triangle winding must point outwards"
    lengths = [math.sqrt(dot(n, n)) for n in accum]
    assert min(lengths) > 1e-14, "Every vertex needs a finite nonzero normal"
    error = max(abs(1-dot(tuple(n/lengths[v] for n in accum[v]), expected))
                for v, expected in planar_vertices)
    assert error < 1e-8, "Inset vertices must retain planar face normals"
    audit = {"vertices": len(output), "edges": len(directed)//2, "triangles": len(refined),
             "boundary_edges": 0, "signed_volume": volume6/6,
             "normal_min_length": min(lengths), "planar_normal_max_dot_error": error,
             "support_fraction": fraction, "support_max_vertex_displacement": max_width}
    return output, refined, normals, audit

tree = ET.parse(TASK / "code/dae/sky/CBbunny.dae")
root = tree.getroot()
root.find(f".//{tag('perspective')}/{tag('xfov')}").text = "35"
root.find(f".//{tag('perspective')}/{tag('aspect_ratio')}").text = "1.3333333333"
geometries = root.find(tag("library_geometries"))
visual = root.find(f"{tag('library_visual_scenes')}/{tag('visual_scene')}")
effects = root.find(tag("library_effects"))
materials = root.find(tag("library_materials"))
for item in list(geometries):
    if item.get("id") == "Mesh-mesh":
        geometries.remove(item)
for item in list(visual):
    if item.get("id") == "Mesh":
        visual.remove(item)

letters = []
for letter in "FZU":
    source = ET.parse(TASK / f"code/dae/fzu/{letter}.dae").getroot()
    geometry = deepcopy(source.find(f"{tag('library_geometries')}/{tag('geometry')}"))
    position = geometry.find(f".//{tag('source')}[@name='position']/{tag('float_array')}")
    values = list(map(float, position.text.split()))
    points = [values[i:i + 3] for i in range(0, len(values), 3)]
    lower = [min(p[i] for p in points) for i in range(3)]
    upper = [max(p[i] for p in points) for i in range(3)]
    scale = 0.65 / (upper[1] - lower[1])
    letters.append((letter, geometry, points, lower, upper, scale))

gap = 0.12
total = sum((item[4][0] - item[3][0]) * item[5] for item in letters) + 2 * gap
cursor = -total / 2
record = []
colors = {"F": "0.72 0.34 0.10 1", "Z": "0.13 0.53 0.65 1", "U": "0.70 0.70 0.64 1"}
for letter, geometry, points, lower, upper, scale in letters:
    width = (upper[0] - lower[0]) * scale
    ids = {e.get("id"): f"{letter}-{e.get('id')}" for e in geometry.iter() if e.get("id")}
    for element in geometry.iter():
        for key, value in list(element.attrib.items()):
            if key == "id":
                element.set(key, ids[value])
            elif value.startswith("#") and value[1:] in ids:
                element.set(key, "#" + ids[value[1:]])
    # Bake a proper rotation from the letter's Y-up into the box's Z-up.
    transformed = []
    for x, y, z in points:
        transformed += [cursor + (x - lower[0]) * scale,
                        -0.25 - (z - (lower[2] + upper[2]) / 2) * scale,
                        0.025 + (y - lower[1]) * scale]
    position = geometry.find(f".//{tag('source')}[@name='position']/{tag('float_array')}")
    position.text = " ".join(f"{value:.8g}" for value in transformed)
    normal = geometry.find(f".//{tag('source')}[@name='normal']/{tag('float_array')}")
    values = list(map(float, normal.text.split()))
    normals = []
    for i in range(0, len(values), 3):
        x, y, z = values[i:i + 3]
        normals += [x, -z, y]
    normal.text = " ".join(f"{value:.8g}" for value in normals)
    triangles = geometry.find(f".//{tag('triangles')}")
    triangles.set("material", f"{letter}-material")
    triangle_count = int(triangles.get("count"))
    index_data = list(map(int, triangles.find(tag("p")).text.split()))
    stride = max(int(e.get("offset", "0")) for e in triangles.findall(tag("input"))) + 1
    vertex_offset = next(int(e.get("offset", "0")) for e in triangles.findall(tag("input"))
                         if e.get("semantic") == "VERTEX")
    original_faces = [tuple(index_data[(3*f+c)*stride+vertex_offset] for c in range(3))
                      for f in range(triangle_count)]
    mesh_points = [tuple(transformed[i:i+3]) for i in range(0, len(transformed), 3)]
    refined_points, refined_faces, face_normals, audit = support_hard_edges(mesh_points, original_faces)
    position.text = " ".join(f"{value:.12g}" for point in refined_points for value in point)
    position.set("count", str(3*len(refined_points)))
    position_source = geometry.find(f".//{tag('source')}[@name='position']")
    position_source.find(f"{tag('technique_common')}/{tag('accessor')}").set("count", str(len(refined_points)))
    normal.text = " ".join(f"{value:.12g}" for direction in face_normals for value in direction)
    normal.set("count", str(3*len(face_normals)))
    normal_source = geometry.find(f".//{tag('source')}[@name='normal']")
    normal_source.find(f"{tag('technique_common')}/{tag('accessor')}").set("count", str(len(face_normals)))
    for input_element in triangles.findall(tag("input")):
        input_element.set("offset", "0" if input_element.get("semantic") == "VERTEX" else "1")
    triangles.find(tag("p")).text = " ".join(str(index) for f, face in enumerate(refined_faces) for vertex in face for index in (vertex, f))
    triangles.set("count", str(len(refined_faces)))
    # The supplied loader reads COLLADA polylist.
    triangles.tag = tag("polylist")
    vcount = ET.Element(tag("vcount"))
    vcount.text = " ".join(["3"] * len(refined_faces))
    triangles.insert(len(triangles) - 1, vcount)
    geometries.append(geometry)

    effect = deepcopy(effects.find(f"{tag('effect')}[@id='Default-effect']"))
    effect.set("id", f"{letter}-effect")
    effect.find(f".//{tag('diffuse')}/{tag('color')}").text = colors[letter]
    effects.append(effect)
    material = ET.SubElement(materials, tag("material"), id=f"{letter}-material", name=letter)
    ET.SubElement(material, tag("instance_effect"), url=f"#{letter}-effect")
    node = ET.SubElement(visual, tag("node"), id=f"FZU-{letter}", name=letter, type="NODE")
    ET.SubElement(node, tag("matrix"), sid="transform").text = "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"
    instance = ET.SubElement(node, tag("instance_geometry"), url="#" + geometry.get("id"))
    common = ET.SubElement(ET.SubElement(instance, tag("bind_material")), tag("technique_common"))
    ET.SubElement(common, tag("instance_material"), symbol=f"{letter}-material", target=f"#{letter}-material")
    record.append({"letter": letter, "source_vertices": len(points), "source_triangles": triangle_count,
                   "exported_mesh": audit,
                   "height": 0.65, "width": width, "baseline": 0.025, "diffuse": colors[letter]})
    cursor += width + gap

root.find(f"{tag('asset')}/{tag('contributor')}/{tag('author')}").text = "FZU coursework scene"
root.find(f"{tag('asset')}/{tag('contributor')}/{tag('authoring_tool')}").text = "create_fzu_scene.py"
output = TASK / "code/dae/fzu/FZU-gallery.dae"
ET.indent(tree, space="  ")
tree.write(output, encoding="utf-8", xml_declaration=True)
(TASK / "verification").mkdir(parents=True, exist_ok=True)
(TASK / "verification/fzu-scene.json").write_text(json.dumps({
    "source": ["code/dae/fzu/F.dae", "code/dae/fzu/Z.dae", "code/dae/fzu/U.dae"],
    "container_scene": "code/dae/sky/CBbunny.dae", "output": "code/dae/fzu/FZU-gallery.dae",
    "letters": record, "area_light_radiance": [10, 10, 10]}, indent=2), encoding="utf-8")
print(output)
