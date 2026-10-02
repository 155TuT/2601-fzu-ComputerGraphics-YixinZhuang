#include "rasterizer.h"
#include "texture.h"
#include "transforms.h"
#include "CGL/matrix3x3.h"
#include "CGL/vector2D.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace CGL;

namespace {
  int passed = 0;
  int failed = 0;

  void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
  }

  void near(double actual, double expected, double tolerance, const std::string& message) {
    if (std::abs(actual - expected) > tolerance || !std::isfinite(actual)) {
      std::ostringstream out;
      out << message << ": expected " << expected << ", got " << actual;
      throw std::runtime_error(out.str());
    }
  }

  void color_near(const Color& actual, const Color& expected, double tolerance = 1e-5) {
    near(actual.r, expected.r, tolerance, "red channel");
    near(actual.g, expected.g, tolerance, "green channel");
    near(actual.b, expected.b, tolerance, "blue channel");
  }

  void test(const std::string& name, const std::function<void()>& body) {
    try {
      body();
      ++passed;
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
  }

  struct Frame {
    size_t w, h;
    std::vector<unsigned char> pixels;
    RasterizerImp raster;

    Frame(size_t width, size_t height, unsigned int rate = 1)
      : w(width), h(height), pixels(3 * width * height, 255),
        raster(P_NEAREST, L_ZERO, width, height, rate) {
      raster.set_framebuffer_target(pixels.data(), w, h);
      raster.clear_buffers();
    }

    void resolve() { raster.resolve_to_framebuffer(); }

    void pixel(size_t x, size_t y, const Color& expected) const {
      // Accept either floor or nearest integer quantization, but no coverage error.
      const size_t index = 3 * (y * w + x);
      near(pixels[index], expected.r * 255.0, 1.01, "framebuffer red");
      near(pixels[index + 1], expected.g * 255.0, 1.01, "framebuffer green");
      near(pixels[index + 2], expected.b * 255.0, 1.01, "framebuffer blue");
    }

    void all(const Color& expected) const {
      for (size_t y = 0; y < h; ++y)
        for (size_t x = 0; x < w; ++x) pixel(x, y, expected);
    }
  };

  MipLevel solid_mip(size_t width, size_t height, unsigned char r,
                     unsigned char g, unsigned char b) {
    MipLevel mip = {width, height, std::vector<unsigned char>(width * height * 3)};
    for (size_t i = 0; i < width * height; ++i) {
      mip.texels[3 * i] = r;
      mip.texels[3 * i + 1] = g;
      mip.texels[3 * i + 2] = b;
    }
    return mip;
  }

  Texture corners() {
    Texture texture;
    texture.width = texture.height = 2;
    texture.mipmap.push_back(MipLevel{2, 2, {
      255, 0, 0,   0, 255, 0,
      0, 0, 255,   255, 255, 255
    }});
    return texture;
  }

  Texture colored_levels() {
    Texture texture;
    texture.width = texture.height = 8;
    texture.mipmap.push_back(solid_mip(8, 8, 255, 0, 0));
    texture.mipmap.push_back(solid_mip(4, 4, 0, 255, 0));
    texture.mipmap.push_back(solid_mip(2, 2, 0, 0, 255));
    texture.mipmap.push_back(solid_mip(1, 1, 255, 255, 255));
    return texture;
  }

  SampleParams params(Vector2D uv = Vector2D(.25, .25)) {
    SampleParams result;
    result.p_uv = result.p_dx_uv = result.p_dy_uv = uv;
    result.psm = P_NEAREST;
    result.lsm = L_ZERO;
    return result;
  }
}

