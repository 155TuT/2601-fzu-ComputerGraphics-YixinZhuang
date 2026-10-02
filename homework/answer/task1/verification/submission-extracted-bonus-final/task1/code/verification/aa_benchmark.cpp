#include "analytic_rasterizer.h"
#include "CGL/lodepng.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
using namespace CGL;
struct BenchTriangle { Vector2D v[3]; Color color[3]; bool gradient=false; };
struct Scene { std::string name;std::vector<BenchTriangle> triangles; };
static constexpr size_t W=96,H=96;
double cross(Vector2D a,Vector2D b,Vector2D p){return(b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);}
void submit(Rasterizer& r,const Scene& s){for(const auto& t:s.triangles){
  if(t.gradient)r.rasterize_interpolated_color_triangle(t.v[0].x,t.v[0].y,t.color[0],t.v[1].x,t.v[1].y,t.color[1],t.v[2].x,t.v[2].y,t.color[2]);
  else r.rasterize_triangle(t.v[0].x,t.v[0].y,t.v[1].x,t.v[1].y,t.v[2].x,t.v[2].y,t.color[0]);
}}
BenchTriangle tri(Vector2D a,Vector2D b,Vector2D c,Color color){BenchTriangle t;t.v[0]=a;t.v[1]=b;t.v[2]=c;t.color[0]=color;return t;}
void png(const std::string& path,const std::vector<unsigned char>& rgb){std::vector<unsigned char> rgba;rgba.reserve(W*H*4);for(size_t p=0;p<W*H;++p){rgba.insert(rgba.end(),rgb.begin()+3*p,rgb.begin()+3*p+3);rgba.push_back(255);}if(lodepng::encode(path,rgba,W,H))throw std::runtime_error("PNG encode failed");}
// Independent numerical reference: at each 64x64 grid location, search the
// frontmost triangle and shade directly. No tested clipping/SIMD helpers used.
std::vector<double> reference(const Scene& s){
  std::vector<double> ref(W*H*3,0);const int side=64;
  for(size_t y=0;y<H;++y)for(size_t x=0;x<W;++x)for(int sy=0;sy<side;++sy)for(int sx=0;sx<side;++sx){
    Vector2D p(x+(sx+.5)/side,y+(sy+.5)/side);Color color=Color::White;
    for(auto it=s.triangles.rbegin();it!=s.triangles.rend();++it){const auto& t=*it;const double area=cross(t.v[0],t.v[1],t.v[2]);
      const double a=cross(t.v[1],t.v[2],p)/area,b=cross(t.v[2],t.v[0],p)/area,c=1-a-b;
      if(a>=0&&b>=0&&c>=0){color=t.gradient?float(a)*t.color[0]+float(b)*t.color[1]+float(c)*t.color[2]:t.color[0];break;}
    }
    const size_t i=3*(y*W+x);ref[i]+=255*color.r/(side*side);ref[i+1]+=255*color.g/(side*side);ref[i+2]+=255*color.b/(side*side);
  }return ref;
}
int main(int argc,char** argv){
  if(argc!=2){std::cerr<<"Usage aa_benchmark output-directory\n";return 2;}const std::string dir=argv[1];
  const Color black(0,0,0),red(1,0,0),blue(0,0,1);
  std::vector<Scene> scenes;
  scenes.push_back({"diagonal",{tri({7.13,10.17},{88.39,23.73},{17.41,86.29},black)}});
  scenes.push_back({"thin",{tri({5.13,19.17},{89.39,61.73},{89.32,61.87},black)}});
  scenes.push_back({"shared_edge",{tri({11.13,14.17},{84.39,23.73},{20.41,82.29},black),tri({20.41,82.29},{84.39,23.73},{91.03,87.19},black)}});
  scenes.push_back({"overlap",{tri({9.13,11.17},{86.39,21.73},{16.41,88.29},red),tri({2.03,44.71},{85.21,8.13},{77.97,90.21},blue)}});
  auto gradient=tri({-17.13,7.17},{91.39,14.73},{19.41,110.29},red);gradient.gradient=true;gradient.color[1]=Color(0,1,0);gradient.color[2]=blue;
  scenes.push_back({"gradient_clipped",{gradient}});
  std::ofstream raw(dir+"/aa-timings.csv"),summary(dir+"/aa-summary.csv");
  raw<<"scene,mode,repeat,total_ms\n";summary<<"scene,mode,median_ms,p95_ms,mae_8bit,rmse_8bit,edge_mae_8bit,max_error_8bit,retained_geometry_or_sample_bytes,framebuffer_bytes,foreground_pixels\n";
  for(const auto& scene:scenes){
    const auto ref=reference(scene);std::vector<unsigned char> ref8(ref.size());for(size_t i=0;i<ref.size();++i)ref8[i]=std::lround(ref[i]);png(dir+"/"+scene.name+"_reference.png",ref8);
    for(int mode=0;mode<4;++mode){std::vector<unsigned char> out(W*H*3);const unsigned int rates[3]={1,4,16};
      RasterizerImp regular(P_NEAREST,L_ZERO,W,H,mode<3?rates[mode]:1);AnalyticRasterizer analytic(P_NEAREST,L_ZERO,W,H,1);
      Rasterizer& r=mode==3?static_cast<Rasterizer&>(analytic):static_cast<Rasterizer&>(regular);r.set_framebuffer_target(out.data(),W,H);
      const std::string name=mode==3?"analytic":"ssaa_"+std::to_string(rates[mode]);std::vector<double> times;
      for(int repeat=-2;repeat<11;++repeat){const auto start=std::chrono::steady_clock::now();r.clear_buffers();submit(r,scene);r.resolve_to_framebuffer();
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(repeat>=0){times.push_back(ms);raw<<scene.name<<","<<name<<","<<repeat<<","<<std::setprecision(10)<<ms<<"\n";}}
      double mae=0,sq=0,edge=0,maximum=0;size_t edge_count=0,foreground=0;
      for(size_t p=0;p<W*H;++p){bool is_edge=false;for(int k=0;k<3;++k)if(ref[3*p+k]>1e-5&&ref[3*p+k]<254.99999)is_edge=true;
        if(out[3*p]!=255||out[3*p+1]!=255||out[3*p+2]!=255)++foreground;
        for(int k=0;k<3;++k){const double error=std::abs(out[3*p+k]-ref[3*p+k]);mae+=error;sq+=error*error;maximum=std::max(maximum,error);if(is_edge){edge+=error;++edge_count;}}}
      std::sort(times.begin(),times.end());const size_t bytes=mode==3?analytic.geometry_bytes():W*H*rates[mode]*sizeof(Color);
      summary<<scene.name<<","<<name<<","<<times[times.size()/2]<<","<<times.back()<<","<<mae/ref.size()<<","<<std::sqrt(sq/ref.size())<<","<<(edge_count?edge/edge_count:0)<<","<<maximum<<","<<bytes<<","<<out.size()<<","<<foreground<<"\n";
      png(dir+"/"+scene.name+"_"+name+".png",out);std::cout<<scene.name<<" "<<name<<" "<<times[times.size()/2]<<" ms\n";
    }
  }
}
