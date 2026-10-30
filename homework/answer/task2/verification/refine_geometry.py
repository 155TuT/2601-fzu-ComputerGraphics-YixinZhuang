from pathlib import Path
import xml.etree.ElementTree as E
r=Path('homework/answer/task2')
p=r/'code/src/student_code.cpp';s=p.read_text(encoding='utf-8-sig');s=s.replace('  if (c == d) return e;','''  if (c == d) return e;
  // Reject a diagonal already present across an immediately adjacent triangle.
  // In particular, this keeps tetrahedron edges unchanged in constant time.
  for (auto side : {h1, h2, h4, h5}) {
    auto other = side->twin();
    if (!other->isBoundary()) {
      auto x = other->next();
      for (int k = 0; k < 2; ++k, x = x->next())
        if ((x->vertex() == c && x->twin()->vertex() == d) ||
            (x->vertex() == d && x->twin()->vertex() == c)) return e;
    }
  }''');p.write_text(s,encoding='utf-8')
p=r/'code/src/meshEdit.cpp';s=p.read_text(encoding='utf-8-sig');s=s.replace('else if (operation == "wire") shadingMode = true;', '''else if (operation == "wire") shadingMode = false;
  else if (operation == "solid") shadingMode = true;
  else if (operation == "hud-off") showHUD = false;
  else if (operation.compare(0, 6, "focus:") == 0) {
    size_t index = std::stoul(operation.substr(6));
    if(index >= mesh.nEdges()) throw std::out_of_range("Edge index exceeds mesh size");
    auto e=mesh.edgesBegin(); std::advance(e,index); auto h=e->halfedge();
    auto a=h->vertex()->position,b=h->twin()->vertex()->position;
    view_focus=(a+b)*0.5; view_distance=(a-b).norm()*7;
    Vector3D normal;
    if(!h->isBoundary()) normal+=h->face()->normal();
    if(!h->twin()->isBoundary()) normal+=h->twin()->face()->normal();
    normal.normalize();
    camera_angles.x=atan2(normal.x,normal.y);camera_angles.y=asin(normal.z)*0.85;
  }''');p.write_text(s,encoding='utf-8')
p=r/'code/verification/geometry_tests.cpp';s=p.read_text(encoding='utf-8-sig');s=s.replace('  MeshResampler resampler;','''  auto tet=tetra(); auto te=tet.edgesBegin();auto th=te->halfedge();
  auto ta=th->vertex(),tb=th->twin()->vertex();tet.flipEdge(te);validate(tet);
  require(te->halfedge()->vertex()==ta && te->halfedge()->twin()->vertex()==tb,"Tetrahedron flip protected");
  MeshResampler resampler;''');p.write_text(s,encoding='utf-8')
t=E.parse(r/'code/dae/cube.dae');root=t.getroot();ns=root.tag.split('}')[0][1:];n={'c':ns};E.register_namespace('',ns)
g=root.find('c:library_geometries/c:geometry/c:mesh',n)
a=g.find("c:source[@id='Cube-mesh-positions']/c:float_array",n);a.text='-1 -1 0 1 -1 0 1 1 0 -1 1 0';a.set('count','12')
g.find("c:source[@id='Cube-mesh-positions']/c:technique_common/c:accessor",n).set('count','4')
f=g.find('c:polylist',n);f.set('count','2');f.find('c:vcount',n).text='3 3';f.find('c:p',n).text='0 1 1 1 2 1 0 1 2 1 3 1'
t.write(r/'code/dae/open-square.dae',encoding='utf-8',xml_declaration=True)
