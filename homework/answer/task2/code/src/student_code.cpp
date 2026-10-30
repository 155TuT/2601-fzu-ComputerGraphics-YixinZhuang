#include "student_code.h"
#include <stdexcept>

namespace CGL {
std::vector<Vector2D> BezierCurve::evaluateStep(const std::vector<Vector2D>& points) {
  std::vector<Vector2D> result;
  for (size_t i = 1; i < points.size(); ++i)
    result.push_back((1.0 - t) * points[i - 1] + t * points[i]);
  return result;
}
std::vector<Vector3D> BezierPatch::evaluateStep(const std::vector<Vector3D>& points, double t) const {
  std::vector<Vector3D> result;
  for (size_t i = 1; i < points.size(); ++i)
    result.push_back((1.0 - t) * points[i - 1] + t * points[i]);
  return result;
}
Vector3D BezierPatch::evaluate1D(const std::vector<Vector3D>& points, double t) const {
  if (points.empty()) throw std::invalid_argument("Bezier control points are empty");
  std::vector<Vector3D> level = points;
  while (level.size() > 1) level = evaluateStep(level, t);
  return level.front();
}
Vector3D BezierPatch::evaluate(double u, double v) const {
  std::vector<Vector3D> rows;
  for (const auto& row : controlPoints) rows.push_back(evaluate1D(row, u));
  return evaluate1D(rows, v);
}
Vector3D Vertex::normal() const {
  Vector3D areaVector;
  HalfedgeCIter start = halfedge(), h = start;
  do {
    if (!h->face()->isBoundary()) {
      const Vector3D a = h->next()->vertex()->position - position;
      const Vector3D b = h->next()->next()->vertex()->position - position;
      areaVector += cross(a, b);
    }
    h = h->twin()->next();
  } while (h != start);
  const double length = areaVector.norm();
  return length > 0.0 ? areaVector / length : Vector3D();
}
EdgeIter HalfedgeMesh::flipEdge(EdgeIter e) {
  if (e->isBoundary()) return e;
  auto h0 = e->halfedge(), h3 = h0->twin();
  auto h1 = h0->next(), h2 = h1->next();
  auto h4 = h3->next(), h5 = h4->next();
  if (h2->next() != h0 || h5->next() != h3) return e;
  auto a = h0->vertex(), b = h3->vertex(), c = h2->vertex(), d = h5->vertex();
  if (c == d) return e;
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
  }
  auto f0 = h0->face(), f1 = h3->face();
  h0->setNeighbors(h5, h3, c, e, f0);
  h3->setNeighbors(h2, h0, d, e, f1);
  h5->next() = h1; h5->face() = f0;
  h1->next() = h0; h1->face() = f0;
  h2->next() = h4; h2->face() = f1;
  h4->next() = h3; h4->face() = f1;
  if (a->halfedge() == h0) a->halfedge() = h4;
  if (b->halfedge() == h3) b->halfedge() = h1;
  e->halfedge() = h0;
  f0->halfedge() = h0; f1->halfedge() = h3;
  return e;
}
VertexIter HalfedgeMesh::splitEdge(EdgeIter e) {
  auto h0 = e->halfedge();
  if (h0->isBoundary()) h0 = h0->twin();
  auto h3 = h0->twin(), h1 = h0->next(), h2 = h1->next();
  if (h2->next() != h0) return h0->vertex();
  const bool boundary = h3->isBoundary();
  auto h4 = h3->next(), h5 = h4->next();
  if (!boundary && h5->next() != h3) return h0->vertex();
  auto a = h0->vertex(), b = h3->vertex(), c = h2->vertex();
  auto m = newVertex();
  m->position = (a->position + b->position) * 0.5;
  m->newPosition = m->position; m->isNew = true;
  auto e1 = newEdge(), ec = newEdge();
  e->isNew = false; e1->isNew = false; ec->isNew = true;
  auto ha = newHalfedge(), hb = newHalfedge();
  auto hc = newHalfedge(), hd = newHalfedge();
  auto f0 = h0->face(), f2 = newFace();
  if (boundary) {
    // Keep B -> M on the boundary and insert M -> A immediately after it.
    // Every pointer update stays within the incident triangle and boundary.
    auto boundaryNext = h3->next();
    h0->setNeighbors(h1, h3, m, e, f0);
    h3->next() = hb;
    ha->setNeighbors(hc, hb, a, e1, f2);
    hb->setNeighbors(boundaryNext, ha, m, e1, h3->face());
    hc->setNeighbors(h2, hd, m, ec, f2);
    hd->setNeighbors(h0, hc, c, ec, f0);
    h1->next() = hd;
    h2->next() = ha; h2->face() = f2;
    if (a->halfedge() == h0) a->halfedge() = ha;
    m->halfedge() = hb;
  } else {
    auto d = h5->vertex();
    auto f1 = h3->face(), f3 = newFace();
    auto ed = newEdge(); ed->isNew = true;
    auto he = newHalfedge(), hf = newHalfedge();
    h0->setNeighbors(hc, h3, a, e, f0);
    h3->setNeighbors(h4, h0, m, e, f3);
    ha->setNeighbors(h1, hb, m, e1, f2);
    hb->setNeighbors(he, ha, b, e1, f1);
    hc->setNeighbors(h2, hd, m, ec, f0);
    hd->setNeighbors(ha, hc, c, ec, f2);
    he->setNeighbors(h5, hf, m, ed, f1);
    hf->setNeighbors(h3, he, d, ed, f3);
    h1->next() = hd; h1->face() = f2;
    h2->next() = h0;
    h4->next() = hf; h4->face() = f3;
    h5->next() = hb;
    if (b->halfedge() == h3) b->halfedge() = h1;
    m->halfedge() = ha;
    ed->halfedge() = he;
    f1->halfedge() = hb; f3->halfedge() = h3;
  }
  e->halfedge() = h0; e1->halfedge() = ha; ec->halfedge() = hc;
  f0->halfedge() = h0; f2->halfedge() = ha;
  return m;
}
void MeshResampler::upsample(HalfedgeMesh& mesh) {
  std::vector<EdgeIter> originalEdges;
  for (auto v = mesh.verticesBegin(); v != mesh.verticesEnd(); ++v) {
    v->isNew = false;
    Vector3D sum, boundarySum;
    size_t degree = 0, boundaryDegree = 0;
    auto start = v->halfedge(), h = start;
    do {
      auto neighbor = h->twin()->vertex();
      sum += neighbor->position; ++degree;
      if (h->edge()->isBoundary()) { boundarySum += neighbor->position; ++boundaryDegree; }
      h = h->twin()->next();
    } while (h != start);
    if (boundaryDegree == 2) v->newPosition = 0.75 * v->position + 0.125 * boundarySum;
    else {
      const double beta = degree == 3 ? 3.0 / 16.0 : 3.0 / (8.0 * degree);
      v->newPosition = (1.0 - degree * beta) * v->position + beta * sum;
    }
  }
  for (auto e = mesh.edgesBegin(); e != mesh.edgesEnd(); ++e) {
    originalEdges.push_back(e); e->isNew = false;
    auto h = e->halfedge(), t = h->twin();
    Vector3D ends = h->vertex()->position + t->vertex()->position;
    e->newPosition = e->isBoundary() ? 0.5 * ends :
      0.375 * ends + 0.125 * (h->next()->next()->vertex()->position + t->next()->next()->vertex()->position);
  }
  for (auto e : originalEdges) {
    Vector3D position = e->newPosition;
    auto v = mesh.splitEdge(e);
    v->newPosition = position;
  }
  for (auto e = mesh.edgesBegin(); e != mesh.edgesEnd(); ++e)
    if (e->isNew && e->halfedge()->vertex()->isNew != e->halfedge()->twin()->vertex()->isNew)
      mesh.flipEdge(e);
  for (auto v = mesh.verticesBegin(); v != mesh.verticesEnd(); ++v) v->position = v->newPosition;
}
}
