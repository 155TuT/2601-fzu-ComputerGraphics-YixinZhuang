#include "rasterizer.h"
#include "texture.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace CGL;

namespace {
  const unsigned int kSeed = 20261002;
  volatile uint64_t checksum_sink = 0;
  const RasterizationMethod methods[] = {R_BASELINE, R_INCREMENTAL, R_SIMD};
  const char* method_name(RasterizationMethod method) {
    return method == R_BASELINE ? "direct_edges" : (method == R_INCREMENTAL ? "incremental" : "simd");
  }
  const char* shader_name(int shader) {
    return shader == 0 ? "constant" : (shader == 1 ? "barycentric_color" : "texture");
  }

  struct BenchTriangle {
    float x[3], y[3];
    Color color;
  };
  struct Workload {
    std::string name;
    std::vector<BenchTriangle> triangles;
  };
  struct BenchRenderer {
    size_t width;
    std::vector<unsigned char> pixels;
    RasterizerImp raster;
    Texture texture;
    BenchRenderer(size_t w, unsigned int rate, RasterizationMethod method)
      : width(w), pixels(3 * w * w), raster(P_LINEAR, L_LINEAR, w, w, rate) {
      raster.set_framebuffer_target(pixels.data(), w, w);
      raster.set_rasterization_method(method);
      std::vector<unsigned char> texels(3 * 4 * 4);
      for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        texels[3 * (4 * y + x)] = x % 2 ? 220 : 20;
        texels[3 * (4 * y + x) + 1] = y % 2 ? 180 : 40;
        texels[3 * (4 * y + x) + 2] = (x + y) % 2 ? 140 : 60;
      }
      texture.init(texels, 4, 4);
    }
    void draw(const Workload& workload, int shader) {
      for (const BenchTriangle& t : workload.triangles) {
        if (shader == 0) raster.rasterize_triangle(t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2], t.color);
        else if (shader == 1) raster.rasterize_interpolated_color_triangle(
          t.x[0], t.y[0], Color(.9f, .1f, .2f), t.x[1], t.y[1], Color(.2f, .8f, .1f),
          t.x[2], t.y[2], Color(.1f, .2f, .9f));
        else raster.rasterize_textured_triangle(t.x[0], t.y[0], 0, 0,
          t.x[1], t.y[1], 1, 0, t.x[2], t.y[2], 0, 1, texture);
      }
    }
  };

  uint64_t byte_checksum(const std::vector<unsigned char>& pixels) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (unsigned char c : pixels) { hash ^= c; hash *= UINT64_C(1099511628211); }
    return hash;
  }
  uint64_t covered_samples(const std::vector<Color>& samples) {
    uint64_t count = 0;
    for (const Color& c : samples) if (c != Color::White) ++count;
    return count;
  }

  std::vector<Workload> workloads(size_t width) {
    std::mt19937 random(kSeed);
    std::uniform_real_distribution<float> unit(0, 1);
    const float w = static_cast<float>(width);
    std::vector<Workload> result(4);
    result[0].name = "large"; result[1].name = "tiny";
    result[2].name = "thin"; result[3].name = "clipped";
    for (int i = 0; i < 10; ++i) {
      const float offset = unit(random) * w * .08f;
      result[0].triangles.push_back(BenchTriangle{{offset, .95f * w, .12f * w},
        {.07f * w, .16f * w, .92f * w}, Color(.1f + .07f * i, .2f, .3f)});
    }
    for (int i = 0; i < 512; ++i) {
      const float x = unit(random) * (w - 3), y = unit(random) * (w - 3);
      const float size = .05f + unit(random) * 2.5f;
      result[1].triangles.push_back(BenchTriangle{{x, x + size, x + .17f * size},
        {y, y + .13f * size, y + size}, Color(.2f, .3f, .4f)});
    }
    for (int i = 0; i < 12; ++i) {
      const float y = .2f + unit(random) * w * .1f;
      const float thickness = .01f + unit(random) * 1.25f;
      result[2].triangles.push_back(BenchTriangle{{.17f, w - .31f, w - .62f},
        {y, w - y, w - y + thickness}, Color(.3f, .4f, .2f)});
      const float dx = -.4f * w + unit(random) * w * .3f;
      result[3].triangles.push_back(BenchTriangle{{dx, w * 1.4f, w * .27f},
        {-w * .27f, w * .13f, w * 1.3f}, Color(.15f, .35f, .55f)});
    }
    // Include both winding orders without changing primitive painter order.
    for (Workload& workload : result) for (size_t i = 1; i < workload.triangles.size(); i += 2) {
      std::swap(workload.triangles[i].x[1], workload.triangles[i].x[2]);
      std::swap(workload.triangles[i].y[1], workload.triangles[i].y[2]);
    }
    return result;
  }

  double percentile(std::vector<double> samples, double p) {
    std::sort(samples.begin(), samples.end());
    const double index = (samples.size() - 1) * p;
    const size_t lower = static_cast<size_t>(index), upper = std::min(lower + 1, samples.size() - 1);
    return samples[lower] + (index - lower) * (samples[upper] - samples[lower]);
  }

  // Independent coverage oracle uses long-double endpoint cross products,
  // not the implementation's affine coefficients, traversal or SIMD masks.
  long double cross(float ax, float ay, float bx, float by, long double x, long double y) {
    return (static_cast<long double>(bx) - ax) * (y - ay)
         - (static_cast<long double>(by) - ay) * (x - ax);
  }
  bool oracle_inside(const BenchTriangle& t, long double x, long double y) {
    const long double area = cross(t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2]);
    if (std::abs(area) <= 1e-12L) return false;
    const long double orientation = area > 0 ? 1 : -1;
    const long double tolerance = 16 * std::numeric_limits<double>::epsilon() * std::max(1.0L, std::abs(area));
    for (int i = 0; i < 3; ++i) {
      const int j = (i + 1) % 3;
      if (orientation * cross(t.x[i], t.y[i], t.x[j], t.y[j], x, y) < -tolerance) return false;
    }
    return true;
  }

  uint64_t verify_oracle(std::ofstream& csv) {
    std::vector<BenchTriangle> triangles = {
      {{0, 24, 0}, {0, 0, 24}, Color::Black},
      {{.5f, 23.5f, .5f}, {.5f, .5f, 23.5f}, Color::Black},
      {{1.5f, 20.5f, 9.5f}, {8.5f, 8.5f, 20.5f}, Color::Black},
      {{-8, 30, 4}, {-7, 2, 31}, Color::Black},
      {{1.1f, 22.9f, 22.9001f}, {1.3f, 22.8f, 22.8001f}, Color::Black},
      {{2, 12, 22}, {2, 12, 22}, Color::Black},
      {{5, 5, 5}, {5, 5, 5}, Color::Black},
      {{-6, -2, -3}, {-6, -3, -2}, Color::Black}
    };
    std::mt19937 random(kSeed);
    std::uniform_int_distribution<int> quarter(-32, 128);
    for (int i = 0; i < 64; ++i) {
      BenchTriangle t;
      for (int v = 0; v < 3; ++v) { t.x[v] = quarter(random) * .25f; t.y[v] = quarter(random) * .25f; }
      t.color = Color::Black; triangles.push_back(t);
    }
    const unsigned int rates[] = {1, 4, 9, 16};
    uint64_t errors = 0;
    for (unsigned int rate : rates) for (RasterizationMethod method : methods) {
      BenchRenderer renderer(24, rate, method);
      const unsigned int side = static_cast<unsigned int>(std::sqrt(rate));
      for (size_t triangle = 0; triangle < triangles.size(); ++triangle) {
        Workload one{"oracle", std::vector<BenchTriangle>{triangles[triangle]}};
        renderer.raster.clear_buffers(); renderer.draw(one, 0);
        const auto& samples = renderer.raster.get_sample_buffer();
        uint64_t wrong = 0, expected_count = 0;
        for (size_t y = 0; y < 24; ++y) for (size_t x = 0; x < 24; ++x)
          for (unsigned int sy = 0; sy < side; ++sy) for (unsigned int sx = 0; sx < side; ++sx) {
            const long double px = x + (sx + .5L) / side, py = y + (sy + .5L) / side;
            const bool expected = oracle_inside(triangles[triangle], px, py);
            const size_t index = (y * 24 + x) * rate + sy * side + sx;
            const bool actual = samples[index] == Color::Black;
            expected_count += expected; wrong += expected != actual;
          }
        errors += wrong;
        csv << triangle << ',' << rate << ',' << method_name(method) << ','
            << expected_count << ',' << covered_samples(samples) << ',' << wrong << '\n';
      }
    }
    return errors;
  }

  uint64_t verify_equivalence(const std::vector<Workload>& cases, size_t width, std::ofstream& csv) {
    const unsigned int rates[] = {1, 4, 9, 16};
    uint64_t failures = 0;
    for (unsigned int rate : rates) for (const Workload& workload : cases) for (int shader = 0; shader < 3; ++shader) {
      BenchRenderer baseline(width, rate, R_BASELINE), scalar(width, rate, R_INCREMENTAL);
      baseline.raster.clear_buffers(); baseline.draw(workload, shader); baseline.raster.resolve_to_framebuffer();
      scalar.raster.clear_buffers(); scalar.draw(workload, shader); scalar.raster.resolve_to_framebuffer();
      for (RasterizationMethod method : std::vector<RasterizationMethod>{R_INCREMENTAL, R_SIMD}) {
        BenchRenderer candidate(width, rate, method);
        candidate.raster.clear_buffers(); candidate.draw(workload, shader); candidate.raster.resolve_to_framebuffer();
        for (int reference_kind = 0; reference_kind < 2; ++reference_kind) {
          const BenchRenderer& reference = reference_kind == 0 ? baseline : scalar;
          const auto& expected = reference.raster.get_sample_buffer();
          const auto& actual = candidate.raster.get_sample_buffer();
          uint64_t pixel_differences = 0, coverage_differences = 0, sample_differences = 0;
          double maximum_error = 0;
          for (size_t pixel = 0; pixel < width * width; ++pixel) {
            bool different = false;
            for (int channel = 0; channel < 3; ++channel)
              different = different || reference.pixels[3 * pixel + channel] != candidate.pixels[3 * pixel + channel];
            pixel_differences += different;
          }
          for (size_t sample = 0; sample < actual.size(); ++sample) {
            coverage_differences += (expected[sample] != Color::White) != (actual[sample] != Color::White);
            sample_differences += expected[sample] != actual[sample];
            for (int channel = 0; channel < 3; ++channel)
              maximum_error = std::max(maximum_error, static_cast<double>(std::abs(expected[sample][channel] - actual[sample][channel])));
          }
          // Pixel/coverage equality is mandatory for every path. SIMD must also
          // preserve every internal float sample bit relative to incremental.
          if (pixel_differences || coverage_differences ||
              (reference_kind == 1 && method == R_SIMD && sample_differences)) ++failures;
          csv << workload.name << ',' << shader_name(shader) << ',' << rate << ','
              << (reference_kind == 0 ? "direct_edges" : "incremental") << ',' << method_name(method) << ','
              << pixel_differences << ',' << coverage_differences << ',' << sample_differences << ','
              << maximum_error << ',' << byte_checksum(candidate.pixels) << '\n';
        }
      }
    }
    return failures;
  }
}

