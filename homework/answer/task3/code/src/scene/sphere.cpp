#include "sphere.h"

#include <cmath>

#include "pathtracer/bsdf.h"
#include "util/sphere_drawing.h"

namespace CGL {
namespace SceneObjects {

bool Sphere::test(const Ray &r, double &t1, double &t2) const {
  const Vector3D offset = r.o-o;
  const double a = dot(r.d,r.d), b = 2*dot(offset,r.d);
  const double c = dot(offset,offset)-r2;
  if (a <= 0) return false;
  const double discriminant = b*b-4*a*c;
  if (discriminant < 0) return false;
  const double q = -0.5*(b+std::copysign(sqrt(discriminant),b));
  if (q == 0) t1 = t2 = -b/(2*a);
  else { t1=q/a; t2=c/q; if (t1>t2) std::swap(t1,t2); }
  return true;
}

bool Sphere::has_intersection(const Ray &r) const {
  double near_t, far_t;
  if (!test(r,near_t,far_t)) return false;
  const double t = near_t >= r.min_t ? near_t : far_t;
  if (t < r.min_t || t > r.max_t) return false;
  r.max_t = t;
  return true;
}

bool Sphere::intersect(const Ray &r, Intersection *i) const {
  double near_t, far_t;
  if (!test(r,near_t,far_t)) return false;
  const double t = near_t >= r.min_t ? near_t : far_t;
  if (t < r.min_t || t > r.max_t) return false;
  r.max_t = t;
  i->t = t;
  i->n = normal(r.at_time(t));
  i->primitive = this;
  i->bsdf = get_bsdf();
  return true;
}

void Sphere::draw(const Color &c, float alpha) const {
  Misc::draw_sphere_opengl(o, r, c);
}

void Sphere::drawOutline(const Color &c, float alpha) const {
  // Misc::draw_sphere_opengl(o, r, c);
}

} // namespace SceneObjects
} // namespace CGL
