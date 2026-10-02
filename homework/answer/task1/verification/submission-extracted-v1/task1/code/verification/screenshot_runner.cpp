#include "GL/glew.h"
#include "drawrend.h"
#include "svgparser.h"
#include <iostream>
#include <cstdlib>

// Exercise the real GUI callbacks and S screenshot path in an OpenGL window.
// Hidden only to keep the reproducible report run from covering the desktop.
int main(int argc, char** argv) {
  if (argc != 10) {
    std::cerr << "Usage: screenshot_runner svg spp psm lsm inspector cursor_x cursor_y zoom width\n";
    return 2;
  }
  CGL::SVG svg;
  if (CGL::SVGParser::load(argv[1], &svg) < 0) return 3;
  if (!glfwInit()) return 4;
  const int size = std::atoi(argv[9]);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(size, size, "Rasterizer verification", nullptr, nullptr);
  if (!window) { glfwTerminate(); return 5; }
  glfwMakeContextCurrent(window);
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) return 6;
  // Extension probing can leave a driver error; establish a clean baseline
  // before checking the application's actual drawing and capture operations.
  while (glGetError() != GL_NO_ERROR) {}
  std::cout << "OpenGL " << glGetString(GL_VERSION) << " / " << glGetString(GL_RENDERER) << '\n';
  {
    CGL::DrawRend app(std::vector<CGL::SVG*>{&svg});
    app.init();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    app.resize(width, height);
    const int samples = std::atoi(argv[2]);
    while (app.software_rasterizer->get_sample_rate() < static_cast<unsigned>(samples))
      app.keyboard_event('=', EVENT_PRESS, 0);
    for (int i = 0; i < std::atoi(argv[3]); ++i) app.keyboard_event('P', EVENT_PRESS, 0);
    for (int i = 0; i < std::atoi(argv[4]); ++i) app.keyboard_event('L', EVENT_PRESS, 0);
    const float zoom = std::atof(argv[8]);
    if (zoom != 1) { app.move_view(0, 0, zoom); app.redraw(); }
    app.cursor_event(std::atof(argv[6]), std::atof(argv[7]));
    if (std::atoi(argv[5])) app.keyboard_event('Z', EVENT_PRESS, 0);
    std::cout << app.info() << '\n';
    // Exactly the same entry point used when the user presses S.
    app.keyboard_event('S', EVENT_PRESS, 0);
    glFinish();
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) { std::cerr << "OpenGL error " << error << '\n'; return 7; }
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
