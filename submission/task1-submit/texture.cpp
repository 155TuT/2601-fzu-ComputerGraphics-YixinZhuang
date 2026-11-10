#include "texture.h"
#include "CGL/color.h"

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace CGL {

  namespace {
    bool valid_mip(const MipLevel& mip) {
      return mip.width > 0 && mip.height > 0 &&
             mip.texels.size() >= 3 * mip.width * mip.height;
    }

    double clamp_uv(double value) {
      return std::max(0.0, std::min(1.0, value));
    }
  }

  Color Texture::sample(const SampleParams& sp) {
    if (mipmap.empty()) return Color(1, 0, 1);

    // Pixel filtering and level filtering are independent choices.
    const auto sample_level = [&](int level) -> Color {
      if (sp.psm == P_NEAREST) return sample_nearest(sp.p_uv, level);
      if (sp.psm == P_LINEAR) return sample_bilinear(sp.p_uv, level);
      return Color(1, 0, 1);
    };

    if (sp.lsm == L_ZERO) return sample_level(0);

    const float level = get_level(sp);
    if (sp.lsm == L_NEAREST) {
      return sample_level(static_cast<int>(std::floor(level + 0.5f)));
    }
    if (sp.lsm == L_LINEAR) {
      const int lower = static_cast<int>(std::floor(level));
      const int upper = std::min(lower + 1, static_cast<int>(mipmap.size()) - 1);
      const float weight = level - lower;
      return sample_level(lower) * (1.0f - weight) + sample_level(upper) * weight;
    }

    // Magenta makes an invalid sampling method or texture visible.
    return Color(1, 0, 1);
  }

  float Texture::get_level(const SampleParams& sp) {
    if (mipmap.empty() || !valid_mip(mipmap[0])) return 0.0f;

    // Derivatives are measured one SCREEN pixel away even with supersampling.
    // Scale UV differences into texels of the original, full-resolution image.
    const Vector2D dx = sp.p_dx_uv - sp.p_uv;
    const Vector2D dy = sp.p_dy_uv - sp.p_uv;
    const double w = static_cast<double>(mipmap[0].width);
    const double h = static_cast<double>(mipmap[0].height);
    const double dx_length_squared = dx.x * dx.x * w * w + dx.y * dx.y * h * h;
    const double dy_length_squared = dy.x * dy.x * w * w + dy.y * dy.y * h * h;
    const double footprint = std::sqrt(std::max(dx_length_squared, dy_length_squared));
    const float max_level = static_cast<float>(mipmap.size() - 1);

    // Magnification and a constant UV field use the original image.
    if (std::isnan(footprint) || footprint <= 1.0) return 0.0f;
    if (!std::isfinite(footprint)) return max_level;
    return static_cast<float>(std::min(static_cast<double>(max_level), std::log2(footprint)));
  }

  Color MipLevel::get_texel(int tx, int ty) {
    return Color(&texels[tx * 3 + ty * width * 3]);
  }

  Color Texture::sample_nearest(Vector2D uv, int level) {
    if (level < 0 || static_cast<size_t>(level) >= mipmap.size() ||
        !std::isfinite(uv.x) || !std::isfinite(uv.y)) return Color(1, 0, 1);
    auto& mip = mipmap[level];
    if (!valid_mip(mip)) return Color(1, 0, 1);

    // In this framework UV endpoints map to the first and last texel centers.
    const double x = clamp_uv(uv.x) * (mip.width - 1);
    const double y = clamp_uv(uv.y) * (mip.height - 1);
    const int tx = static_cast<int>(std::floor(x + 0.5));
    const int ty = static_cast<int>(std::floor(y + 0.5));
    return mip.get_texel(tx, ty);
  }

  Color Texture::sample_bilinear(Vector2D uv, int level) {
    if (level < 0 || static_cast<size_t>(level) >= mipmap.size() ||
        !std::isfinite(uv.x) || !std::isfinite(uv.y)) return Color(1, 0, 1);
    auto& mip = mipmap[level];
    if (!valid_mip(mip)) return Color(1, 0, 1);

    const double x = clamp_uv(uv.x) * (mip.width - 1);
    const double y = clamp_uv(uv.y) * (mip.height - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, static_cast<int>(mip.width) - 1);
    const int y1 = std::min(y0 + 1, static_cast<int>(mip.height) - 1);
    const float tx = static_cast<float>(x - x0);
    const float ty = static_cast<float>(y - y0);
    const Color top = mip.get_texel(x0, y0) * (1.0f - tx) + mip.get_texel(x1, y0) * tx;
    const Color bottom = mip.get_texel(x0, y1) * (1.0f - tx) + mip.get_texel(x1, y1) * tx;
    return top * (1.0f - ty) + bottom * ty;
  }



  /****************************************************************************/

  // Helpers

  inline void uint8_to_float(float dst[3], unsigned char* src) {
    uint8_t* src_uint8 = (uint8_t*)src;
    dst[0] = src_uint8[0] / 255.f;
    dst[1] = src_uint8[1] / 255.f;
    dst[2] = src_uint8[2] / 255.f;
  }

  inline void float_to_uint8(unsigned char* dst, float src[3]) {
    uint8_t* dst_uint8 = (uint8_t*)dst;
    // Round instead of truncating: a constant field must stay constant through
    // odd-size filter weights and repeated mip reductions.
    dst_uint8[0] = (uint8_t)std::lround(255.f * max(0.0f, min(1.0f, src[0])));
    dst_uint8[1] = (uint8_t)std::lround(255.f * max(0.0f, min(1.0f, src[1])));
    dst_uint8[2] = (uint8_t)std::lround(255.f * max(0.0f, min(1.0f, src[2])));
  }

  void Texture::generate_mips(int startLevel) {

    // make sure there's a valid texture
    if (startLevel < 0 || static_cast<size_t>(startLevel) >= mipmap.size() ||
        startLevel >= kMaxMipLevels || !valid_mip(mipmap[startLevel])) {
      std::cerr << "Invalid start level or texture" << std::endl;
      return;
    }

    // allocate sublevels
    int baseWidth = mipmap[startLevel].width;
    int baseHeight = mipmap[startLevel].height;
    int numSubLevels = (int)(log2f((float)max(baseWidth, baseHeight)));

    numSubLevels = min(numSubLevels, kMaxMipLevels - startLevel - 1);
    mipmap.resize(startLevel + numSubLevels + 1);

    int width = baseWidth;
    int height = baseHeight;
    for (int i = 1; i <= numSubLevels; i++) {

      MipLevel& level = mipmap[startLevel + i];

      // handle odd size texture by rounding down
      width = max(1, width / 2);
      //assert (width > 0);
      height = max(1, height / 2);
      //assert (height > 0);

      level.width = width;
      level.height = height;
      level.texels = vector<unsigned char>(3 * width * height);
    }

    // create mips
    // Populate EVERY allocated level, including the final 1x1 image.
    for (int mipLevel = startLevel + 1; mipLevel <= startLevel + numSubLevels;
      mipLevel++) {

      MipLevel& prevLevel = mipmap[mipLevel - 1];
      MipLevel& currLevel = mipmap[mipLevel];

      int prevLevelPitch = prevLevel.width * 3; // 32 bit RGB
      int currLevelPitch = currLevel.width * 3; // 32 bit RGB

      unsigned char* prevLevelMem;
      unsigned char* currLevelMem;

      currLevelMem = (unsigned char*)&currLevel.texels[0];
      prevLevelMem = (unsigned char*)&prevLevel.texels[0];

      float wDecimal, wNorm, wWeight[3];
      int wSupport;
      float hDecimal, hNorm, hWeight[3];
      int hSupport;

      float result[3];
      float input[3];

      // conditional differentiates no rounding case from round down case
      if (prevLevel.width & 1) {
        wSupport = 3;
        wDecimal = 1.0f / (float)currLevel.width;
      }
      else {
        wSupport = 2;
        wDecimal = 0.0f;
      }

      // conditional differentiates no rounding case from round down case
      if (prevLevel.height & 1) {
        hSupport = 3;
        hDecimal = 1.0f / (float)currLevel.height;
      }
      else {
        hSupport = 2;
        hDecimal = 0.0f;
      }

      wNorm = 1.0f / (2.0f + wDecimal);
      hNorm = 1.0f / (2.0f + hDecimal);

      // case 1: reduction only in horizontal size (vertical size is 1)
      if (currLevel.height == prevLevel.height) {
        //assert (currLevel.height == 1);

        for (int i = 0; i < currLevel.width; i++) {
          wWeight[0] = wNorm * (1.0f - wDecimal * i);
          wWeight[1] = wNorm * 1.0f;
          wWeight[2] = wNorm * wDecimal * (i + 1);

          result[0] = result[1] = result[2] = 0.0f;

          for (int ii = 0; ii < wSupport; ii++) {
            uint8_to_float(input, prevLevelMem + 3 * (2 * i + ii));
            result[0] += wWeight[ii] * input[0];
            result[1] += wWeight[ii] * input[1];
            result[2] += wWeight[ii] * input[2];
          }

          // convert back to format of the texture
          float_to_uint8(currLevelMem + (3 * i), result);
        }

        // case 2: reduction only in vertical size (horizontal size is 1)
      }
      else if (currLevel.width == prevLevel.width) {
        //assert (currLevel.width == 1);

        for (int j = 0; j < currLevel.height; j++) {
          hWeight[0] = hNorm * (1.0f - hDecimal * j);
          hWeight[1] = hNorm;
          hWeight[2] = hNorm * hDecimal * (j + 1);

          result[0] = result[1] = result[2] = 0.0f;
          for (int jj = 0; jj < hSupport; jj++) {
            uint8_to_float(input, prevLevelMem + prevLevelPitch * (2 * j + jj));
            result[0] += hWeight[jj] * input[0];
            result[1] += hWeight[jj] * input[1];
            result[2] += hWeight[jj] * input[2];
          }

          // convert back to format of the texture
          float_to_uint8(currLevelMem + (currLevelPitch * j), result);
        }

        // case 3: reduction in both horizontal and vertical size
      }
      else {

        for (int j = 0; j < currLevel.height; j++) {
          hWeight[0] = hNorm * (1.0f - hDecimal * j);
          hWeight[1] = hNorm;
          hWeight[2] = hNorm * hDecimal * (j + 1);

          for (int i = 0; i < currLevel.width; i++) {
            wWeight[0] = wNorm * (1.0f - wDecimal * i);
            wWeight[1] = wNorm * 1.0f;
            wWeight[2] = wNorm * wDecimal * (i + 1);

            result[0] = result[1] = result[2] = 0.0f;

            // convolve source image with a trapezoidal filter.
            // in the case of no rounding this is just a box filter of width 2.
            // in the general case, the support region is 3x3.
            for (int jj = 0; jj < hSupport; jj++)
              for (int ii = 0; ii < wSupport; ii++) {
                float weight = hWeight[jj] * wWeight[ii];
                uint8_to_float(input, prevLevelMem +
                  prevLevelPitch * (2 * j + jj) +
                  3 * (2 * i + ii));
                result[0] += weight * input[0];
                result[1] += weight * input[1];
                result[2] += weight * input[2];
              }

            // convert back to format of the texture
            float_to_uint8(currLevelMem + currLevelPitch * j + 3 * i, result);
          }
        }
      }
    }
  }

}