int main() {
  test("translation homogeneous coordinates", [] {
    const Vector2D p = translate(4, -3) * Vector2D(1, 2);
    near(p.x, 5, 1e-8, "x"); near(p.y, -1, 1e-8, "y");
  });
  test("scale including reflection", [] {
    const Vector2D p = scale(2, -3) * Vector2D(1, 2);
    near(p.x, 2, 1e-8, "x"); near(p.y, -6, 1e-8, "y");
  });
  test("rotation degrees counterclockwise", [] {
    const Vector2D p = rotate(90) * Vector2D(1, 0);
    const Vector2D q = rotate(-90) * Vector2D(1, 0);
    near(p.x, 0, 1e-7, "90 x"); near(p.y, 1, 1e-7, "90 y");
    near(q.x, 0, 1e-7, "-90 x"); near(q.y, -1, 1e-7, "-90 y");
  });
  test("matrix composition rightmost first", [] {
    const Vector2D p = translate(5, 7) * rotate(90) * scale(2, 3) * Vector2D(1, 2);
    near(p.x, -1, 1e-7, "x"); near(p.y, 9, 1e-7, "y");
  });

  test("triangle pixel centers and included diagonal boundary", [] {
    Frame frame(4, 4);
    frame.raster.rasterize_triangle(0, 0, 4, 0, 0, 4, Color::Black);
    frame.resolve();
    // At pixel center (x+.5,y+.5), the analytic condition is x+y <= 3.
    for (size_t y = 0; y < 4; ++y)
      for (size_t x = 0; x < 4; ++x)
        frame.pixel(x, y, x + y <= 3 ? Color::Black : Color::White);
  });
  test("triangle CW and CCW identical", [] {
    Frame ccw(4, 4), cw(4, 4);
    ccw.raster.rasterize_triangle(0, 0, 4, 0, 0, 4, Color(1, 0, 0));
    cw.raster.rasterize_triangle(0, 0, 0, 4, 4, 0, Color(1, 0, 0));
    ccw.resolve(); cw.resolve();
    require(ccw.pixels == cw.pixels, "vertex order changed coverage");
  });
  test("triangle subpixel shape misses pixel center", [] {
    Frame frame(2, 2);
    frame.raster.rasterize_triangle(0, 0, .4f, 0, 0, .4f, Color::Black);
    frame.resolve(); frame.all(Color::White);
  });
  test("triangle collinear and coincident degeneracy", [] {
    Frame frame(4, 4);
    frame.raster.rasterize_triangle(0, 0, 2, 2, 4, 4, Color::Black);
    frame.raster.rasterize_triangle(1, 1, 1, 1, 1, 1, Color::Black);
    frame.resolve(); frame.all(Color::White);
  });
  test("triangle clipped across all framebuffer bounds", [] {
    Frame frame(4, 4);
    frame.raster.rasterize_triangle(-10, -10, 20, -10, -10, 20, Color::Black);
    frame.resolve(); frame.all(Color::Black);
  });
  test("triangle fully outside framebuffer", [] {
    Frame frame(4, 4);
    frame.raster.rasterize_triangle(-5, -5, -1, -5, -5, -1, Color::Black);
    frame.raster.rasterize_triangle(5, 5, 9, 5, 5, 9, Color::Black);
    frame.resolve(); frame.all(Color::White);
  });
  test("shared edge rectangle has no cracks", [] {
    Frame frame(4, 4, 9);
    frame.raster.rasterize_triangle(0, 0, 4, 0, 4, 4, Color::Black);
    frame.raster.rasterize_triangle(0, 0, 4, 4, 0, 4, Color::Black);
    frame.resolve(); frame.all(Color::Black);
  });

  const unsigned int rates[] = {4, 9, 16};
  const int covered[] = {3, 6, 10};
  for (int i = 0; i < 3; ++i) {
    const unsigned int rate = rates[i];
    const int count = covered[i];
    test("SSAA exact half-square coverage spp=" + std::to_string(rate), [=] {
      Frame frame(1, 1, rate);
      frame.raster.rasterize_triangle(0, 0, 1, 0, 0, 1, Color::Black);
      frame.resolve();
      // Grid centers on x+y=1 are included: 3/4, 6/9, and 10/16 covered.
      const float background = 1.0f - static_cast<float>(count) / rate;
      frame.pixel(0, 0, Color(background, background, background));
    });
  }
  test("point broadcasts to every supersample", [] {
    Frame frame(2, 2, 16);
    frame.raster.rasterize_point(.2f, .8f, Color::Black);
    frame.raster.rasterize_point(-.1f, .5f, Color::Black);
    frame.resolve(); frame.pixel(0, 0, Color::Black);
    frame.pixel(1, 0, Color::White); frame.pixel(0, 1, Color::White);
  });
  test("horizontal and vertical lines broadcast supersamples", [] {
    Frame frame(4, 4, 9);
    frame.raster.rasterize_line(.1f, 1.1f, 3.1f, 1.1f, Color::Black);
    frame.raster.rasterize_line(2.1f, .1f, 2.1f, 3.1f, Color::Black);
    frame.resolve();
    for (size_t y = 0; y < 4; ++y)
      for (size_t x = 0; x < 4; ++x)
        frame.pixel(x, y, y == 1 || x == 2 ? Color::Black : Color::White);
  });
  test("rate change allocates and clears sample buffer", [] {
    Frame frame(1, 1, 1);
    frame.raster.rasterize_point(.5f, .5f, Color::Black);
    frame.raster.set_sample_rate(16);
    require(frame.raster.get_sample_rate() == 16, "rate getter");
    frame.raster.clear_buffers();
    frame.raster.rasterize_triangle(0, 0, 1, 0, 0, 1, Color::Black);
    frame.resolve(); frame.pixel(0, 0, Color(.375f, .375f, .375f));
  });
  test("framebuffer resize preserves rate and supports new bounds", [] {
    Frame frame(1, 1, 9);
    frame.w = 3; frame.h = 1; frame.pixels.assign(9, 255);
    frame.raster.set_framebuffer_target(frame.pixels.data(), 3, 1);
    frame.raster.clear_buffers();
    frame.raster.rasterize_point(2.5f, .5f, Color(0, 0, 1));
    frame.resolve();
    frame.pixel(0, 0, Color::White); frame.pixel(1, 0, Color::White);
    frame.pixel(2, 0, Color(0, 0, 1));
    require(frame.raster.get_sample_rate() == 9, "resize changed sample rate");
  });
  test("clear removes previous frame", [] {
    Frame frame(2, 2, 4);
    frame.raster.fill_pixel(1, 1, Color::Black);
    frame.resolve(); frame.pixel(1, 1, Color::Black);
    frame.raster.clear_buffers(); frame.resolve(); frame.all(Color::White);
  });

  test("barycentric analytic weights at (.5,.5)", [] {
    Frame frame(2, 2);
    frame.raster.rasterize_interpolated_color_triangle(
      0, 0, Color(1, 0, 0), 2, 0, Color(0, 1, 0), 0, 2, Color(0, 0, 1));
    frame.resolve(); frame.pixel(0, 0, Color(.5f, .25f, .25f));
    frame.pixel(1, 1, Color::White);
  });
  test("barycentric affine mean under supersampling", [] {
    Frame frame(2, 2, 4);
    frame.raster.rasterize_interpolated_color_triangle(
      0, 0, Color(1, 0, 0), 0, 2, Color(0, 0, 1), 2, 0, Color(0, 1, 0));
    frame.resolve(); frame.pixel(0, 0, Color(.5f, .25f, .25f));
  });

  test("nearest texture corners and center tie", [] {
    Texture t = corners();
    color_near(t.sample_nearest(Vector2D(0, 0)), Color(1, 0, 0));
    color_near(t.sample_nearest(Vector2D(1, 0)), Color(0, 1, 0));
    color_near(t.sample_nearest(Vector2D(0, 1)), Color(0, 0, 1));
    color_near(t.sample_nearest(Vector2D(1, 1)), Color(1, 1, 1));
    color_near(t.sample_nearest(Vector2D(.5, .5)), Color(1, 1, 1));
  });
  test("bilinear texture corners and center average", [] {
    Texture t = corners();
    color_near(t.sample_bilinear(Vector2D(0, 0)), Color(1, 0, 0));
    color_near(t.sample_bilinear(Vector2D(1, 0)), Color(0, 1, 0));
    color_near(t.sample_bilinear(Vector2D(0, 1)), Color(0, 0, 1));
    color_near(t.sample_bilinear(Vector2D(1, 1)), Color(1, 1, 1));
    color_near(t.sample_bilinear(Vector2D(.5, .5)), Color(.5f, .5f, .5f));
    color_near(t.sample_bilinear(Vector2D(.25, .25)), Color(.625f, .25f, .25f));
  });
  test("texture UV clamps to border texels", [] {
    Texture t = corners();
    color_near(t.sample_nearest(Vector2D(-2, 3)), Color(0, 0, 1));
    color_near(t.sample_bilinear(Vector2D(-2, 3)), Color(0, 0, 1));
    color_near(t.sample_bilinear(Vector2D(3, -2)), Color(0, 1, 0));
  });
  test("texture invalid levels and empty data are safe magenta", [] {
    Texture t = corners(), empty;
    color_near(t.sample_nearest(Vector2D(0, 0), -1), Color(1, 0, 1));
    color_near(t.sample_bilinear(Vector2D(0, 0), 1), Color(1, 0, 1));
    color_near(empty.sample(params()), Color(1, 0, 1));
    t.mipmap[0].texels.clear();
    color_near(t.sample_nearest(Vector2D(.5, .5)), Color(1, 0, 1));
    color_near(t.sample_bilinear(Vector2D(.5, .5)), Color(1, 0, 1));
  });
  test("single texel texture for every UV", [] {
    Texture t; t.init(std::vector<unsigned char>{255, 0, 255}, 1, 1);
    require(t.mipmap.size() == 1, "1x1 mip count");
    color_near(t.sample_nearest(Vector2D(1, 1)), Color(1, 0, 1));
    color_near(t.sample_bilinear(Vector2D(.37, .84)), Color(1, 0, 1));
  });
  test("mipmap 4x4 checker reaches populated 1x1 average", [] {
    std::vector<unsigned char> pixels(4 * 4 * 3);
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x)
      for (int c = 0; c < 3; ++c) pixels[3 * (4 * y + x) + c] = (x + y) % 2 ? 255 : 0;
    Texture t; t.init(pixels, 4, 4);
    require(t.mipmap.size() == 3, "4x4 requires levels 4,2,1");
    require(t.mipmap[2].width == 1 && t.mipmap[2].height == 1, "last dimensions");
    color_near(t.sample_nearest(Vector2D(.5, .5), 2), Color(.5f, .5f, .5f), 1.01 / 255.0);
  });
  test("odd non-square mipmap preserves constant field", [] {
    Texture t; t.init(solid_mip(7, 5, 255, 0, 0).texels, 7, 5);
    require(t.mipmap.size() == 3, "7x5 requires levels 7x5,3x2,1x1");
    require(t.mipmap[1].width == 3 && t.mipmap[1].height == 2, "middle dimensions");
    require(t.mipmap[2].width == 1 && t.mipmap[2].height == 1, "last dimensions");
    for (int level = 0; level < 3; ++level)
      color_near(t.sample_bilinear(Vector2D(.61, .73), level), Color(1, 0, 0));
  });
  test("one-dimensional mipmap 1x8 preserves constant field", [] {
    Texture t; t.init(solid_mip(1, 8, 255, 255, 255).texels, 1, 8);
    require(t.mipmap.size() == 4, "1x8 requires four levels");
    for (size_t level = 0; level < t.mipmap.size(); ++level) {
      require(t.mipmap[level].width == 1, "width must remain 1");
      color_near(t.sample_bilinear(Vector2D(1, .6), static_cast<int>(level)), Color::White);
    }
  });
  test("mipmap regeneration from nonzero starting level", [] {
    Texture t = colored_levels();
    t.mipmap[1] = solid_mip(4, 4, 255, 255, 255);
    t.generate_mips(1);
    require(t.mipmap.size() == 4, "nonzero start count");
    color_near(t.sample_nearest(Vector2D(.5, .5), 0), Color(1, 0, 0));
    color_near(t.sample_nearest(Vector2D(.5, .5), 2), Color::White);
    color_near(t.sample_nearest(Vector2D(.5, .5), 3), Color::White);
  });

  test("LOD uses Euclidean texel footprint and max derivative", [] {
    Texture t = colored_levels();
    SampleParams sp = params(Vector2D(0, 0));
    sp.p_dx_uv = Vector2D(.25, .25);  // sqrt(2^2+2^2), log2 = 1.5.
    sp.p_dy_uv = Vector2D(.125, 0);
    near(t.get_level(sp), 1.5, 1e-5, "diagonal footprint");
    sp.p_dx_uv = Vector2D(.125, 0);
    sp.p_dy_uv = Vector2D(0, .5);    // max footprint 4, level 2.
    near(t.get_level(sp), 2, 1e-5, "y derivative maximum");
  });
  test("LOD respects separate width and height", [] {
    Texture t; t.init(solid_mip(8, 4, 255, 255, 255).texels, 8, 4);
    SampleParams sp = params(Vector2D(0, 0));
    sp.p_dx_uv = Vector2D(.25, 0); sp.p_dy_uv = Vector2D(0, .25);
    near(t.get_level(sp), 1, 1e-5, "non-square footprint");
  });
  test("LOD magnification and excessive minification clamp", [] {
    Texture t = colored_levels();
    SampleParams sp = params();
    near(t.get_level(sp), 0, 1e-5, "constant UV");
    sp.p_dx_uv = sp.p_uv + Vector2D(.03125, 0);
    near(t.get_level(sp), 0, 1e-5, "magnification");
    sp.p_dx_uv = sp.p_uv + Vector2D(100, 0);
    near(t.get_level(sp), 3, 1e-5, "last level clamp");
  });

  for (int pixel = 0; pixel < 2; ++pixel) for (int level = 0; level < 3; ++level) {
    test("texture sampling combination p=" + std::to_string(pixel) + " l=" + std::to_string(level), [=] {
      Texture t = corners();
      t.mipmap.push_back(solid_mip(1, 1, 255, 255, 0));
      SampleParams sp = params();
      sp.psm = static_cast<PixelSampleMethod>(pixel);
      sp.lsm = static_cast<LevelSampleMethod>(level);
      // A footprint of 2^.75 yields unambiguous nearest level 1, linear weight .75.
      sp.p_dx_uv = sp.p_uv + Vector2D(std::pow(2.0, .75) / 2.0, 0);
      const Color base = pixel == 0 ? Color(1, 0, 0) : Color(.625f, .25f, .25f);
      const Color coarse(1, 1, 0);
      const Color expected = level == 0 ? base : (level == 1 ? coarse : .25f * base + .75f * coarse);
      color_near(t.sample(sp), expected);
    });
  }
  test("textured triangle interpolates UV analytically", [] {
    Frame frame(2, 2); frame.raster.set_psm(P_LINEAR);
    Texture t = corners();
    frame.raster.rasterize_textured_triangle(0, 0, 0, 0, 2, 0, 1, 0, 0, 2, 0, 1, t);
    frame.resolve(); frame.pixel(0, 0, Color(.625f, .25f, .25f));
  });
  test("textured SSAA derivatives remain one screen pixel", [] {
    Frame frame(4, 4, 9); frame.raster.set_lsm(L_NEAREST);
    Texture t = colored_levels();
    // UV=x/4,y/4; an 8x8 texture covers exactly 2 texels per screen pixel.
    frame.raster.rasterize_textured_triangle(0, 0, 0, 0, 4, 0, 1, 0, 0, 4, 0, 1, t);
    frame.resolve(); frame.pixel(0, 0, Color(0, 1, 0));
  });

  std::cout << "RESULT " << passed << " passed, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
