#include "analytic_rasterizer.h"
#include <cmath>
#include <iostream>
#include <vector>
using namespace CGL;
int main() {
  int count=0,failed=0;
  auto check=[&](bool value,const char* label){++count;if(!value){++failed;std::cerr<<"FAIL "<<label<<"\n";}};
  const Vector2D a(0,0),b(1,0),c(0,1);
  check(std::abs(AnalyticRasterizer::pixel_coverage(a,b,c,0,0)-.5)<1e-12,"unit right triangle exact half pixel");
  check(std::abs(AnalyticRasterizer::pixel_coverage(a,c,b,0,0)-.5)<1e-12,"opposite winding same exact area");
  check(AnalyticRasterizer::pixel_coverage(a,b,c,1,0)==0,"touching edge zero area");
  check(AnalyticRasterizer::pixel_coverage(a,a,b,0,0)==0,"degenerate triangle zero area");
  check(std::abs(AnalyticRasterizer::pixel_coverage({0,0},{1,0},{1,.02},0,0)-.01)<1e-12,"subpixel thin triangle area");
  check(std::abs(AnalyticRasterizer::pixel_coverage({9999,9999},{10000,9999},{9999,10000},9999,9999)-.5)<1e-12,"large coordinate moment stability");
  std::vector<unsigned char> out(3*4*4);
  AnalyticRasterizer r(P_NEAREST,L_ZERO,4,4,1);r.set_framebuffer_target(out.data(),4,4);
  auto channel=[&](int x,int y,int c){return int(out[3*(y*4+x)+c]);};
  r.clear_buffers();r.rasterize_triangle(0,0,1,0,0,1,Color::Black);r.resolve_to_framebuffer();
  check(channel(0,0,0)==128&&channel(0,0,1)==128&&channel(0,0,2)==128,"half coverage blends white geometrically");
  r.rasterize_triangle(0,1,1,0,1,1,Color::Black);r.resolve_to_framebuffer();
  check(channel(0,0,0)==0,"tessellated square has no diagonal seam");
  r.clear_buffers();r.rasterize_triangle(0,0,1,0,0,1,Color(1,0,0));
  r.rasterize_triangle(0,0,1,0,0,1,Color(0,0,1));r.resolve_to_framebuffer();
  check(channel(0,0,0)==128&&channel(0,0,1)==128&&channel(0,0,2)==255,"opaque overlap last shape wins before integration");
  r.clear_buffers();r.rasterize_triangle(0,0,1,0,0,1,Color(0,0,1));
  r.rasterize_triangle(0,0,1,0,0,1,Color(1,0,0));r.resolve_to_framebuffer();
  check(channel(0,0,0)==255&&channel(0,0,1)==128&&channel(0,0,2)==128,"reversing painter order reverses visible color");
  r.clear_buffers();r.rasterize_interpolated_color_triangle(0,0,Color(1,0,0),1,0,Color(0,1,0),0,1,Color(0,0,1));r.resolve_to_framebuffer();
  check(channel(0,0,0)==170&&channel(0,0,1)==170&&channel(0,0,2)==170,"linear RGB integral uses area centroid exactly");
  r.clear_buffers();r.rasterize_triangle(0,0,1,0,1,.02,Color::Black);r.resolve_to_framebuffer();
  check(channel(0,0,0)==252,"thin geometry survives without sample-center hit");
  r.clear_buffers();r.rasterize_triangle(-1,-1,2,-1,-1,2,Color::Black);r.resolve_to_framebuffer();
  check(channel(0,0,0)==128,"screen clipping integrates partial pixel");
  r.clear_buffers();r.rasterize_point(.3f,.7f,Color(1,0,0));r.resolve_to_framebuffer();
  check(channel(0,0,0)==255&&channel(0,0,1)==0&&channel(0,0,2)==0,"point retains full-pixel treatment");
  const auto previous=out;r.set_sample_rate(16);r.resolve_to_framebuffer();
  check(previous==out,"analytic result independent of retained UI sample rate");
  r.clear_buffers();r.resolve_to_framebuffer();
  check(out==std::vector<unsigned char>(out.size(),255),"clear resets geometry and background");
  check(r.get_primitive_count()==0,"no stale primitives after clear");
  r.rasterize_line(3.1f,3.5f,3.9f,3.5f,Color::Black);r.resolve_to_framebuffer();
  check(channel(3,3,0)==0,"line inside final pixel is retained after clipping");
  r.set_framebuffer_target(nullptr,0,0);r.resolve_to_framebuffer();check(true,"zero viewport safe");
  try{r.set_sample_rate(3);check(false,"invalid rate rejected");}catch(const std::invalid_argument&){check(true,"invalid rate rejected");}
  std::cout<<"Analytic checks: "<<count<<", passed: "<<count-failed<<", failed: "<<failed<<"\n";
  return failed?1:0;
}
