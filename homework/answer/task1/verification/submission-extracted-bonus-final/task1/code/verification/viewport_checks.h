#pragma once

#include "transforms.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

// Numerical checks exercise the same public event handlers as the real Viewer,
// with a live OpenGL context. They are automated, not manual keyboard evidence.
inline int verify_viewport(CGL::DrawRend& app, GLFWwindow* window,
                           const CGL::SVG& first_svg) {
  using namespace CGL;
  int passed = 0, failed = 0;
  const auto require = [](bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
  };
  const auto near = [&](double actual, double expected, double tolerance = 0.002) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
            "Viewport numerical mismatch");
  };
  const auto matrix_near = [&](const Matrix3x3& actual, const Matrix3x3& expected) {
    for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x)
      near(actual(y, x), expected(y, x), 0.001);
  };
  const auto test = [&](const std::string& name, const std::function<void()>& run) {
    try {
      run();
      require(glGetError() == GL_NO_ERROR, "OpenGL error during viewport check");
      ++passed;
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
  };
  const auto key = [&](int code) { app.keyboard_event(code, EVENT_PRESS, 0); };
  const Matrix3x3 initial = app.get_view_transform();
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  const Vector2D center(first_svg.width / 2, first_svg.height / 2);
  const Vector2D point = center + Vector2D(40, 0);
  const Vector2D initial_point = initial * point;
  const double initial_radius = (initial_point - Vector2D(width / 2, height / 2)).norm();

  test("default view center maps to framebuffer center", [&] {
    const Vector2D mapped = initial * center;
    near(mapped.x, width / 2); near(mapped.y, height / 2);
    near(app.get_view_angle(), 0);
  });
  test("E rotates clockwise 15 degrees around the fixed center", [&] {
    key('E');
    near(app.get_view_angle(), 15);
    const Vector2D mapped_center = app.get_view_transform() * center;
    near(mapped_center.x, width / 2); near(mapped_center.y, height / 2);
    const Vector2D mapped = app.get_view_transform() * point;
    const double radians = 15 * std::acos(-1.0) / 180;
    near(mapped.x - mapped_center.x, initial_radius * std::cos(radians));
    near(mapped.y - mapped_center.y, initial_radius * std::sin(radians));
    near((mapped - mapped_center).norm(), initial_radius);
  });
  test("Q undoes E without changing scale or center", [&] {
    key('Q'); near(app.get_view_angle(), 0);
    matrix_near(app.get_view_transform(), initial);
  });
  test("24 E steps restore a complete 360 degree turn", [&] {
    for (int i = 0; i < 24; ++i) key('E');
    near(app.get_view_angle(), 0);
    matrix_near(app.get_view_transform(), initial);
  });
  test("rotated dragging follows the screen displacement", [&] {
    key('E'); key('E');
    const Vector2D before = app.get_view_transform() * point;
    app.cursor_event(200, 200);
    app.mouse_event(MOUSE_LEFT, EVENT_PRESS, 0);
    app.cursor_event(225, 185);
    app.mouse_event(MOUSE_LEFT, EVENT_RELEASE, 0);
    const Vector2D after = app.get_view_transform() * point;
    near(after.x - before.x, 25); near(after.y - before.y, -15);
    near(app.get_view_angle(), 30);
  });
  test("zoom preserves the rotated center and scales its radius", [&] {
    const Matrix3x3 before = app.get_view_transform();
    const Vector2D screen_center(width / 2, height / 2);
    const Vector2D view_center = before.inv() * screen_center;
    const double radius = (before * point - screen_center).norm();
    app.scroll_event(0, 2); // Existing scale factor: 1 + .05*2 = 1.1.
    const Matrix3x3 after = app.get_view_transform();
    const Vector2D mapped_center = after * view_center;
    near(mapped_center.x, screen_center.x); near(mapped_center.y, screen_center.y);
    near((after * point - screen_center).norm(), radius / 1.1);
    near(app.get_view_angle(), 30);
  });
  const Matrix3x3 first_saved = app.get_view_transform();
  Matrix3x3 second_initial, second_saved;
  test("a second SVG starts with an independent unrotated view", [&] {
    key('2'); near(app.get_view_angle(), 0);
    second_initial = app.get_view_transform();
    key('Q'); near(app.get_view_angle(), -15);
    app.move_view(8, -4, .9f); app.redraw();
    second_saved = app.get_view_transform();
  });
  test("switching SVG restores its own rotation pan and zoom", [&] {
    key('1'); near(app.get_view_angle(), 30);
    matrix_near(app.get_view_transform(), first_saved);
    key('2'); near(app.get_view_angle(), -15);
    matrix_near(app.get_view_transform(), second_saved);
  });
  test("Space restores the active SVG and leaves other SVG views intact", [&] {
    key('1'); key(' '); near(app.get_view_angle(), 0);
    matrix_near(app.get_view_transform(), initial);
    key('2'); near(app.get_view_angle(), -15);
    matrix_near(app.get_view_transform(), second_saved);
    key(' '); near(app.get_view_angle(), 0);
    matrix_near(app.get_view_transform(), second_initial);
    key('1');
  });
  test("rectangular framebuffer keeps rotation isotropic and centered", [&] {
    key('E');
    glfwSetWindowSize(window, 960, 640);
    glfwGetFramebufferSize(window, &width, &height);
    app.resize(width, height);
    const Matrix3x3 transform = app.get_view_transform();
    const Vector2D screen_center = transform * center;
    near(screen_center.x, width / 2); near(screen_center.y, height / 2);
    const double x_radius = (transform * (center + Vector2D(20, 0)) - screen_center).norm();
    const double y_radius = (transform * (center + Vector2D(0, 20)) - screen_center).norm();
    near(x_radius, y_radius); near(app.get_view_angle(), 15);
  });
  test("rectangular rotated drag and resized Space reset are correct", [&] {
    const Vector2D before = app.get_view_transform() * point;
    app.cursor_event(300, 200);
    app.mouse_event(MOUSE_LEFT, EVENT_PRESS, 0);
    app.cursor_event(283, 223);
    app.mouse_event(MOUSE_LEFT, EVENT_RELEASE, 0);
    const Vector2D after = app.get_view_transform() * point;
    near(after.x - before.x, -17); near(after.y - before.y, 23);
    key(' '); near(app.get_view_angle(), 0);
    const Vector2D reset_center = app.get_view_transform() * center;
    near(reset_center.x, width / 2); near(reset_center.y, height / 2);
  });
  test("B cycles regular coverage kernels without changing the view", [&] {
    const Matrix3x3 before = app.get_view_transform();
    const RasterizationMethod original = app.software_rasterizer->get_rasterization_method();
    key('B');
    require(app.software_rasterizer->get_rasterization_method() == (original + 1) % 3,
            "B failed to cycle regular kernel");
    key('B'); key('B');
    require(app.software_rasterizer->get_rasterization_method() == original,
            "B cycle failed to restore original kernel");
    matrix_near(app.get_view_transform(), before);
  });
  test("A switches area integration and preserves controls view and regular kernel", [&] {
    const Matrix3x3 before = app.get_view_transform();
    const RasterizationMethod original = app.software_rasterizer->get_rasterization_method();
    key('=');
    key('A');
    require(app.is_analytic(), "A failed to select analytic coverage");
    require(app.software_rasterizer->get_sample_rate() == 4, "A failed to retain UI sample rate");
    require(app.info().find("saved SSAA rate 4") != std::string::npos,
            "Analytic status incorrectly describes spp as integration density");
    matrix_near(app.get_view_transform(), before);
    key('B'); // Deliberately ignored while analytic integration is active.
    key('P'); key('L'); key('A');
    require(!app.is_analytic(), "A failed to return to regular SSAA");
    require(app.software_rasterizer->get_sample_rate() == 4, "Regular rate was lost");
    require(app.software_rasterizer->get_rasterization_method() == original,
            "A switch or analytic B changed the saved regular kernel");
    require(app.info().find("nearest level, bilinear pixel interpolation") != std::string::npos,
            "Pixel and level control choices were lost");
    matrix_near(app.get_view_transform(), before);
    key('P'); key('L'); key('L'); key('-');
  });
  std::cout << "VIEWPORT_RESULT " << passed << " passed, " << failed << " failed\n";
  return failed == 0 ? 0 : 8;
}
