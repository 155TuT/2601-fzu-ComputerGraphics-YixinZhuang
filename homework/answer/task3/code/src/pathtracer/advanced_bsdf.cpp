#include "bsdf.h"

#include <algorithm>
#include <iostream>
#include <utility>

#include "application/visual_debugger.h"

using std::max;
using std::min;
using std::swap;

namespace CGL {

// Mirror BSDF //

Vector3D MirrorBSDF::f(const Vector3D wo, const Vector3D wi) {
  return Vector3D();
}

Vector3D MirrorBSDF::sample_f(const Vector3D wo, Vector3D* wi, double* pdf) {
  reflect(wo, wi);
  *pdf = 1.0;
  const double cosine = abs_cos_theta(*wi);
  return cosine > 0 ? reflectance / cosine : Vector3D();
}

void MirrorBSDF::render_debugger_node()
{
  if (ImGui::TreeNode(this, "Mirror BSDF"))
  {
    DragDouble3("Reflectance", &reflectance[0], 0.005);
    ImGui::TreePop();
  }
}

// Microfacet BSDF //

double MicrofacetBSDF::G(const Vector3D wo, const Vector3D wi) {
  return 1.0 / (1.0 + Lambda(wi) + Lambda(wo));
}

double MicrofacetBSDF::D(const Vector3D h) {
  if (h.z <= 0) return 0;
  const double roughness = max(alpha, 1e-4);
  const double cos2 = h.z * h.z;
  const double tan2 = max(0.0, 1.0 - cos2) / cos2;
  return exp(-tan2 / (roughness * roughness)) /
         (PI * roughness * roughness * cos2 * cos2);
}

Vector3D MicrofacetBSDF::F(const Vector3D wi) {
  // wi.z is the cosine relative to the microfacet normal.
  const double cosine = clamp(abs_cos_theta(wi), 0.0, 1.0);
  if (cosine == 0) return Vector3D(1.0);
  const double cos2 = cosine * cosine, sin2 = 1.0 - cos2;
  Vector3D result;
  for (int channel = 0; channel < 3; ++channel) {
    const double eta2 = eta[channel] * eta[channel];
    const double k2 = k[channel] * k[channel];
    const double t = eta2 - k2 - sin2;
    const double magnitude = sqrt(t * t + 4 * eta2 * k2);
    const double real_part = sqrt(max(0.0, (magnitude + t) * 0.5));
    const double twice = 2 * cosine * real_part;
    const double rs = (magnitude + cos2 - twice) / (magnitude + cos2 + twice);
    const double numerator = cos2 * magnitude + sin2 * sin2;
    const double rp = rs * (numerator - twice * sin2) / (numerator + twice * sin2);
    result[channel] = clamp(0.5 * (rs + rp), 0.0, 1.0);
  }
  return result;
}

Vector3D MicrofacetBSDF::f(const Vector3D wo, const Vector3D wi) {
  if (wo.z <= 0 || wi.z <= 0) return Vector3D();
  const Vector3D half = (wo + wi).unit();
  const double cosine = clamp(dot(wi, half), 0.0, 1.0);
  return F(Vector3D(0, 0, cosine)) *
         (D(half) * G(wo, wi) / (4 * wo.z * wi.z));
}

Vector3D MicrofacetBSDF::sample_f(const Vector3D wo, Vector3D* wi, double* pdf) {
  *pdf = 0;
  *wi = Vector3D();
  if (wo.z <= 0) return Vector3D();
  const Vector2D sample = sampler.get_sample();
  const double roughness = max(alpha, 1e-4);
  const double tan2 = -roughness * roughness * log1p(-sample.x);
  const double cosine = 1.0 / sqrt(1.0 + tan2);
  const double sine = sqrt(max(0.0, 1.0 - cosine * cosine));
  const double phi = 2 * PI * sample.y;
  const Vector3D half(sine * cos(phi), sine * sin(phi), cosine);
  const double projected = dot(wo, half);
  if (projected <= 0) return Vector3D();
  *wi = 2 * projected * half - wo;
  if (wi->z <= 0) return Vector3D();
  // Transform the NDF's projected-area density to a reflected-direction density.
  *pdf = D(half) * half.z / (4 * projected);
  return f(wo, *wi);
}

void MicrofacetBSDF::render_debugger_node()
{
  if (ImGui::TreeNode(this, "Micofacet BSDF"))
  {
    DragDouble3("eta", &eta[0], 0.005);
    DragDouble3("K", &k[0], 0.005);
    DragDouble("alpha", &alpha, 0.005);
    ImGui::TreePop();
  }
}

// Refraction BSDF //

Vector3D RefractionBSDF::f(const Vector3D wo, const Vector3D wi) {
  return Vector3D();
}

Vector3D RefractionBSDF::sample_f(const Vector3D wo, Vector3D* wi, double* pdf) {
  *pdf = 0;
  *wi = Vector3D();
  if (!refract(wo, wi, ior)) return Vector3D();
  const double ratio = wo.z >= 0 ? 1.0 / ior : ior;
  *pdf = 1.0;
  // Camera paths transport radiance; the two interface factors cancel on exit.
  const double cosine = abs_cos_theta(*wi);
  return cosine > 0 ? transmittance * (ratio * ratio / cosine) : Vector3D();
}

void RefractionBSDF::render_debugger_node()
{
  if (ImGui::TreeNode(this, "Refraction BSDF"))
  {
    DragDouble3("Transmittance", &transmittance[0], 0.005);
    DragDouble("ior", &ior, 0.005);
    ImGui::TreePop();
  }
}

// Glass BSDF //

Vector3D GlassBSDF::f(const Vector3D wo, const Vector3D wi) {
  return Vector3D();
}

Vector3D GlassBSDF::sample_f(const Vector3D wo, Vector3D* wi, double* pdf) {
  Vector3D transmitted;
  if (!refract(wo, &transmitted, ior)) {
    reflect(wo, wi);
    *pdf = 1.0;
    return abs_cos_theta(*wi) > 0 ? reflectance / abs_cos_theta(*wi) : Vector3D();
  }
  const double eta_i = wo.z >= 0 ? 1.0 : ior;
  const double eta_t = wo.z >= 0 ? ior : 1.0;
  const double cos_i = abs_cos_theta(wo), cos_t = abs_cos_theta(transmitted);
  const double rs = (eta_i * cos_i - eta_t * cos_t) / (eta_i * cos_i + eta_t * cos_t);
  const double rp = (eta_t * cos_i - eta_i * cos_t) / (eta_t * cos_i + eta_i * cos_t);
  const double fresnel = clamp(0.5 * (rs * rs + rp * rp), 0.0, 1.0);
  if (coin_flip(fresnel)) {
    reflect(wo, wi);
    *pdf = fresnel;
    return reflectance * (fresnel / abs_cos_theta(*wi));
  }
  *wi = transmitted;
  *pdf = 1.0 - fresnel;
  const double ratio = eta_i / eta_t;
  return transmittance * ((1.0 - fresnel) * ratio * ratio / abs_cos_theta(*wi));
}

void GlassBSDF::render_debugger_node()
{
  if (ImGui::TreeNode(this, "Refraction BSDF"))
  {
    DragDouble3("Reflectance", &reflectance[0], 0.005);
    DragDouble3("Transmittance", &transmittance[0], 0.005);
    DragDouble("ior", &ior, 0.005);
    ImGui::TreePop();
  }
}

void BSDF::reflect(const Vector3D wo, Vector3D* wi) {
  *wi = Vector3D(-wo.x, -wo.y, wo.z);
}

bool BSDF::refract(const Vector3D wo, Vector3D* wi, double ior) {
  if (!(ior > 0) || !std::isfinite(ior)) return false;
  const double ratio = wo.z >= 0 ? 1.0 / ior : ior;
  const double sin2 = ratio * ratio * max(0.0, 1.0 - wo.z * wo.z);
  if (sin2 >= 1.0) return false;
  const double z = sqrt(max(0.0, 1.0 - sin2));
  *wi = Vector3D(-ratio * wo.x, -ratio * wo.y, wo.z >= 0 ? -z : z);
  return true;

}

} // namespace CGL
