#include "pathtracer.h"
#include "render_options.h"

#include "scene/light.h"
#include "scene/sphere.h"
#include "scene/triangle.h"


using namespace CGL::SceneObjects;

namespace CGL {

PathTracer::PathTracer() {
  gridSampler = new UniformGridSampler2D();
  hemisphereSampler = new UniformHemisphereSampler3D();
  bvh=nullptr; envLight=nullptr; scene=nullptr; camera=nullptr;
  max_ray_depth=1; isAccumBounces=1; ns_aa=1; ns_area_light=1;
  ns_diff=ns_glsy=ns_refr=1; samplesPerBatch=32; maxTolerance=0.05;
  direct_hemisphere_sample=false;
  tm_gamma=2.2f; tm_level=1.0f; tm_key=0.18; tm_wht=5.0f;
}

PathTracer::~PathTracer() {
  delete gridSampler;
  delete hemisphereSampler;
}

void PathTracer::set_frame_size(size_t width, size_t height) {
  sampleBuffer.resize(width, height);
  sampleCountBuffer.resize(width * height);
}

void PathTracer::clear() {
  bvh = NULL;
  scene = NULL;
  camera = NULL;
  sampleBuffer.clear();
  sampleCountBuffer.clear();
  sampleBuffer.resize(0, 0);
  sampleCountBuffer.resize(0, 0);
}

void PathTracer::write_to_framebuffer(ImageBuffer &framebuffer, size_t x0,
                                      size_t y0, size_t x1, size_t y1) {
  sampleBuffer.toColor(framebuffer, x0, y0, x1, y1);
}

Vector3D
PathTracer::estimate_direct_lighting_hemisphere(const Ray &r,
                                                const Intersection &isect) {
  Matrix3x3 o2w; make_coord_space(o2w,isect.n);
  const Matrix3x3 w2o=o2w.T();
  const Vector3D hit_p=r.at_time(isect.t), w_out=w2o*(-r.d);
  const size_t count=scene->lights.size()*ns_area_light;
  Vector3D sum;
  for (size_t s=0; s<count; ++s) {
    const Vector3D wi=hemisphereSampler->get_sample();
    Ray shadow(hit_p,(o2w*wi).unit()); shadow.min_t=EPS_F;
    Intersection light_hit;
    if (bvh->intersect(shadow,&light_hit))
      sum += isect.bsdf->f(w_out,wi)*light_hit.bsdf->get_emission()*wi.z*(2*PI);
  }
  return count ? sum/double(count) : Vector3D();
}

Vector3D
PathTracer::estimate_direct_lighting_importance(const Ray &r,
                                                const Intersection &isect) {
  Matrix3x3 o2w; make_coord_space(o2w,isect.n);
  const Matrix3x3 w2o=o2w.T();
  const Vector3D hit_p=r.at_time(isect.t), w_out=w2o*(-r.d);
  Vector3D sum;
  for (auto light : scene->lights) {
    const size_t count=light->is_delta_light() ? 1 : ns_area_light;
    Vector3D light_sum;
    for (size_t s=0; s<count; ++s) {
      Vector3D direction; double distance=0, pdf=0;
      const Vector3D radiance=light->sample_L(hit_p,&direction,&distance,&pdf);
      const Vector3D wi=w2o*direction;
      if (pdf<=0 || wi.z<=0 || distance<=2*EPS_F) continue;
      Ray shadow(hit_p,direction); shadow.min_t=EPS_F;
      shadow.max_t=distance-EPS_F;
      if (!bvh->has_intersection(shadow))
        light_sum += isect.bsdf->f(w_out,wi)*radiance*(wi.z/pdf);
    }
    if (count) sum += light_sum/double(count);
  }
  return sum;
}

Vector3D PathTracer::zero_bounce_radiance(const Ray &r,
                                          const Intersection &isect) {
  return isect.bsdf->get_emission();
}

Vector3D PathTracer::one_bounce_radiance(const Ray &r,
                                         const Intersection &isect) {
  return direct_hemisphere_sample ? estimate_direct_lighting_hemisphere(r,isect)
                                  : estimate_direct_lighting_importance(r,isect);
}

Vector3D PathTracer::at_least_one_bounce_radiance(const Ray &r,
                                                  const Intersection &isect) {
  if (r.depth==0) return Vector3D();
  Vector3D result;
  const bool delta = isect.bsdf->is_delta();
  if (!delta && (isAccumBounces || r.depth==1)) result=one_bounce_radiance(r,isect);
  if (!delta && r.depth==1) return result;
  Matrix3x3 o2w; make_coord_space(o2w,isect.n);
  const Vector3D wo=o2w.T()*(-r.d);
  Vector3D wi; double pdf=0;
  const Vector3D f=isect.bsdf->sample_f(wo,&wi,&pdf);
  if (pdf<=0 || (!delta && wi.z<=0) || wi.z==0 || f.norm2()==0) return result;
  // The first indirect bounce always survives. Subsequent bounces use roulette.
  const double survival=(!render_options().roulette || r.depth==max_ray_depth || r.depth==1) ? 1.0 : 0.65;
  if (!coin_flip(survival)) return result;
  Ray next(r.at_time(isect.t),(o2w*wi).unit());
  next.min_t=EPS_F; next.depth=r.depth-1;
  Intersection next_hit;
  if (bvh->intersect(next,&next_hit)) {
    Vector3D incoming = at_least_one_bounce_radiance(next,next_hit);
    // Delta paths cannot sample a light by NEE; a specular hit must carry emission.
    if (delta && (isAccumBounces || r.depth==1))
      incoming += zero_bounce_radiance(next,next_hit);
    const double cosine = delta ? abs_cos_theta(wi) : wi.z;
    result += f*incoming*(cosine/(pdf*survival));
  }
  return result;
}

Vector3D PathTracer::est_radiance_global_illumination(const Ray &r) {
  Intersection hit;
  if (!bvh || !bvh->intersect(r,&hit)) return Vector3D();
  if (render_options().normals) return normal_shading(hit.n);
  Vector3D result;
  if (isAccumBounces || max_ray_depth==0) result=zero_bounce_radiance(r,hit);
  if (max_ray_depth>0) result+=at_least_one_bounce_radiance(r,hit);
  return result;
}

void PathTracer::raytrace_pixel(size_t x, size_t y) {
  // A pixel owns its random stream, independent of worker count and tile order.
  seed_random(static_cast<uint32_t>((x+1)*73856093u ^ (y+1)*19349663u ^ render_options().seed*83492791u));
  const size_t count=std::max(size_t(1),ns_aa);
  Vector3D sum;
  for (size_t sample=0; sample<count; ++sample) {
    const Vector2D offset=gridSampler->get_sample();
    Ray ray=camera->generate_ray((x+offset.x)/sampleBuffer.w,
                                 (y+offset.y)/sampleBuffer.h);
    ray.depth=max_ray_depth;
    sum += est_radiance_global_illumination(ray);
  }
  sampleBuffer.update_pixel(sum/double(count),x,y);
  sampleCountBuffer[x+y*sampleBuffer.w]=static_cast<int>(count);
}

void PathTracer::autofocus(Vector2D loc) {
  Ray r=camera->generate_ray(loc.x/sampleBuffer.w,loc.y/sampleBuffer.h);
  Intersection hit;
  if (bvh && bvh->intersect(r,&hit)) camera->focalDistance=hit.t;
}

} // namespace CGL
