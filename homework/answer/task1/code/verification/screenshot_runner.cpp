#include "GL/glew.h"
#include "drawrend.h"
#include "svgparser.h"
#include <iostream>
#include <cstdlib>
#include <string>
#include "viewport_checks.h"

namespace {
  // Always release GUI resources, including on GLEW or verification failures.
  struct GlfwResources {
    GLFWwindow* window = nullptr;
    ~GlfwResources() {
      if (window) glfwDestroyWindow(window);
      glfwTerminate();
    }
  };
}

// Exercise the real GUI callbacks and S screenshot path in an OpenGL window.
// Hidden only to keep the reproducible report run from covering the desktop.
int main(int argc, char** argv) {
  const bool viewport_check = argc == 4 && std::string(argv[1]) == "--viewport-check";
  if (!viewport_check && (argc < 10 || argc > 14)) {
    std::cerr << "Usage: screenshot_runner svg spp psm lsm inspector cursor_x cursor_y zoom width [rotation_steps [reset [analytic [kernel]]]]\n"
              << "       screenshot_runner --viewport-check first_svg second_svg\n";
    return 2;
  }
  CGL::SVG svg;
  CGL::SVG second_svg;
  if (CGL::SVGParser::load(argv[viewport_check ? 2 : 1], &svg) < 0) return 3;
  if (viewport_check && CGL::SVGParser::load(argv[3], &second_svg) < 0) return 3;
  if (!glfwInit()) return 4;
  GlfwResources resources;
  const int size = viewport_check ? 800 : std::atoi(argv[9]);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(size, size, "Rasterizer verification", nullptr, nullptr);
  resources.window = window;
  if (!window) return 5;
  glfwMakeContextCurrent(window);
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) return 6;
  // Extension probing can leave a driver error; establish a clean baseline
  // before checking the application's actual drawing and capture operations.
  while (glGetError() != GL_NO_ERROR) {}
  std::cout << "OpenGL " << glGetString(GL_VERSION) << " / " << glGetString(GL_RENDERER) << '\n';
  {
    std::vector<CGL::SVG*> svgs{&svg};
    if (viewport_check) svgs.push_back(&second_svg);
    CGL::DrawRend app(svgs);
    app.init();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    app.resize(width, height);
    if (viewport_check) return verify_viewport(app, window, svg);
    if (argc == 14) app.software_rasterizer->set_rasterization_method(
        static_cast<CGL::RasterizationMethod>(std::atoi(argv[13])));
    if (argc >= 13 && std::atoi(argv[12])) app.keyboard_event('A', EVENT_PRESS, 0);
    const int samples = std::atoi(argv[2]);
    if (samples != 1 && samples != 4 && samples != 9 && samples != 16) return 2;
    while (app.software_rasterizer->get_sample_rate() < static_cast<unsigned>(samples))
      app.keyboard_event('=', EVENT_PRESS, 0);
    for (int i = 0; i < std::atoi(argv[3]); ++i) app.keyboard_event('P', EVENT_PRESS, 0);
    for (int i = 0; i < std::atoi(argv[4]); ++i) app.keyboard_event('L', EVENT_PRESS, 0);
    const float zoom = std::atof(argv[8]);
    if (zoom != 1) { app.move_view(0, 0, zoom); app.redraw(); }
    const int rotation_steps = argc >= 11 ? std::atoi(argv[10]) : 0;
    for (int i = 0; i < std::abs(rotation_steps); ++i)
      app.keyboard_event(rotation_steps < 0 ? 'Q' : 'E', EVENT_PRESS, 0);
    if (argc >= 12 && std::atoi(argv[11])) app.keyboard_event(' ', EVENT_PRESS, 0);
    app.cursor_event(std::atof(argv[6]), std::atof(argv[7]));
    if (std::atoi(argv[5])) app.keyboard_event('Z', EVENT_PRESS, 0);
    std::cout << app.info() << '\n';
    // Exactly the same entry point used when the user presses S.
    app.keyboard_event('S', EVENT_PRESS, 0);
    glFinish();
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) { std::cerr << "OpenGL error " << error << '\n'; return 7; }
  }
  return 0;
}
