#ifndef CGL_RANDOMUTIL_H
#define CGL_RANDOMUTIL_H
#include <random>
#include <cstdint>
namespace CGL {
inline std::mt19937& random_engine() {
  static thread_local std::mt19937 engine(5489u);
  return engine;
}
inline void seed_random(uint32_t seed) { random_engine().seed(seed); }
inline double random_uniform() {
  return (double(random_engine()())+0.5)/4294967296.0;
}
inline bool coin_flip(double p) { return random_uniform()<p; }
}
#endif