int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 4) {
      std::cerr << "Usage: rasterizer_benchmark output_prefix [width=128] [repetitions=7]\n";
      return 2;
    }
    const std::string prefix = argv[1];
    const size_t width = argc > 2 ? static_cast<size_t>(std::stoul(argv[2])) : 128;
    const int repetitions = argc > 3 ? std::stoi(argv[3]) : 7;
    if (width < 24 || width > 2048 || repetitions < 3 || repetitions > 101)
      throw std::invalid_argument("width must be 24..2048 and repetitions 3..101");
    std::ofstream raw(prefix + "_raw.csv"), summary(prefix + "_summary.csv"),
      equivalence(prefix + "_equivalence.csv"), geometry(prefix + "_geometry.csv"), oracle(prefix + "_oracle.csv");
    if (!raw || !summary || !equivalence || !geometry || !oracle)
      throw std::runtime_error("Cannot open output CSV files; create destination directory first");
    raw << "case,width,spp,method,backend,repeat,rasterize_ms,rgb_checksum,covered_samples\n";
    summary << "case,width,spp,method,backend,repetitions,min_ms,p10_ms,median_ms,p90_ms,max_ms,speedup_vs_direct,speedup_vs_incremental\n";
    equivalence << "case,shader,spp,reference,candidate,different_pixels,different_coverage_samples,different_float_samples,max_float_error,rgb_checksum\n";
    geometry << "case,triangle,x0,y0,x1,y1,x2,y2,r,g,b\n";
    oracle << "triangle,spp,method,expected_covered,actual_covered,wrong_samples\n";
    raw << std::setprecision(10); summary << std::setprecision(10);
    equivalence << std::setprecision(10); geometry << std::setprecision(9);
    const auto cases = workloads(width);
    BenchRenderer capability(width, 1, R_SIMD);
    std::cout << "Backend: " << capability.raster.get_simd_backend() << "; seed=" << kSeed
              << "; width=" << width << "; repetitions=" << repetitions << '\n';
    std::cout << "Timing includes actual triangle setup, coverage, constant shader and sample-buffer writes;"
                 " excludes clear, resolve, allocation, SVG parsing, GUI and CSV/hash work.\n";
    for (const Workload& workload : cases) for (size_t i = 0; i < workload.triangles.size(); ++i) {
      const BenchTriangle& t = workload.triangles[i];
      geometry << workload.name << ',' << i;
      for (int v = 0; v < 3; ++v) geometry << ',' << t.x[v] << ',' << t.y[v];
      geometry << ',' << t.color.r << ',' << t.color.g << ',' << t.color.b << '\n';
    }
    const uint64_t oracle_errors = verify_oracle(oracle);
    const uint64_t equivalence_errors = verify_equivalence(cases, width, equivalence);
    std::cout << "Independent endpoint-cross-product oracle wrong samples: " << oracle_errors << '\n';
    std::cout << "Pipeline equivalence failed comparisons: " << equivalence_errors << '\n';
    if (oracle_errors || equivalence_errors) {
      std::cerr << "Correctness failed; timing skipped. Inspect oracle/equivalence CSV.\n";
      return 1;
    }
    const unsigned int rates[] = {1, 4, 16};
    for (unsigned int rate : rates) for (const Workload& workload : cases) {
      std::vector<std::vector<double>> timings(3);
      BenchRenderer direct(width, rate, R_BASELINE), incremental(width, rate, R_INCREMENTAL), simd(width, rate, R_SIMD);
      BenchRenderer* renderers[] = {&direct, &incremental, &simd};
      for (int warm = 0; warm < 2; ++warm) for (BenchRenderer* renderer : renderers) {
        renderer->raster.clear_buffers(); renderer->draw(workload, 0);
      }
      for (int repeat = 0; repeat < repetitions; ++repeat) for (int order = 0; order < 3; ++order) {
        // Rotate execution order so a fixed method does not always run hot/cold.
        const int index = (repeat + order) % 3;
        BenchRenderer& renderer = *renderers[index];
        renderer.raster.clear_buffers();
        const auto begin = std::chrono::steady_clock::now();
        renderer.draw(workload, 0);
        const auto end = std::chrono::steady_clock::now();
        const double milliseconds = std::chrono::duration<double, std::milli>(end - begin).count();
        renderer.raster.resolve_to_framebuffer();
        const uint64_t checksum = byte_checksum(renderer.pixels);
        checksum_sink ^= checksum;
        timings[index].push_back(milliseconds);
        raw << workload.name << ',' << width << ',' << rate << ',' << method_name(methods[index]) << ','
            << (index == 2 ? renderer.raster.get_simd_backend() : "scalar") << ',' << repeat << ','
            << milliseconds << ',' << checksum << ',' << covered_samples(renderer.raster.get_sample_buffer()) << '\n';
      }
      const double direct_median = percentile(timings[0], .5), scalar_median = percentile(timings[1], .5);
      for (int i = 0; i < 3; ++i) {
        const double median = percentile(timings[i], .5);
        summary << workload.name << ',' << width << ',' << rate << ',' << method_name(methods[i]) << ','
                << (i == 2 ? simd.raster.get_simd_backend() : "scalar") << ',' << repetitions << ','
                << percentile(timings[i], 0) << ',' << percentile(timings[i], .1) << ',' << median << ','
                << percentile(timings[i], .9) << ',' << percentile(timings[i], 1) << ','
                << direct_median / median << ',' << scalar_median / median << '\n';
      }
      std::cout << workload.name << " spp=" << rate << " SIMD median=" << percentile(timings[2], .5)
                << " ms, vs direct=" << direct_median / percentile(timings[2], .5)
                << "x, vs incremental=" << scalar_median / percentile(timings[2], .5) << "x\n";
    }
    std::cout << "PASS: zero coverage/pixel differences; SIMD internal samples equal incremental; CSV prefix "
              << prefix << "; checksum sink=" << checksum_sink << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << '\n';
    return 2;
  }
}
