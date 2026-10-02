#include "analytic_rasterizer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace CGL {
namespace {
using ClipPolygon = std::vector<Vector2D>;
double cross(const Vector2D& a, const Vector2D& b, const Vector2D& c) {
  return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
}

// Sutherland-Hodgman clipping against a half-plane. Using complementary
// inside/outside tests partitions a convex polygon without overlapping areas.
ClipPolygon clip(const ClipPolygon& polygon, const Vector2D& a, const Vector2D& b,
             double orientation, bool inside) {
  ClipPolygon result;
  if (polygon.empty()) return result;
  result.reserve(polygon.size()+1);
  Vector2D previous = polygon.back();
  double ep = orientation * cross(a,b,previous);
  bool previous_in = inside ? ep >= 0 : ep <= 0;
  for (const Vector2D& current : polygon) {
    const double ec = orientation * cross(a,b,current);
    const bool current_in = inside ? ec >= 0 : ec <= 0;
    if (current_in != previous_in) {
      const double t = ep / (ep-ec);
      result.emplace_back(previous.x + t*(current.x-previous.x),
                          previous.y + t*(current.y-previous.y));
    }
    if (current_in) result.push_back(current);
    previous=current; ep=ec; previous_in=current_in;
  }
  return result;
}

// Translate before taking moments, preventing cancellation at large positions.
double measure(const ClipPolygon& polygon, Vector2D& centroid) {
  if (polygon.size()<3) return 0;
  const Vector2D origin=polygon[0];
  double twice_area=0, mx=0, my=0;
  for (size_t i=0; i<polygon.size(); ++i) {
    const Vector2D a=polygon[i]-origin;
    const Vector2D b=polygon[(i+1)%polygon.size()]-origin;
    const double z=a.x*b.y-a.y*b.x;
    twice_area+=z; mx+=(a.x+b.x)*z; my+=(a.y+b.y)*z;
  }
  if (std::abs(twice_area)<1e-15) return 0;
  centroid=Vector2D(origin.x+mx/(3*twice_area),origin.y+my/(3*twice_area));
  return std::abs(twice_area)*.5;
}

ClipPolygon pixel_square(size_t x, size_t y) {
  return {{double(x),double(y)}, {double(x+1),double(y)},
          {double(x+1),double(y+1)}, {double(x),double(y+1)}};
}

ClipPolygon intersection(ClipPolygon polygon, const Vector2D v[3], double orientation) {
  for (int e=0; e<3 && !polygon.empty(); ++e)
    polygon=clip(polygon,v[e],v[(e+1)%3],orientation,true);
  return polygon;
}

// Each emitted outside piece is disjoint from all subsequently emitted pieces.
// The final inside remainder is the visible intersection, handled separately.
void subtract(ClipPolygon region, const Vector2D v[3], double orientation,
              std::vector<ClipPolygon>& outside) {
  for (int e=0; e<3 && !region.empty(); ++e) {
    ClipPolygon piece=clip(region,v[e],v[(e+1)%3],orientation,false);
    Vector2D ignored;
    if (measure(piece,ignored)>1e-15) outside.push_back(std::move(piece));
    region=clip(region,v[e],v[(e+1)%3],orientation,true);
  }
}
unsigned char byte(double value) {
  return static_cast<unsigned char>(std::lround(255*std::max(0.0,std::min(1.0,value))));
}
}

AnalyticRasterizer::AnalyticRasterizer(PixelSampleMethod p, LevelSampleMethod l,
    size_t w, size_t h, unsigned int rate)
    : sample_rate(rate), psm(p), lsm(l), width(0),height(0),tiles_x(0),tiles_y(0) {
  set_sample_rate(rate);
  set_framebuffer_target(nullptr,w,h);
}

void AnalyticRasterizer::set_sample_rate(unsigned int rate) {
  const unsigned int side=static_cast<unsigned int>(std::sqrt(rate));
  if (!rate || static_cast<unsigned long long>(side)*side!=rate)
    throw std::invalid_argument("The sample rate must be a positive square.");
  // Kept only so switching back to regular SSAA retains the UI's chosen spp.
  sample_rate=rate;
}

