#include "triangle.h"

#include "CGL/CGL.h"
#include "GL/glew.h"

namespace CGL {
namespace SceneObjects {

Triangle::Triangle(const Mesh *mesh, size_t v1, size_t v2, size_t v3) {
  p1 = mesh->positions[v1];
  p2 = mesh->positions[v2];
  p3 = mesh->positions[v3];
  n1 = mesh->normals[v1];
  n2 = mesh->normals[v2];
  n3 = mesh->normals[v3];
  bbox = BBox(p1);
  bbox.expand(p2);
  bbox.expand(p3);

  bsdf = mesh->get_bsdf();
}

  BBox Triangle::get_bbox() const { return bbox; }

bool Triangle::has_intersection(const Ray &r) const {
  Intersection hit;
  return intersect(r, &hit);
}

bool Triangle::intersect(const Ray &r, Intersection *isect) const {
  const Vector3D e1 = p2-p1, e2 = p3-p1;
  const Vector3D p = cross(r.d, e2);
  const double det = dot(e1, p);
  const double scale = sqrt(e1.norm2()*e2.norm2()*r.d.norm2());
  if (scale == 0 || fabs(det) <= 1e-12*scale) return false;
  const Vector3D offset = r.o-p1;
  const double u = dot(offset,p)/det;
  if (u < 0 || u > 1) return false;
  const Vector3D q = cross(offset,e1);
  const double v = dot(r.d,q)/det;
  if (v < 0 || u+v > 1) return false;
  const double t = dot(e2,q)/det;
  if (t < r.min_t || t > r.max_t) return false;
  r.max_t = t;
  isect->t = t;
  Vector3D n = (1-u-v)*n1 + u*n2 + v*n3;
  if (n.norm2() == 0) n = cross(e1,e2);
  isect->n = n.unit();
  isect->primitive = this;
  isect->bsdf = bsdf;
  return true;
}

void Triangle::draw(const Color &c, float alpha) const {
  glColor4f(c.r, c.g, c.b, alpha);
  glBegin(GL_TRIANGLES);
  glVertex3d(p1.x, p1.y, p1.z);
  glVertex3d(p2.x, p2.y, p2.z);
  glVertex3d(p3.x, p3.y, p3.z);
  glEnd();
}

void Triangle::drawOutline(const Color &c, float alpha) const {
  glColor4f(c.r, c.g, c.b, alpha);
  glBegin(GL_LINE_LOOP);
  glVertex3d(p1.x, p1.y, p1.z);
  glVertex3d(p2.x, p2.y, p2.z);
  glVertex3d(p3.x, p3.y, p3.z);
  glEnd();
}

} // namespace SceneObjects
} // namespace CGL
