#include "pathtracer/pathtracer.h"
#include "pathtracer/render_options.h"
#include "scene/sphere.h"
#include "scene/triangle.h"
#include "scene/light.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <memory>
using namespace CGL;
using namespace CGL::SceneObjects;
static size_t checks=0;
void require(bool condition, const char* message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
bool near(double a,double b,double eps=1e-9) { return std::abs(a-b)<eps; }
void test_camera() {
  Camera camera;
  Collada::CameraInfo info;
  info.hFov=90; info.vFov=90; info.nClip=0.1f; info.fClip=100;
  camera.configure(info,400,400);
  camera.place(Vector3D(),PI/2,0,5,0.1,100);
  Ray middle=camera.generate_ray(.5,.5);
  require((middle.d-Vector3D(0,0,-1)).norm()<1e-9,"camera center direction");
  require(near(middle.min_t,0.1,1e-7)&&near(middle.max_t,100),"camera clips");
  Ray corner=camera.generate_ray(1,1);
  require((corner.d-Vector3D(1,1,-1).unit()).norm()<1e-8,"camera FOV corner");
  camera.place(Vector3D(),PI/2,PI/2,5,0.1,100);
  require((camera.generate_ray(.5,.5).d-Vector3D(-1,0,0)).norm()<1e-8,"camera world rotation");
}
void test_intersections() {
  DiffuseBSDF material(Vector3D(.7));
  Triangle triangle;
  triangle.p1=Vector3D(0,0,0); triangle.p2=Vector3D(1,0,0); triangle.p3=Vector3D(0,1,0);
  triangle.n1=Vector3D(0,0,1); triangle.n2=Vector3D(0,1,1).unit(); triangle.n3=Vector3D(1,0,1).unit();
  triangle.bsdf=&material;
  Intersection hit;
  Ray ray(Vector3D(.25,.25,2),Vector3D(0,0,-1));
  require(triangle.intersect(ray,&hit)&&near(hit.t,2)&&near(ray.max_t,2),"triangle hit and closest t");
  require((hit.n-(.5*triangle.n1+.25*triangle.n2+.25*triangle.n3).unit()).norm()<1e-10,"barycentric normal");
  require(hit.bsdf==&material&&hit.primitive==&triangle,"intersection record");
  require(!triangle.has_intersection(Ray(Vector3D(1,1,1),Vector3D(0,0,-1))),"triangle outside");
  require(!triangle.has_intersection(Ray(Vector3D(.2,.2,1),Vector3D(1,0,0))),"triangle parallel");
  require(triangle.has_intersection(Ray(Vector3D(0,0,1),Vector3D(0,0,-1))),"triangle vertex");
  Ray segment(Vector3D(.2,.2,1),Vector3D(0,0,-1)); segment.max_t=.5;
  require(!triangle.has_intersection(segment),"triangle clipping");
  triangle.p3=triangle.p2;
  require(!triangle.has_intersection(Ray(Vector3D(.2,0,1),Vector3D(0,0,-1))),"degenerate triangle");
  SphereObject object(Vector3D(),1,&material);
  Sphere sphere(&object,Vector3D(),1);
  ray=Ray(Vector3D(0,0,3),Vector3D(0,0,-2));
  require(sphere.intersect(ray,&hit)&&near(hit.t,1),"sphere nonunit direction");
  ray=Ray(Vector3D(),Vector3D(0,0,1));
  require(sphere.intersect(ray,&hit)&&near(hit.t,1),"sphere inside exit");
  require(sphere.has_intersection(Ray(Vector3D(1,0,3),Vector3D(0,0,-1))),"sphere tangent");
  require(!sphere.has_intersection(Ray(Vector3D(2,0,3),Vector3D(0,0,-1))),"sphere miss");
  ray=Ray(Vector3D(0,0,3),Vector3D(0,0,-1)); ray.min_t=2.5;
  require(sphere.intersect(ray,&hit)&&near(hit.t,4),"sphere near root clipped");
  BBox box(Vector3D(-1),Vector3D(1)); double lo=0,hi=100;
  require(box.intersect(Ray(Vector3D(0,0,3),Vector3D(0,0,-1)),lo,hi)&&near(lo,2)&&near(hi,4),"slab interval");
  lo=0; hi=100;
  require(box.intersect(Ray(Vector3D(1,0,3),Vector3D(0,0,-1)),lo,hi),"parallel on slab boundary");
  lo=0; hi=100;
  require(!box.intersect(Ray(Vector3D(2,0,3),Vector3D(0,0,-1)),lo,hi),"parallel outside slab");
  lo=0; hi=100;
  require(!BBox().intersect(Ray(Vector3D(),Vector3D(1,0,0)),lo,hi),"empty box");
}
size_t check_tree(BVHNode* node,size_t maximum) {
  if(node->isLeaf()) {
    require(size_t(node->end-node->start)<=maximum,"leaf capacity");
    return node->end-node->start;
  }
  require(node->l&&node->r,"binary tree children");
  return check_tree(node->l,maximum)+check_tree(node->r,maximum);
}
void test_bvh() {
  DiffuseBSDF material(Vector3D(.5));
  std::mt19937 rng(12345); std::uniform_real_distribution<double> u(-5,5);
  std::vector<std::unique_ptr<SphereObject>> objects;
  std::vector<std::unique_ptr<Sphere>> spheres;
  std::vector<Primitive*> primitives;
  for(int k=0;k<200;++k) {
    Vector3D p(u(rng),u(rng),u(rng));
    objects.emplace_back(new SphereObject(p,.2,&material));
    spheres.emplace_back(new Sphere(objects.back().get(),p,.2));
    primitives.push_back(spheres.back().get());
  }
  BVHAccel bvh(primitives,4);
  require(check_tree(bvh.get_root(),4)==primitives.size(),"BVH primitive coverage");
  for(int k=0;k<1000;++k) {
    Ray a(Vector3D(u(rng),u(rng),10),Vector3D(u(rng),u(rng),-10).unit()), b=a;
    Intersection ia,ib;
    bool accelerated=bvh.intersect(a,&ia), brute=false;
    for(auto primitive:primitives) brute=primitive->intersect(b,&ib)||brute;
    require(accelerated==brute,"BVH and brute hit agreement");
    if(brute) require(near(ia.t,ib.t)&&ia.primitive==ib.primitive,"BVH closest agreement");
    Ray c(a.o,a.d),d=c; bool any=bvh.has_intersection(c), expected=false;
    for(auto primitive:primitives) if(primitive->has_intersection(d)){expected=true;break;}
    require(any==expected,"BVH shadow agreement");
  }
  std::vector<Primitive*> same(17,primitives.front()); BVHAccel degenerate(same,1);
  require(check_tree(degenerate.get_root(),1)==17,"identical centroid termination");
  BVHAccel empty(std::vector<Primitive*>{});
  require(!empty.has_intersection(Ray(Vector3D(),Vector3D(1,0,0))),"empty BVH");
}
void test_lighting() {
  DiffuseBSDF material(Vector3D(.2,.4,.8));
  Vector3D wo(0,0,1), wi; double pdf=0;
  require((material.f(wo,wo)-Vector3D(.2,.4,.8)/PI).norm()<1e-12,"Lambertian BRDF");
  seed_random(17); double mean_z=0;
  for(int k=0;k<20000;++k) {
    Vector3D f=material.sample_f(wo,&wi,&pdf);
    require(pdf>0&&wi.z>0&&near(wi.norm(),1,1e-9),"cosine sample hemisphere and PDF");
    require((f*(wi.z/pdf)-Vector3D(.2,.4,.8)).norm()<1e-10,"cosine throughput equals albedo");
    mean_z+=wi.z;
  }
  require(std::abs(mean_z/20000-2.0/3)<.01,"cosine distribution mean");
  PathTracer tracer;
  PointLight light(Vector3D(2),Vector3D(0,0,2));
  Scene scene({}, {&light}); BVHAccel empty(std::vector<Primitive*>{});
  tracer.scene=&scene; tracer.bvh=&empty;
  Intersection hit; hit.t=1; hit.n=Vector3D(0,0,1); hit.bsdf=&material;
  Ray ray(Vector3D(0,0,1),Vector3D(0,0,-1));ray.depth=1;
  Vector3D expected=Vector3D(.2,.4,.8)*(2/PI);
  require((tracer.one_bounce_radiance(ray,hit)-expected).norm()<1e-10,"analytic point direct light");
  require((tracer.at_least_one_bounce_radiance(ray,hit)-expected).norm()<1e-10,"depth one direct light");
  SphereObject occluder(Vector3D(0,0,1),.25,&material); Sphere blocker(&occluder,occluder.o,.25);
  BVHAccel blocked({&blocker}); tracer.bvh=&blocked;
  require(tracer.one_bounce_radiance(ray,hit).norm()==0,"shadow occlusion");
  EmissionBSDF emission(Vector3D(3));hit.bsdf=&emission;
  require((tracer.zero_bounce_radiance(ray,hit)-Vector3D(3)).norm()==0,"zero bounce emission");
}
// A controlled transport chain isolates recursion, bounce selection and roulette.
struct ChainBSDF : BSDF {
  Vector3D f(Vector3D,Vector3D) override {return Vector3D(.5);}
  Vector3D sample_f(Vector3D,Vector3D* wi,double* pdf) override {
    *wi=Vector3D(0,0,1);*pdf=1;return Vector3D(.5);
  }
  Vector3D get_emission() const override {return Vector3D();}
  bool is_delta() const override {return false;}
};
struct ChainLight : SceneLight {
  Vector3D sample_L(Vector3D,Vector3D* wi,double* dist,double* pdf) const override {
    *wi=Vector3D(0,0,1);*dist=INF_D;*pdf=1;return Vector3D(1);
  }
  bool is_delta_light() const override {return true;}
};
struct ChainPrimitive : Primitive {
  BSDF* bsdf;
  explicit ChainPrimitive(BSDF* b):bsdf(b){}
  BBox get_bbox() const override {return BBox(Vector3D(-100),Vector3D(100));}
  bool has_intersection(const Ray&) const override {return false;}
  bool intersect(const Ray& r,Intersection* hit) const override {
    if(r.min_t>1||r.max_t<1) return false;
    r.max_t=1;hit->t=1;hit->n=Vector3D(0,0,1);hit->bsdf=bsdf;hit->primitive=this;return true;
  }
  BSDF* get_bsdf() const override {return bsdf;}
  void draw(const Color&,float) const override {}
  void drawOutline(const Color&,float) const override {}
};
void test_transport() {
  ChainBSDF material;ChainPrimitive surface(&material);ChainLight light;
  BVHAccel bvh({&surface});Scene scene({}, {&light});
  PathTracer tracer;tracer.bvh=&bvh;tracer.scene=&scene;
  render_options().roulette=false;
  for(size_t depth=0;depth<=8;++depth) {
    tracer.max_ray_depth=depth;
    Ray ray(Vector3D(),Vector3D(0,0,1));ray.depth=depth;
    tracer.isAccumBounces=true;
    require(near(tracer.est_radiance_global_illumination(ray).x,1-std::pow(.5,depth)),"cumulative bounce geometric sum");
    ray.max_t=INF_D;tracer.isAccumBounces=false;
    require(near(tracer.est_radiance_global_illumination(ray).x,depth ? std::pow(.5,depth):0),"isolated bounce geometric term");
  }
  render_options().roulette=true;tracer.isAccumBounces=true;tracer.max_ray_depth=2;
  for(int k=0;k<100;++k) {
    Ray ray(Vector3D(),Vector3D(0,0,1));ray.depth=2;
    require(near(tracer.est_radiance_global_illumination(ray).x,.75),"first indirect bounce always survives");
  }
  tracer.max_ray_depth=8;double sum=0;seed_random(34871);
  for(int k=0;k<40000;++k) {
    Ray ray(Vector3D(),Vector3D(0,0,1));ray.depth=8;
    sum+=tracer.est_radiance_global_illumination(ray).x;
  }
  require(std::abs(sum/40000-(1-std::pow(.5,8)))<.005,"roulette unbiased finite-depth expectation");
}
int main() {
  try {
    test_camera(); test_intersections(); test_bvh(); test_lighting(); test_transport();
    std::cout << "PASS " << checks << " checks: camera, intersections, slabs, BVH/brute, cosine BSDF, shadows, bounce sums and roulette expectation\n";
    return 0;
  } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n';return 1; }
}