void AnalyticRasterizer::set_framebuffer_target(unsigned char* rgb,size_t w,size_t h) {
  if (w>static_cast<size_t>(std::numeric_limits<int>::max()) ||
      h>static_cast<size_t>(std::numeric_limits<int>::max()) ||
      (h && w>std::numeric_limits<size_t>::max()/h/3))
    throw std::length_error("Analytic framebuffer dimensions are too large.");
  framebuffer=rgb; width=w; height=h;
  tiles_x=(w+tile_side-1)/tile_side; tiles_y=(h+tile_side-1)/tile_side;
  tile_bins.clear(); tile_bins.resize(tiles_x*tiles_y);
  primitives.clear();
}

void AnalyticRasterizer::clear_buffers() {
  primitives.clear();
  for (auto& bin:tile_bins) bin.clear();
  if (framebuffer) std::fill(framebuffer,framebuffer+3*width*height,255);
}

void AnalyticRasterizer::add(Primitive p) {
  for (const auto& v:p.v) if (!std::isfinite(v.x)||!std::isfinite(v.y)) return;
  p.area=cross(p.v[0],p.v[1],p.v[2]);
  if (!std::isfinite(p.area)||std::abs(p.area)<1e-12||!width||!height) return;
  const double min_x=std::min({p.v[0].x,p.v[1].x,p.v[2].x});
  const double max_x=std::max({p.v[0].x,p.v[1].x,p.v[2].x});
  const double min_y=std::min({p.v[0].y,p.v[1].y,p.v[2].y});
  const double max_y=std::max({p.v[0].y,p.v[1].y,p.v[2].y});
  const double bx=std::max(0.0,std::floor(min_x)), ex=std::min(double(width),std::ceil(max_x));
  const double by=std::max(0.0,std::floor(min_y)), ey=std::min(double(height),std::ceil(max_y));
  if (bx>=ex||by>=ey) return;
  p.x0=int(bx);p.x1=int(ex);p.y0=int(by);p.y1=int(ey);
  const size_t id=primitives.size();primitives.push_back(p);
  for (size_t ty=p.y0/tile_side;ty<=size_t(p.y1-1)/tile_side;++ty)
    for (size_t tx=p.x0/tile_side;tx<=size_t(p.x1-1)/tile_side;++tx)
      tile_bins[ty*tiles_x+tx].push_back(id);
}

void AnalyticRasterizer::rasterize_triangle(float x0,float y0,float x1,float y1,
    float x2,float y2,Color c) {
  Primitive p;p.v[0]={x0,y0};p.v[1]={x1,y1};p.v[2]={x2,y2};p.color[0]=c;add(p);
}

void AnalyticRasterizer::rasterize_interpolated_color_triangle(float x0,float y0,Color c0,
    float x1,float y1,Color c1,float x2,float y2,Color c2) {
  Primitive p;p.v[0]={x0,y0};p.v[1]={x1,y1};p.v[2]={x2,y2};
  p.color[0]=c0;p.color[1]=c1;p.color[2]=c2;p.kind=1;add(p);
}

void AnalyticRasterizer::rasterize_textured_triangle(float x0,float y0,float u0,float v0,
    float x1,float y1,float u1,float v1,float x2,float y2,float u2,float v2,Texture& tex) {
  Primitive p;p.v[0]={x0,y0};p.v[1]={x1,y1};p.v[2]={x2,y2};
  p.uv[0]={u0,v0};p.uv[1]={u1,v1};p.uv[2]={u2,v2};p.texture=&tex;p.kind=2;add(p);
}

void AnalyticRasterizer::rasterize_point(float x,float y,Color c) {
  if (!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>=width||y>=height) return;
  const float px=std::floor(x),py=std::floor(y);
  rasterize_triangle(px,py,px+1,py,px,py+1,c);
  rasterize_triangle(px,py+1,px+1,py,px+1,py+1,c);
}

void AnalyticRasterizer::rasterize_line(float x0,float y0,float x1,float y1,Color c) {
  // Match the starter's point/line treatment; analytic AA targets filled shapes.
  if (!std::isfinite(x0)||!std::isfinite(y0)||!std::isfinite(x1)||!std::isfinite(y1)) return;
  const double dx=x1-x0,dy=y1-y0,steps=std::max(std::abs(dx),std::abs(dy));
  if (!steps) { rasterize_point(x0,y0,c);return; }
  // Liang-Barsky clip first, avoiding work for a huge off-screen line.
  double t0=0,t1=1;
  const double p[4]={-dx,dx,-dy,dy};
  // Pixel coordinates occupy [0,width) x [0,height), not [0,width-1].
  // An endpoint exactly on the outer edge is rejected by rasterize_point.
  const double q[4]={x0,double(width)-x0,y0,double(height)-y0};
  for (int i=0;i<4;++i) {
    if (!p[i]) { if(q[i]<0)return; continue; }
    const double t=q[i]/p[i];
    if(p[i]<0)t0=std::max(t0,t);else t1=std::min(t1,t);
    if(t0>t1)return;
  }
  const size_t n=static_cast<size_t>(std::ceil((t1-t0)*steps));
  for(size_t i=0;i<=n;++i) {
    const double t=n?t0+(t1-t0)*i/n:t0;
    rasterize_point(float(x0+t*dx),float(y0+t*dy),c);
  }
}

