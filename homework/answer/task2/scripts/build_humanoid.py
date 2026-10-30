"""Build a shared-vertex outer surface from editable integer boxes."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
NS = "http://www.collada.org/2005/11/COLLADASchema"


def box_cells(box):
    lo, hi = box["min"], box["max"]
    if any(type(v) is not int for v in lo + hi) or any(a >= b for a, b in zip(lo, hi)):
        raise ValueError("Box limits must be increasing integers")
    return {(x, y, z) for x in range(lo[0], hi[0])
            for y in range(lo[1], hi[1]) for z in range(lo[2], hi[2])}


def components(adjacency):
    unseen = set(adjacency)
    count = 0
    while unseen:
        count += 1
        queue = [unseen.pop()]
        while queue:
            for other in adjacency[queue.pop()]:
                if other in unseen:
                    unseen.remove(other)
                    queue.append(other)
    return count


def build_surface(recipe):
    cells = set()
    for box in recipe["boxes"]:
        cells.update(box_cells(box))
    for box in recipe.get("cutouts", []):
        cells.difference_update(box_cells(box))
    # Every face is counterclockwise when viewed from outside.
    sides = [
        ((-1, 0, 0), ((0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0))),
        ((1, 0, 0), ((1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1))),
        ((0, -1, 0), ((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1))),
        ((0, 1, 0), ((0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0))),
        ((0, 0, -1), ((0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0))),
        ((0, 0, 1), ((0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1))),
    ]
    keys, lookup, triangles, face_normals = [], {}, [], []

    def vertex(key):
        if key not in lookup:
            lookup[key] = len(keys)
            keys.append(key)
        return lookup[key]

    quads = 0
    for cell in sorted(cells):
        for direction, corners in sides:
            neighbor = tuple(a + b for a, b in zip(cell, direction))
            if neighbor in cells:
                continue
            quads += 1
            # Doubled integer coordinates identify corners and quad centres exactly.
            corner_keys = [tuple(2 * (a + b) for a, b in zip(cell, corner)) for corner in corners]
            center = tuple(sum(key[axis] for key in corner_keys) // 4 for axis in range(3))
            ring = [vertex(key) for key in corner_keys]
            middle = vertex(center)
            for i in range(4):
                triangles.append((ring[i], ring[(i + 1) % 4], middle))
                face_normals.append(direction)
    positions = [tuple(x * recipe["unit_scale"] / 2 for x in key) for key in keys]
    return cells, quads, positions, triangles, face_normals


def verify(cells, quads, vertices, triangles):
    directed = Counter()
    neighbors = defaultdict(set)
    links = [defaultdict(set) for _ in vertices]
    volume6 = 0.0
    for a, b, c in triangles:
        assert len({a, b, c}) == 3
        for x, y, z in ((a, b, c), (b, c, a), (c, a, b)):
            directed[x, y] += 1
            neighbors[x].add(y)
            neighbors[y].add(x)
            links[x][y].add(z)
            links[x][z].add(y)
        av, bv, cv = vertices[a], vertices[b], vertices[c]
        volume6 += av[0] * (bv[1]*cv[2]-bv[2]*cv[1]) + av[1] * (bv[2]*cv[0]-bv[0]*cv[2]) + av[2] * (bv[0]*cv[1]-bv[1]*cv[0])
    assert all(n == 1 and directed[b, a] == 1 for (a, b), n in directed.items()), "Every edge must have two opposing incident faces"
    assert components(neighbors) == 1, "The surface must be connected"
    for link in links:
        assert all(len(ring) == 2 for ring in link.values()) and components(link) == 1, "Each vertex link must form one closed ring"
    edge_count = len(directed) // 2
    euler = len(vertices) - edge_count + len(triangles)
    assert euler == 2 and volume6 > 0, "Expected one outward-oriented closed genus-zero surface"
    return {"occupied_voxels": len(cells), "exterior_quads": quads,
            "vertices": len(vertices), "edges": edge_count, "faces": len(triangles),
            "connected_components": 1, "boundary_edges": 0, "euler_characteristic": euler,
            "vertex_links": "single cycles", "signed_volume": volume6 / 6,
            "loop_expected": [{"iterations": i, "vertices": len(triangles)*4**i//2+2,
                               "edges": len(triangles)*4**i*3//2,
                               "faces": len(triangles)*4**i} for i in (1, 2)]}


def save_obj(path, positions, triangles, normals):
    lines = ["# Editable source: docs/models/humanoid.json and scripts/build_humanoid.py", "o WavingHumanoid"]
    lines += ["v " + " ".join(f"{n:.8g}" for n in p) for p in positions]
    unique_normals = list(dict.fromkeys(normals))
    lines += ["vn " + " ".join(str(n) for n in p) for p in unique_normals]
    lines += ["f " + " ".join(f"{v+1}//{unique_normals.index(normal)+1}" for v in tri)
              for tri, normal in zip(triangles, normals)]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def save_dae(path, positions, triangles, normals):
    # Keep the teacher scene's supported camera/material layout; replace the entire mesh.
    ET.register_namespace("", NS)
    tree = ET.parse(ROOT / "code/dae/cube.dae")
    root = tree.getroot()
    tag = lambda name: "{" + NS + "}" + name
    asset = root.find(tag("asset"))
    contributor = asset.find(tag("contributor"))
    contributor.find(tag("author")).text = "Task 2 custom model"
    contributor.find(tag("authoring_tool")).text = "scripts/build_humanoid.py"
    for field in ("created", "modified"):
        asset.find(tag(field)).text = "2026-10-25T00:00:00Z"
    for extra in root.findall(".//" + tag("extra")):
        for parent in root.iter():
            if extra in list(parent):
                parent.remove(extra)
                break
    mesh = root.find(".//" + tag("mesh"))
    for source in mesh.findall(tag("source")):
        data = positions if source.get("id").endswith("positions") else normals
        array = source.find(tag("float_array"))
        array.set("count", str(3 * len(data)))
        array.text = " ".join(f"{n:.8g}" for p in data for n in p)
        source.find(".//" + tag("accessor")).set("count", str(len(data)))
    polylist = mesh.find(tag("polylist"))
    polylist.set("count", str(len(triangles)))
    polylist.find(tag("vcount")).text = " ".join(["3"] * len(triangles))
    polylist.find(tag("p")).text = " ".join(str(i) for face_index, tri in enumerate(triangles) for v in tri for i in (v, face_index))
    for node in root.findall(".//" + tag("node")):
        if node.find(tag("instance_geometry")) is not None:
            node.set("name", "WavingHumanoid")
            node.find(tag("matrix")).text = "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(tree, space="  ")
    tree.write(path, encoding="utf-8", xml_declaration=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--recipe", type=Path, default=ROOT / "docs/models/humanoid.json")
    args = parser.parse_args()
    recipe = json.loads(args.recipe.read_text(encoding="utf-8"))
    cells, quads, positions, triangles, normals = build_surface(recipe)
    result = verify(cells, quads, positions, triangles)
    obj = ROOT / "docs/models/humanoid.obj"
    dae = ROOT / "code/dae/custom/humanoid.dae"
    save_obj(obj, positions, triangles, normals)
    save_dae(dae, positions, triangles, normals)
    result["sha256"] = {str(p.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.recipe, obj, dae)}
    audit = ROOT / "verification/humanoid-topology.json"
    audit.parent.mkdir(parents=True, exist_ok=True)
    audit.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
