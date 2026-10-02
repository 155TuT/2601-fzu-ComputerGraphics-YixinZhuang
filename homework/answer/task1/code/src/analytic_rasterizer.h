#pragma once

#include "rasterizer.h"
#include <cstddef>
#include <vector>

namespace CGL {

// Experimental exact geometric coverage for opaque SVG primitives. The regular
// SSAA renderer remains the default. Affine colors are integrated exactly;
// textures are sampled at each visible polygon's centroid (an approximation).
class AnalyticRasterizer : public Rasterizer {
public:
  AnalyticRasterizer(PixelSampleMethod p, LevelSampleMethod l,
                     size_t w, size_t h, unsigned int rate);
  unsigned int get_sample_rate() override { return sample_rate; }
  void set_sample_rate(unsigned int rate) override;
  void set_psm(PixelSampleMethod p) override { psm = p; }
  void set_lsm(LevelSampleMethod l) override { lsm = l; }
  void rasterize_point(float x, float y, Color c) override;
  void rasterize_line(float x0, float y0, float x1, float y1, Color c) override;
  void rasterize_triangle(float x0, float y0, float x1, float y1,
                          float x2, float y2, Color c) override;
  void rasterize_interpolated_color_triangle(float x0, float y0, Color c0,
      float x1, float y1, Color c1, float x2, float y2, Color c2) override;
  void rasterize_textured_triangle(float x0, float y0, float u0, float v0,
      float x1, float y1, float u1, float v1,
      float x2, float y2, float u2, float v2, Texture& tex) override;
  void set_framebuffer_target(unsigned char* rgb, size_t w, size_t h) override;
  void clear_buffers() override;
  void resolve_to_framebuffer() override;
  size_t geometry_bytes() const;
  size_t get_primitive_count() const { return primitives.size(); }
  // Analytic area and first moment of a triangle intersected with one pixel.
  static double pixel_coverage(const Vector2D& a, const Vector2D& b,
                               const Vector2D& c, size_t x, size_t y);
private:
  struct Primitive {
    Vector2D v[3], uv[3];
    Color color[3];
    Texture* texture = nullptr;
    int kind = 0;
    double area = 0;
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
  };
  static const size_t tile_side = 16;
  unsigned int sample_rate;
  PixelSampleMethod psm;
  LevelSampleMethod lsm;
  size_t width, height, tiles_x, tiles_y;
  unsigned char* framebuffer = nullptr;
  std::vector<Primitive> primitives;
  std::vector<std::vector<size_t>> tile_bins;
  void add(Primitive primitive);
  Color shade(const Primitive& primitive, const Vector2D& p) const;
};
}
