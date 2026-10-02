#include "rasterizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std;

namespace CGL {
namespace {

  unsigned int validate_sample_rate(unsigned int rate) {
    const unsigned int side = static_cast<unsigned int>(std::sqrt(rate));
    if (rate == 0 || static_cast<unsigned long long>(side) * side != rate) {
      throw std::invalid_argument("The sample rate must be a positive square.");
    }
    return rate;
  }

  size_t sample_count(size_t width, size_t height, unsigned int rate) {
    const size_t maximum = std::numeric_limits<size_t>::max();
    if (height != 0 && width > maximum / height) {
      throw std::length_error("Framebuffer dimensions are too large.");
    }
    const size_t pixels = width * height;
    if (pixels > maximum / rate || pixels > maximum / 3) {
      throw std::length_error("Framebuffer or sample buffer is too large.");
    }
    return pixels * rate;
  }

  // Edge i is opposite vertex i. Its affine coefficients and the reciprocal
  // signed area are computed once and reused for coverage and interpolation.
  struct TriangleSetup {
    double a[3], b[3], c[3];
    double area, inverse_area, orientation, edge_tolerance;
    double min_x, max_x, min_y, max_y;
    bool valid;

    TriangleSetup(float x0, float y0, float x1, float y1,
                  float x2, float y2) {
      const double x[3] = {x0, x1, x2};
      const double y[3] = {y0, y1, y2};
      valid = true;
      for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(x[i]) || !std::isfinite(y[i])) valid = false;
        const int j = (i + 1) % 3;
        const int k = (i + 2) % 3;
        a[i] = y[j] - y[k];
        b[i] = x[k] - x[j];
        c[i] = x[j] * y[k] - y[j] * x[k];
      }
      area = (x[1] - x[0]) * (y[2] - y[0])
           - (y[1] - y[0]) * (x[2] - x[0]);
      valid = valid && std::isfinite(area) && std::abs(area) > 1e-12;
      inverse_area = valid ? 1.0 / area : 0.0;
      orientation = area >= 0.0 ? 1.0 : -1.0;
      edge_tolerance = 16.0 * std::numeric_limits<double>::epsilon()
                     * std::max(1.0, std::abs(area));
      min_x = std::min(x[0], std::min(x[1], x[2]));
      max_x = std::max(x[0], std::max(x[1], x[2]));
      min_y = std::min(y[0], std::min(y[1], y[2]));
      max_y = std::max(y[0], std::max(y[1], y[2]));
    }
  };

  // Every pixel owns sample_rate consecutive colors. Its subsamples are
  // row-major at the centers of a sqrt(rate) by sqrt(rate) regular grid.
  template <typename ShadeSample>
  void for_each_triangle_sample(const TriangleSetup& triangle,
                                size_t width, size_t height,
                                unsigned int sample_rate,
                                const ShadeSample& shade) {
    if (!triangle.valid || width == 0 || height == 0) return;
    const double begin_x = std::max(0.0, std::floor(triangle.min_x));
    const double begin_y = std::max(0.0, std::floor(triangle.min_y));
    const double end_x = std::min(static_cast<double>(width), std::ceil(triangle.max_x));
    const double end_y = std::min(static_cast<double>(height), std::ceil(triangle.max_y));
    if (begin_x >= end_x || begin_y >= end_y) return;
    // Clip before converting, avoiding negative indices or huge casts.
    const size_t x_begin = static_cast<size_t>(begin_x);
    const size_t y_begin = static_cast<size_t>(begin_y);
    const size_t x_end = static_cast<size_t>(end_x);
    const size_t y_end = static_cast<size_t>(end_y);
    const unsigned int side = static_cast<unsigned int>(std::sqrt(sample_rate));
    const double spacing = 1.0 / side;
    double step_x[3];
    for (int i = 0; i < 3; ++i) step_x[i] = triangle.a[i] * spacing;

    for (size_t y = y_begin; y < y_end; ++y) {
      for (size_t x = x_begin; x < x_end; ++x) {
        const size_t pixel_base = (y * width + x) * sample_rate;
        for (unsigned int sy = 0; sy < side; ++sy) {
          const double py = y + (sy + 0.5) * spacing;
          const double px = x + 0.5 * spacing;
          double edge[3];
          for (int i = 0; i < 3; ++i) {
            edge[i] = triangle.a[i] * px + triangle.b[i] * py + triangle.c[i];
          }
          for (unsigned int sx = 0; sx < side; ++sx) {
            if (triangle.orientation * edge[0] >= -triangle.edge_tolerance &&
                triangle.orientation * edge[1] >= -triangle.edge_tolerance &&
                triangle.orientation * edge[2] >= -triangle.edge_tolerance) {
              const double w0 = edge[0] * triangle.inverse_area;
              const double w1 = edge[1] * triangle.inverse_area;
              const double w2 = 1.0 - w0 - w1;
              shade(pixel_base + sy * side + sx, w0, w1, w2);
            }
            // Edge functions are affine: only additions in the inner loop.
            for (int i = 0; i < 3; ++i) edge[i] += step_x[i];
          }
        }
      }
    }
  }

  unsigned char encode_channel(float value) {
    if (!std::isfinite(value)) value = 0.0f;
    value = std::max(0.0f, std::min(1.0f, value));
    return static_cast<unsigned char>(std::lround(value * 255.0f));
  }

} // anonymous namespace

  RasterizerImp::RasterizerImp(PixelSampleMethod psm, LevelSampleMethod lsm,
    size_t width, size_t height,
    unsigned int sample_rate) {
    this->psm = psm;
    this->lsm = lsm;
    this->width = width;
    this->height = height;
    this->sample_rate = validate_sample_rate(sample_rate);
    this->rgb_framebuffer_target = nullptr;
    sample_buffer.assign(sample_count(width, height, this->sample_rate), Color::White);
  }

  // Used by rasterize_point and rasterize_line
  void RasterizerImp::fill_pixel(size_t x, size_t y, Color c) {
    // Points and lines cover the entire pixel, so broadcast to every sample.
    if (x >= width || y >= height) return;
    const size_t base = (y * width + x) * sample_rate;
    std::fill(sample_buffer.begin() + base,
              sample_buffer.begin() + base + sample_rate, c);
  }

  // Rasterize a point: simple example to help you start familiarizing
  // yourself with the starter code.
  //
  void RasterizerImp::rasterize_point(float x, float y, Color color) {
    if (!std::isfinite(x) || !std::isfinite(y) ||
        x < 0.0f || y < 0.0f || x >= width || y >= height) return;
    fill_pixel(static_cast<size_t>(std::floor(x)),
               static_cast<size_t>(std::floor(y)), color);
  }

  // Rasterize a line.
  void RasterizerImp::rasterize_line(float x0, float y0,
    float x1, float y1,
    Color color) {
    if (!std::isfinite(x0) || !std::isfinite(y0) ||
        !std::isfinite(x1) || !std::isfinite(y1)) return;
    if (x0 == x1 && y0 == y1) {
      rasterize_point(x0, y0, color);
      return;
    }
    if (x0 > x1) {
      swap(x0, x1); swap(y0, y1);
    }

    float pt[] = { x0,y0 };
    float m = x0 == x1 ? std::numeric_limits<float>::infinity()
                       : (y1 - y0) / (x1 - x0);
    float dpt[] = { 1,m };
    int steep = abs(m) > 1;
    if (steep) {
      dpt[0] = x1 == x0 ? 0 : 1 / abs(m);
      dpt[1] = x1 == x0 ? (y1 > y0 ? 1.0f : -1.0f) : m / abs(m);
    }

    while (floor(pt[0]) <= floor(x1) && abs(pt[1] - y0) <= abs(y1 - y0)) {
      rasterize_point(pt[0], pt[1], color);
      pt[0] += dpt[0]; pt[1] += dpt[1];
    }
  }

  // Rasterize a triangle.
  void RasterizerImp::rasterize_triangle(float x0, float y0,
    float x1, float y1,
    float x2, float y2,
    Color color) {
    const TriangleSetup triangle(x0, y0, x1, y1, x2, y2);
    for_each_triangle_sample(triangle, width, height, sample_rate,
      [&](size_t sample, double, double, double) {
        sample_buffer[sample] = color;
      });
  }


  void RasterizerImp::rasterize_interpolated_color_triangle(float x0, float y0, Color c0,
    float x1, float y1, Color c1,
    float x2, float y2, Color c2)
  {
    const TriangleSetup triangle(x0, y0, x1, y1, x2, y2);
    for_each_triangle_sample(triangle, width, height, sample_rate,
      [&](size_t sample, double w0, double w1, double w2) {
        sample_buffer[sample] = c0 * static_cast<float>(w0)
                              + c1 * static_cast<float>(w1)
                              + c2 * static_cast<float>(w2);
      });
  }


  void RasterizerImp::rasterize_textured_triangle(float x0, float y0, float u0, float v0,
    float x1, float y1, float u1, float v1,
    float x2, float y2, float u2, float v2,
    Texture& tex)
  {
    const TriangleSetup triangle(x0, y0, x1, y1, x2, y2);
    if (!triangle.valid) return;
    const Vector2D uv0(u0, v0), uv1(u1, v1), uv2(u2, v2);
    // Affine barycentric gradients give UV(x+1,y)-UV(x,y) and
    // UV(x,y+1)-UV(x,y). The offset is one screen pixel, even at high rate.
    const Vector2D dx_uv = (uv0 * triangle.a[0] + uv1 * triangle.a[1]
                         + uv2 * triangle.a[2]) * triangle.inverse_area;
    const Vector2D dy_uv = (uv0 * triangle.b[0] + uv1 * triangle.b[1]
                         + uv2 * triangle.b[2]) * triangle.inverse_area;
    SampleParams sp;
    sp.psm = psm;
    sp.lsm = lsm;
    for_each_triangle_sample(triangle, width, height, sample_rate,
      [&](size_t sample, double w0, double w1, double w2) {
        sp.p_uv = uv0 * w0 + uv1 * w1 + uv2 * w2;
        sp.p_dx_uv = sp.p_uv + dx_uv;
        sp.p_dy_uv = sp.p_uv + dy_uv;
        sample_buffer[sample] = tex.sample(sp);
      });
  }

  void RasterizerImp::set_sample_rate(unsigned int rate) {
    rate = validate_sample_rate(rate);
    // Allocate first to keep the previous state consistent on failure.
    std::vector<Color> replacement(sample_count(width, height, rate), Color::White);
    sample_buffer.swap(replacement);
    sample_rate = rate;
  }


  void RasterizerImp::set_framebuffer_target(unsigned char* rgb_framebuffer,
    size_t width, size_t height)
  {
    std::vector<Color> replacement(sample_count(width, height, sample_rate), Color::White);
    sample_buffer.swap(replacement);
    this->width = width;
    this->height = height;
    this->rgb_framebuffer_target = rgb_framebuffer;
  }


  void RasterizerImp::clear_buffers() {
    if (rgb_framebuffer_target && width != 0 && height != 0) {
      std::fill(rgb_framebuffer_target, rgb_framebuffer_target + 3 * width * height, 255);
    }
    std::fill(sample_buffer.begin(), sample_buffer.end(), Color::White);
  }


  // This function is called at the end of rasterizing all elements of the
  // SVG file.  If you use a supersample buffer to rasterize SVG elements
  // for antialising, you could use this call to fill the target framebuffer
  // pixels from the supersample buffer data.
  //
  void RasterizerImp::resolve_to_framebuffer() {
    if (!rgb_framebuffer_target) return;
    const float weight = 1.0f / sample_rate;
    for (size_t pixel = 0; pixel < width * height; ++pixel) {
      Color col;
      const size_t base = pixel * sample_rate;
      for (unsigned int sample = 0; sample < sample_rate; ++sample) {
        col += sample_buffer[base + sample];
      }
      col *= weight;
      rgb_framebuffer_target[3 * pixel] = encode_channel(col.r);
      rgb_framebuffer_target[3 * pixel + 1] = encode_channel(col.g);
      rgb_framebuffer_target[3 * pixel + 2] = encode_channel(col.b);
    }

  }

  Rasterizer::~Rasterizer() { }


}// CGL
