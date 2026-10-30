#include "pathtracer/bsdf.h"
#include "pathtracer/pathtracer.h"
#include "scene/triangle.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace CGL;
using namespace CGL::SceneObjects;
static size_t checks = 0;
void require(bool value, const char* message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
bool near(double a, double b, double tolerance = 1e-10) { return fabs(a-b) < tolerance; }
bool finite(const Vector3D& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
void reflection_and_refraction() {
  MirrorBSDF mirror(Vector3D(.9,.8,.7));
  Vector3D wo(.6,0,.8), wi; double pdf;
  Vector3D value = mirror.sample_f(wo,&wi,&pdf);
  require((wi-Vector3D(-.6,0,.8)).norm()<1e-12,"reflection direction");
  require(near(pdf,1) && (value*wi.z-Vector3D(.9,.8,.7)).norm()<1e-12,"mirror throughput");
  require(mirror.refract(wo,&wi,1.5),"entering refraction");
  require(near(wi.x,-.4)&&wi.z<0&&near(wi.norm(),1),"Snell entering angle");
  Vector3D backward;
  require(mirror.refract(wi,&backward,1.5),"exiting refraction");
  require((backward-wo).norm()<1e-12,"reversible Snell directions");
  require(!mirror.refract(Vector3D(.9,0,-sqrt(.19)),&wi,1.5),"total internal reflection");
  require(!mirror.refract(wo,&wi,0),"invalid IOR");
  RefractionBSDF transmission(Vector3D(1),0,1.5);
  value=transmission.sample_f(wo,&wi,&pdf);
  const double entering_weight=value.x*fabs(wi.z)/pdf;
  Vector3D exit;
  value=transmission.sample_f(wi,&exit,&pdf);
  const double exiting_weight=value.x*fabs(exit.z)/pdf;
  require(near(entering_weight,1/2.25)&&near(exiting_weight,2.25),"radiance eta squared scaling");
  require(near(entering_weight*exiting_weight,1),"two-interface radiance scaling cancels");
  value=transmission.sample_f(Vector3D(.9,0,-sqrt(.19)),&wi,&pdf);
  require(value.norm2()==0&&pdf==0,"pure transmission has no TIR transmission lobe");
}
void glass_probabilities() {
  GlassBSDF glass(Vector3D(1),Vector3D(1),0,1.5);
  seed_random(128791);
  size_t reflected=0; double throughput=0;
  for(size_t s=0;s<100000;++s) {
    Vector3D wi;double pdf;
    Vector3D value=glass.sample_f(Vector3D(0,0,1),&wi,&pdf);
    if(wi.z>0) {
      ++reflected;require(near(pdf,.04),"normal-incidence Fresnel reflectance");
      require(near(value.x*fabs(wi.z)/pdf,1),"glass reflection probability cancellation");
    } else {
      require(near(pdf,.96),"Fresnel transmittance probability");
      require(near(value.x*fabs(wi.z)/pdf,1/2.25),"glass transmission probability cancellation");
    }
    throughput+=value.x*fabs(wi.z)/pdf;
  }
  require(fabs(double(reflected)/100000-.04)<.002,"Fresnel sampling frequency");
  require(fabs(throughput/100000-(.04+.96/2.25))<.002,"glass expected radiance");
  Vector3D wi;double pdf;
  const Vector3D value=glass.sample_f(Vector3D(.9,0,-sqrt(.19)),&wi,&pdf);
  require(wi.z<0&&near(pdf,1)&&near(value.x*fabs(wi.z),1),"glass TIR reflects all energy");
}
void beckmann_distribution() {
  const Vector3D eta(.2,.9,1.1), k(3.9,2.45,2.14);
  MicrofacetBSDF metal(eta,k,.2);
  const Vector3D normal_fresnel=metal.F(Vector3D(0,0,1));
  for(int channel=0;channel<3;++channel) {
    double e=eta[channel],a=k[channel];
    require(near(normal_fresnel[channel],((e-1)*(e-1)+a*a)/((e+1)*(e+1)+a*a)),"conductor normal Fresnel");
  }
  require((metal.F(Vector3D(1,0,0))-Vector3D(1)).norm()==0,"grazing conductor Fresnel");
  require(near(metal.Lambda(Vector3D(0,0,1)),0),"normal masking limit");
  require(metal.G(Vector3D(1,0,0),Vector3D(0,0,1))==0,"grazing masking limit");
  double projected_integral=0;
  for(int j=0;j<100000;++j) {
    double z=(j+.5)/100000;
    projected_integral+=metal.D(Vector3D(sqrt(1-z*z),0,z))*z*(2*PI/100000);
  }
  require(near(projected_integral,1,1e-6),"projected Beckmann density integrates to one");
  seed_random(1341);double mean_tan2=0;Vector3D average;
  for(int j=0;j<50000;++j) {
    const Vector3D wo(0,0,1);Vector3D wi;double pdf;
    const Vector3D f=metal.sample_f(wo,&wi,&pdf);
    require(finite(f)&&std::isfinite(pdf)&&pdf>=0,"finite microfacet samples");
    if(pdf==0) continue;
    Vector3D h=(wo+wi).unit();
    const double tan2=(1-h.z*h.z)/(h.z*h.z);
    const double expected=exp(-tan2/.04)/(PI*.04*pow(h.z,4))*h.z/(4*dot(wo,h));
    require(near(pdf,expected,1e-8),"Beckmann reflected-direction PDF");
    require((f-metal.f(wi,wo)).norm()<1e-10,"microfacet reciprocity");
    const Vector3D weight=f*(wi.z/pdf);
    require(finite(weight)&&weight.x>=0&&weight.y>=0&&weight.z>=0&&
            weight.x<=1.0000001&&weight.y<=1.0000001&&weight.z<=1.0000001,
            "normal-incidence microfacet energy bounds");
    average+=weight;mean_tan2+=tan2;
  }
  require(fabs(mean_tan2/50000-.04)<.001,"Beckmann sampled slope variance");
  require(average.x>0&&average.y>0&&average.z>0,"metal reflects finite energy");
  for(double roughness:{0.0,.01,.1,.5,1.0}) {
    MicrofacetBSDF sample(eta,k,roughness);
    require(finite(sample.f(Vector3D(.999999,0,.0014142132).unit(),Vector3D(0,0,1))),"finite grazing BRDF across roughness");
  }
}
Triangle plane(double z,BSDF* bsdf) {
  Triangle t;
  t.p1=Vector3D(-10,-10,z);t.p2=Vector3D(10,-10,z);t.p3=Vector3D(0,10,z);
  t.n1=t.n2=t.n3=Vector3D(0,0,1);t.bsdf=bsdf;
  t.bbox=BBox(t.p1);t.bbox.expand(t.p2);t.bbox.expand(t.p3);return t;
}
void delta_transport() {
  MirrorBSDF mirror(Vector3D(.8));EmissionBSDF emission(Vector3D(3));
  Triangle surface=plane(0,&mirror), light=plane(2,&emission);
  BVHAccel bvh({&surface,&light});Scene scene({},{});
  PathTracer tracer;tracer.bvh=&bvh;tracer.scene=&scene;tracer.max_ray_depth=1;
  Ray ray(Vector3D(0,0,1),Vector3D(0,0,-1));ray.depth=1;
  require(near(tracer.est_radiance_global_illumination(ray).x,2.4),"one mirror bounce sees emissive hit");
  ray.max_t=INF_D;ray.depth=2;tracer.max_ray_depth=2;tracer.isAccumBounces=false;
  require(tracer.est_radiance_global_illumination(ray).norm2()==0,"isolated second bounce excludes first-bounce emission");
  RefractionBSDF transmission(Vector3D(1),0,1.5);
  surface=plane(0,&transmission);light=plane(-2,&emission);
  BVHAccel transmitted_bvh({&surface,&light});tracer.bvh=&transmitted_bvh;
  tracer.max_ray_depth=1;tracer.isAccumBounces=true;ray.max_t=INF_D;ray.depth=1;
  require(near(tracer.est_radiance_global_illumination(ray).x,3/2.25),"negative-z delta transmission sees emission");
}
int main() {
  try {
    reflection_and_refraction();glass_probabilities();beckmann_distribution();delta_transport();
    std::cout<<"PASS "<<checks<<" material checks: mirror, Snell/TIR, Fresnel probabilities, eta scaling, Beckmann PDF/energy, delta transport\n";
  } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
