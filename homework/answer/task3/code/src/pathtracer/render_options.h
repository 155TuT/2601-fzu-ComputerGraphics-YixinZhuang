#ifndef CGL_RENDER_OPTIONS_H
#define CGL_RENDER_OPTIONS_H
#include <cstdint>
namespace CGL {
struct RenderOptions {
  bool normals=false;
  bool brute_force=false;
  bool roulette=true;
  uint32_t seed=1;
};
inline RenderOptions& render_options() {
  static RenderOptions options;
  return options;
}
}
#endif