Color AnalyticRasterizer::shade(const Primitive& p,const Vector2D& q) const {
  if(!p.kind)return p.color[0];
  const double a=cross(p.v[1],p.v[2],q)/p.area;
  const double b=cross(p.v[2],p.v[0],q)/p.area;
  const double c=1-a-b;
  if(p.kind==1)return float(a)*p.color[0]+float(b)*p.color[1]+float(c)*p.color[2];
  const Vector2D uv=a*p.uv[0]+b*p.uv[1]+c*p.uv[2];
  const double ax=(p.v[1].y-p.v[2].y)/p.area, bx=(p.v[2].y-p.v[0].y)/p.area;
  const double ay=(p.v[2].x-p.v[1].x)/p.area, by=(p.v[0].x-p.v[2].x)/p.area;
  SampleParams sp;sp.p_uv=uv;
  sp.p_dx_uv=uv+ax*p.uv[0]+bx*p.uv[1]-(ax+bx)*p.uv[2];
  sp.p_dy_uv=uv+ay*p.uv[0]+by*p.uv[1]-(ay+by)*p.uv[2];
  sp.psm=psm;sp.lsm=lsm;return p.texture->sample(sp);
}

void AnalyticRasterizer::resolve_to_framebuffer() {
  if(!framebuffer)return;
  for(size_t y=0;y<height;++y)for(size_t x=0;x<width;++x) {
    const auto& bin=tile_bins[(y/tile_side)*tiles_x+x/tile_side];
    if(bin.empty())continue;
    std::vector<ClipPolygon> uncovered{pixel_square(x,y)};
    double rgb[3]={0,0,0};
    // Traverse from last drawn to first. Earlier layers only contribute where
    // later opaque geometry is absent. This avoids seams along tessellation.
    for(auto it=bin.rbegin();it!=bin.rend()&&!uncovered.empty();++it) {
      const Primitive& p=primitives[*it];
      if(int(x)<p.x0||int(x)>=p.x1||int(y)<p.y0||int(y)>=p.y1)continue;
      const double orientation=p.area>0?1:-1;
      std::vector<ClipPolygon> next;
      for(const ClipPolygon& region:uncovered) {
        ClipPolygon visible=intersection(region,p.v,orientation);
        Vector2D centroid;const double area=measure(visible,centroid);
        if(area<=1e-15){next.push_back(region);continue;}
        const Color c=shade(p,centroid);
        rgb[0]+=area*c.r;rgb[1]+=area*c.g;rgb[2]+=area*c.b;
        subtract(region,p.v,orientation,next);
      }
      uncovered.swap(next);
    }
    // The remaining exact area shows the white background.
    for(const ClipPolygon& region:uncovered) {
      Vector2D centroid;const double area=measure(region,centroid);
      for(double& v:rgb)v+=area;
    }
    const size_t i=3*(y*width+x);
    framebuffer[i]=byte(rgb[0]);framebuffer[i+1]=byte(rgb[1]);framebuffer[i+2]=byte(rgb[2]);
  }
}

double AnalyticRasterizer::pixel_coverage(const Vector2D& a,const Vector2D& b,
    const Vector2D& c,size_t x,size_t y) {
  const Vector2D v[3]={a,b,c};const double area=cross(a,b,c);
  if(!std::isfinite(area)||std::abs(area)<1e-12)return 0;
  Vector2D centroid;
  return measure(intersection(pixel_square(x,y),v,area>0?1:-1),centroid);
}

size_t AnalyticRasterizer::geometry_bytes() const {
  size_t bytes=primitives.capacity()*sizeof(Primitive)+tile_bins.capacity()*sizeof(std::vector<size_t>);
  for(const auto& bin:tile_bins)bytes+=bin.capacity()*sizeof(size_t);
  return bytes;
}
}
