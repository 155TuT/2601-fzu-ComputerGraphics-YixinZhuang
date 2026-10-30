#include "student_code.h"
#include "collada.h"
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <iostream>
using namespace CGL;
static int checks=0;
void require(bool ok,const char* message) { ++checks; if(!ok) throw std::runtime_error(message); }
void close(const Vector3D& a,const Vector3D& b) { require((a-b).norm()<1e-10,"Vector mismatch"); }
struct Curve : BezierCurve { Curve():BezierCurve(4){} void parameter(float x){t=x;} };
struct Patch : BezierPatch { using BezierPatch::evaluate; using BezierPatch::evaluate1D; using BezierPatch::evaluateStep; using BezierPatch::controlPoints; };
void validate(HalfedgeMesh& mesh) {
  require(mesh.nHalfedges()==2*mesh.nEdges(),"Halfedge count");
  std::set<const Halfedge*> outgoing;
  for(auto h=mesh.halfedgesBegin();h!=mesh.halfedgesEnd();++h) {
    require(h->twin()->twin()==h,"Twin involution");
    require(h->edge()==h->twin()->edge(),"Twin edge");
    require(h->next()->vertex()==h->twin()->vertex(),"Edge endpoint consistency");
    require(h->face()==h->next()->face(),"Face cycle assignment");
    require(h->vertex()!=h->twin()->vertex(),"Self edge");
  }
  for(auto v=mesh.verticesBegin();v!=mesh.verticesEnd();++v) {
    auto start=v->halfedge(),h=start;size_t n=0;
    do { require(h->vertex()==v,"Vertex representative/ring"); require(outgoing.insert(&*h).second,"Duplicate ring halfedge");
      h=h->twin()->next();require(++n<=mesh.nHalfedges(),"Vertex ring termination");
    } while(h!=start);
    require(std::isfinite(v->normal().norm()),"Finite vertex normal");
  }
  require(outgoing.size()==mesh.nHalfedges(),"Every halfedge belongs to one vertex fan");
  for(auto f=mesh.facesBegin();f!=mesh.facesEnd();++f) {
    auto h=f->halfedge();require(h->face()==f,"Face representative");
    require(h->next()->next()->next()==h,"Triangular face");
  }
  for(auto f=mesh.boundariesBegin();f!=mesh.boundariesEnd();++f) {
    auto start=f->halfedge(),h=start;size_t n=0;
    do { require(h->face()==f,"Boundary face assignment");h=h->next();require(++n<=mesh.nHalfedges(),"Boundary termination"); }while(h!=start);
  }
  for(auto e=mesh.edgesBegin();e!=mesh.edgesEnd();++e) require(e->halfedge()->edge()==e,"Edge representative");
}
HalfedgeMesh square() {
 HalfedgeMesh m;m.build({{0,1,2},{0,2,3}},{{0,0,0},{2,0,0},{2,2,0},{0,2,0}});return m;
}
HalfedgeMesh tetra() {
 HalfedgeMesh m;m.build({{0,2,1},{0,1,3},{1,2,3},{2,0,3}},{{1,1,1},{-1,-1,1},{-1,1,-1},{1,-1,-1}});return m;
}
int main(int argc, char** argv) {
 try {
  Curve c;c.parameter(.25f);
  auto level=c.evaluateStep({{0,0},{1,2},{2,0}});
  require(level.size()==2,"Curve level size");require((level[0]-Vector2D(.25,.5)).norm()<1e-12,"Curve interpolation");
  require(c.evaluateStep({}).empty(),"Empty step");require(c.evaluateStep({{1,2}}).empty(),"Singleton step");
  Patch p;p.controlPoints={{{0,0,0},{2,0,2}},{{0,4,4},{2,4,6}}};
  close(p.evaluate(.25,.75),{.5,3,3.5});close(p.evaluate(0,0),{0,0,0});close(p.evaluate(1,1),{2,4,6});
  close(p.evaluate1D({{1,2,3}},.8),{1,2,3});
  close(p.evaluate1D({{0,0,0},{2,4,0},{4,0,0}},.5),{2,2,0});
  bool rejected=false;try{p.evaluate1D({},.5);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Empty evaluation rejected");
  auto m=square();validate(m);for(auto v=m.verticesBegin();v!=m.verticesEnd();++v)close(v->normal(),{0,0,1});
  auto diagonal=m.edgesBegin();while(diagonal->isBoundary())++diagonal;
  auto a=diagonal->halfedge()->vertex()->position,b=diagonal->halfedge()->twin()->vertex()->position;
  require(m.flipEdge(diagonal)==diagonal,"Flip iterator identity");validate(m);
  require(m.nVertices()==4&&m.nEdges()==5&&m.nFaces()==2,"Flip preserves counts");
  require((diagonal->halfedge()->vertex()->position-a).norm()>0,"Flip changes diagonal");
  m.flipEdge(diagonal);validate(m);
  auto midpoint=m.splitEdge(diagonal);validate(m);close(midpoint->position,(a+b)*.5);
  require(m.nVertices()==5&&m.nEdges()==8&&m.nFaces()==4,"Interior split counts");
  require(!midpoint->halfedge()->edge()->isNew,"New vertex points along split edge");
  auto boundary=m.edgesBegin();while(!boundary->isBoundary())++boundary;
  auto oldh=boundary->halfedge();m.flipEdge(boundary);require(boundary->halfedge()==oldh,"Boundary flip unchanged");
  auto vb=m.splitEdge(boundary);validate(m);require(vb->isBoundary(),"Boundary split vertex");
  require(m.nVertices()==6&&m.nEdges()==10&&m.nFaces()==5&&m.nBoundaries()==1,"Boundary split counts");
  auto tet=tetra(); auto te=tet.edgesBegin();auto th=te->halfedge();
  auto ta=th->vertex(),tb=th->twin()->vertex();tet.flipEdge(te);validate(tet);
  require(te->halfedge()->vertex()==ta && te->halfedge()->twin()->vertex()==tb,"Tetrahedron flip protected");
  MeshResampler resampler;
  auto open=square();resampler.upsample(open);validate(open);
  close(open.verticesBegin()->position,{.25,.25,0});
  require(open.nVertices()==9&&open.nEdges()==16&&open.nFaces()==8,"Open Loop counts");
  resampler.upsample(open);validate(open);require(open.nFaces()==32,"Second open Loop");
  auto closed=tetra();auto old=closed.verticesBegin();resampler.upsample(closed);validate(closed);close(old->position,{.25,.25,.25});
  require(closed.nVertices()==10&&closed.nEdges()==24&&closed.nFaces()==16,"Closed Loop counts");
  resampler.upsample(closed);validate(closed);require(closed.nFaces()==64,"Second closed Loop");
  // A nonplanar fan makes the two face areas unequal: expected normal follows their area vectors.
  HalfedgeMesh weighted; weighted.build({{0,1,2},{0,2,3}},{{0,0,0},{2,0,0},{0,2,0},{0,0,1}});
  close(weighted.verticesBegin()->normal(),Vector3D(2,0,4).unit());
  HalfedgeMesh degenerate;degenerate.build({{0,1,2}},{{0,0,0},{1,0,0},{2,0,0}});close(degenerate.verticesBegin()->normal(),{0,0,0});
  if(argc > 1) {
    Scene scene;require(ColladaParser::load(argv[1],&scene)==0,"Load teacher mesh");
    for(auto& node:scene.nodes) if(node.instance && node.instance->type==POLYMESH) {
      auto& polymesh=*static_cast<Polymesh*>(node.instance);
      std::vector<std::vector<Index>> faces;
      for(const auto& face:polymesh.polygons) faces.push_back(face.vertex_indices);
      HalfedgeMesh loaded;loaded.build(faces,polymesh.vertices);validate(loaded);
      for(int level=0;level<2;++level) {
        const size_t v=loaded.nVertices(),e=loaded.nEdges(),f=loaded.nFaces(),b=loaded.nBoundaries();
        resampler.upsample(loaded);validate(loaded);
        require(loaded.nVertices()==v+e,"Scene Loop vertex growth");
        require(loaded.nFaces()==4*f,"Scene Loop face growth");
        require(loaded.nBoundaries()==b,"Scene boundary component preservation");
      }
      std::cout<<"scene="<<argv[1]<<" final V/E/F="<<loaded.nVertices()<<"/"<<loaded.nEdges()<<"/"<<loaded.nFaces()<<std::endl;
    }
  }
  std::cout<<"PASS "<<checks<<" checks: de Casteljau, weighted normals, flip, interior/boundary split, open/closed Loop, connectivity"<<std::endl;
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<std::endl;return 1;}
}
