#include "drawrend.h"
#include "svg.h"
#include "transforms.h"
#include "CGL/misc.h"
#include <iostream>
#include <sstream>
#include "CGL/lodepng.h"
#include "texture.h"
#include <ctime>
#include <cmath>
#include "rasterizer.h"
#include "analytic_rasterizer.h"

using namespace std;

namespace CGL {

struct SVG;


DrawRend::DrawRend(std::vector<SVG*> svgs_)
: software_rasterizer(nullptr), svgs(svgs_), current_svg(0),
  regular_rasterizer(nullptr), analytic_rasterizer(nullptr), use_analytic(false)
{
}


DrawRend::~DrawRend(void) {
  svgs.clear();
  delete regular_rasterizer;
  delete analytic_rasterizer;
}

/**
* Initialize the renderer.
* Set default parameters and initialize the viewing transforms for each tab.
*/
void DrawRend::init() {
  gl = true;

  sample_rate = 1;
  left_clicked = false;
  show_zoom = 0;
  cursor_x = cursor_y = 0;

  svg_to_ndc.resize(svgs.size());
  view_states.resize(svgs.size());
  for (int i = 0; i < svgs.size(); ++i) {
    current_svg = i;
    view_init();
  }
  current_svg = 0;
  psm = P_NEAREST;
  lsm = L_ZERO;
  
  width = height = 0;

  regular_rasterizer = new RasterizerImp(psm, lsm, width, height, sample_rate);
  analytic_rasterizer = new AnalyticRasterizer(psm, lsm, width, height, sample_rate);
  use_analytic = false;
  software_rasterizer = regular_rasterizer;
}

/**
* Draw content.
* Simply reposts the framebuffer and the zoom window, if applicable.
*/
void DrawRend::render() {
  draw_pixels();
  if (show_zoom)
    draw_zoom();
}

/**
 * Respond to buffer resize.
 * Resizes the buffers and resets the
 * normalized device coords -> screen coords transform.
 * \param w The new width of the context
 * \param h The new height of the context
 */
void DrawRend::resize(size_t w, size_t h) {
  width = w; height = h;

  framebuffer.resize(3 * w * h);

  float scale = min(width, height);
  ndc_to_screen(0, 0) = scale; ndc_to_screen(0, 2) = (width - scale) / 2;
  ndc_to_screen(1, 1) = scale; ndc_to_screen(1, 2) = (height - scale) / 2;
  
  regular_rasterizer->set_framebuffer_target(framebuffer.data(), width, height);
  analytic_rasterizer->set_framebuffer_target(framebuffer.data(), width, height);

  redraw();
}

/**
 * Return a brief description of the renderer.
 * Displays current buffer resolution, sampling method, sampling rate.
 */
static const string level_strings[] = { "level zero", "nearest level", "bilinear level interpolation" };
static const string pixel_strings[] = { "nearest pixel", "bilinear pixel interpolation" };
std::string DrawRend::info() {
  stringstream ss;
  stringstream sample_method;
  sample_method << level_strings[lsm] << ", " << pixel_strings[psm];
  ss << "Resolution " << width << " x " << height << ". ";
  ss << "Using " << sample_method.str() << " sampling. ";
  if (use_analytic) {
    ss << "Analytic area integration; saved SSAA rate " << sample_rate << ". ";
  } else {
    ss << "Supersample rate " << sample_rate << " per pixel. ";
    static const char* method_names[] = { "baseline", "incremental", "SIMD" };
    const RasterizationMethod method = regular_rasterizer->get_rasterization_method();
    ss << "Kernel " << method_names[method];
    if (method == R_SIMD) ss << " / " << regular_rasterizer->get_simd_backend();
    ss << " (B). ";
  }
  ss << "View rotation " << get_view_angle() << " degrees (Q/E). ";
  ss << "AA " << (use_analytic ? "analytic coverage" : "regular SSAA") << " (A). ";
  return ss.str();
}

/**
 * Respond to cursor events.
 * The viewer itself does not really care about the cursor but it will take
 * the GLFW cursor events and forward the ones that matter to  the renderer.
 * The arguments are defined in screen space coordinates ( (0,0) at top
 * left corner of the window and (w,h) at the bottom right corner.
 * \param x the x coordinate of the cursor
 * \param y the y coordinate of the cursor
 */
void DrawRend::cursor_event(float x, float y) {
  // translate when left mouse button is held down
  if (left_clicked && width != 0 && height != 0) {
    // Convert a screen-pixel drag through the inverse view linear transform.
    // This keeps content moving with the cursor after rotation and zoom, and
    // uses the actual isotropic scale in rectangular windows.
    const ViewState& view = view_states[current_svg];
    const Vector2D svg_delta = rotate(-view.angle)
                            * Vector2D(x - cursor_x, y - cursor_y);
    const double units_per_pixel = 2.0 * view.span / std::min(width, height);
    move_view(svg_delta.x * units_per_pixel, svg_delta.y * units_per_pixel, 1);
    redraw();
  }

  // register new cursor location
  cursor_x = x;
  cursor_y = y;
}

/**
 * Respond to zoom event.
 * Like cursor events, the viewer itself does not care about the mouse wheel
 * either, but it will take the GLFW wheel events and forward them directly
 * to the renderer.
 * \param offset_x Scroll offset in x direction
 * \param offset_y Scroll offset in y direction
 */
void DrawRend::scroll_event(float offset_x, float offset_y) {
  if (offset_x || offset_y) {
    float scale = 1 + 0.05 * (offset_x + offset_y);
    scale = std::min(1.5f, std::max(0.5f, scale));
    move_view(0, 0, scale);
    redraw();
  }
}

/**
 * Respond to mouse click event.
 * The viewer will always forward mouse click events to the renderer.
 * \param key The key that spawned the event. The mapping between the
 *        key values and the mouse buttons are given by the macros defined
 *        at the top of this file.
 * \param event The type of event. Possible values are 0, 1 and 2, which
 *        corresponds to the events defined in macros.
 * \param mods if any modifier keys are held down at the time of the event
 *        modifiers are defined in macros.
 */
void DrawRend::mouse_event(int key, int event, unsigned char mods) {
  if (key == MOUSE_LEFT) {
    if (event == EVENT_PRESS)
      left_clicked = true;
    if (event == EVENT_RELEASE)
      left_clicked = false;
  }
}

/**
 * Respond to keyboard event.
 * The viewer will always forward mouse key events to the renderer.
 * \param key The key that spawned the event. ASCII numbers are used for
 *        letter characters. Non-letter keys are selectively supported
 *        and are defined in macros.
 * \param event The type of event. Possible values are 0, 1 and 2, which
 *        corresponds to the events defined in macros.
 * \param mods if any modifier keys are held down at the time of the event
 *        modifiers are defined in macros.
 */
void DrawRend::keyboard_event(int key, int event, unsigned char mods) {
  if (event != EVENT_PRESS)
    return;

  // tab through the loaded files
  if (key >= '1' && key <= '9' && key - '1' < svgs.size()) {
    current_svg = key - '1';
    redraw();
    return;
  }

  switch (key) {

    // reset view transformation
  case ' ':
    view_init();
    redraw();
    break;

    // Q/E are unused in the original Viewer and DrawRend key maps. Screen y
    // grows down, so negative angle rotates counterclockwise on the display.
  case 'Q':
    rotate_view(-15);
    redraw();
    break;
  case 'E':
    rotate_view(15);
    redraw();
    break;

  case 'A':
    set_analytic(!use_analytic);
    redraw();
    break;

  case 'B':
    if (!use_analytic) {
      const int next = (regular_rasterizer->get_rasterization_method() + 1) % 3;
      regular_rasterizer->set_rasterization_method(static_cast<RasterizationMethod>(next));
      redraw();
    }
    break;

    // set the sampling rate to 1, 4, 9, or 16
  case '=':
    if (sample_rate < 16) {
      sample_rate = (int)(sqrt(sample_rate) + 1) * (sqrt(sample_rate) + 1);
      software_rasterizer->set_sample_rate(sample_rate);
      redraw();
    }
    break;
  case '-':
    if (sample_rate > 1) {
      sample_rate = (int)(sqrt(sample_rate) - 1) * (sqrt(sample_rate) - 1);
      software_rasterizer->set_sample_rate(sample_rate);
      redraw();
    }
    break;

    // save the current buffer to disk
  case 'S':
    write_screenshot();
    break;

    // toggle pixel sampling scheme
  case 'P':
    psm = (PixelSampleMethod)((psm + 1) % 2);
    software_rasterizer->set_psm(psm);
    redraw();
    break;
    // toggle level sampling scheme
  case 'L':
    lsm = (LevelSampleMethod)((lsm + 1) % 3);
    software_rasterizer->set_lsm(lsm);
    redraw();
    break;

    // toggle zoom
  case 'Z':
    show_zoom = (show_zoom + 1) % 2;
    break;

  default:
    return;
  }
}

/**
 * Writes the contents of the pixel buffer to disk as a .png file.
 * The image filename contains the month, date, hour, minute, and second
 * to make sure it is unique and identifiable.
 */
void DrawRend::write_screenshot() {
  redraw();
  if (show_zoom) draw_zoom();

  vector<unsigned char> windowPixels(4 * width * height);
  glReadPixels(0, 0,
    width,
    height,
    GL_RGBA,
    GL_UNSIGNED_BYTE,
    &windowPixels[0]);

  vector<unsigned char> flippedPixels(4 * width * height);
  for (int row = 0; row < height; ++row)
    memcpy(&flippedPixels[row * width * 4], &windowPixels[(height - row - 1) * width * 4], 4 * width);

  time_t t = time(nullptr);
  tm* lt = localtime(&t);
  stringstream ss;
  ss << "screenshot_" << lt->tm_mon + 1 << "-" << lt->tm_mday << "_"
    << lt->tm_hour << "-" << lt->tm_min << "-" << lt->tm_sec << ".png";
  string file = ss.str();
  cout << "Writing file " << file << "...";
  if (lodepng::encode(file, flippedPixels, width, height))
    cerr << "Could not be written" << endl;
  else
    cout << "Success!" << endl;
}

/**
 * Writes the contents of the framebuffer to disk as a .png file.
 *
 */
void DrawRend::write_framebuffer() {
  // lodepng expects alpha channel, so we will just make a new vector with
  // alpha included

  std::vector<unsigned char> export_data;

  export_data.reserve(width * height * 4);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int k = 0; k < 3; ++k) {
        export_data.push_back(framebuffer[3 * (y * width + x) + k]);
      }
      export_data.push_back(255); // Opaque alpha
    }
  }

  if (lodepng::encode("test.png", export_data.data(), width, height))
    cerr << "Could not write framebuffer" << endl;
  else
    cerr << "Succesfully wrote framebuffer" << endl;
}


/**
 * Draws the current SVG tab to the screen. Also draws a
 * border around the SVG canvas. Resolves the supersample buffers
 * into the framebuffer before posting the framebuffer pixels to the screen.
 */
void DrawRend::redraw() {
  software_rasterizer->clear_buffers();

  SVG& svg = *svgs[current_svg];
  svg.draw(software_rasterizer, ndc_to_screen * svg_to_ndc[current_svg]);

  // draw canvas outline
  Vector2D a = ndc_to_screen * svg_to_ndc[current_svg] * (Vector2D(0, 0)); a.x--; a.y++;
  Vector2D b = ndc_to_screen * svg_to_ndc[current_svg] * (Vector2D(svg.width, 0)); b.x++; b.y++;
  Vector2D c = ndc_to_screen * svg_to_ndc[current_svg] * (Vector2D(0, svg.height)); c.x--; c.y--;
  Vector2D d = ndc_to_screen * svg_to_ndc[current_svg] * (Vector2D(svg.width, svg.height)); d.x++; d.y--;

  software_rasterizer->rasterize_line(a.x, a.y, b.x, b.y, Color::Black);
  software_rasterizer->rasterize_line(a.x, a.y, c.x, c.y, Color::Black);
  software_rasterizer->rasterize_line(d.x, d.y, b.x, b.y, Color::Black);
  software_rasterizer->rasterize_line(d.x, d.y, c.x, c.y, Color::Black);

  software_rasterizer->resolve_to_framebuffer();
  if (gl)
    draw_pixels();
}

/**
 * OpenGL boilerplate to put an array of RGBA pixels on the screen.
 */
void DrawRend::draw_pixels() {
  const unsigned char* pixels = &framebuffer[0];
  // copy pixels to the screen
  glPushAttrib(GL_VIEWPORT_BIT);
  glViewport(0, 0, width, height);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  // Distinct near/far planes are required; equal values cause GL_INVALID_VALUE.
  glOrtho(0, width, 0, height, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  // Place the first row at the top-left in the valid pixel-space projection.
  glRasterPos2f(0, height);
  glPixelZoom(1.0, -1.0);
  glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  glPixelZoom(1.0, 1.0);

  glPopAttrib();
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

/**
 * Reads off the pixels that should be in the zoom window, and
 * generates a pixel array with the zoomed view.
 */
void DrawRend::draw_zoom() {

  // size (in pixels) of region of interest
  size_t regionSize = 32;

  // relative size of zoom window
  size_t zoomFactor = 16;

  // compute zoom factor---the zoom window should never cover
  // more than 40% of the framebuffer, horizontally or vertically
  size_t bufferSize = min(width, height);
  if (regionSize * zoomFactor > bufferSize * 0.4) {
    zoomFactor = (bufferSize * 0.4) / regionSize;
  }
  size_t zoomSize = regionSize * zoomFactor;

  // adjust the cursor coordinates so that the region of
  // interest never goes outside the bounds of the framebuffer
  size_t cX = max(regionSize / 2, min(width - regionSize / 2 - 1, (size_t)cursor_x));
  size_t cY = max(regionSize / 2, min(height - regionSize / 2 - 1, height - (size_t)cursor_y));

  // grab pixels from the region of interest
  vector<unsigned char> windowPixels(3 * regionSize * regionSize);
  glReadPixels(cX - regionSize / 2,
    cY - regionSize / 2 + 1, // meh
    regionSize,
    regionSize,
    GL_RGB,
    GL_UNSIGNED_BYTE,
    &windowPixels[0]);

  // upsample by the zoom factor, highlighting pixel boundaries
  vector<unsigned char> zoomPixels(3 * zoomSize * zoomSize);
  unsigned char* wp = &windowPixels[0];
  // outer loop over pixels in region of interest
  for (int y = 0; y < regionSize; y++) {
    int y0 = y * zoomFactor;
    for (int x = 0; x < regionSize; x++) {
      int x0 = x * zoomFactor;
      unsigned char* zp = &zoomPixels[(x0 + y0 * zoomSize) * 3];
      // inner loop over upsampled block
      for (int j = 0; j < zoomFactor; j++) {
        for (int i = 0; i < zoomFactor; i++) {
          for (int k = 0; k < 3; k++) {
            // highlight pixel boundaries
            if (i == 0 || j == 0) {
              const float s = .3;
              zp[k] = (int)((1. - 2. * s) * wp[k] + s * 255.);
            }
            else {
              zp[k] = wp[k];
            }
          }
          zp += 3;
        }
        zp += 3 * (zoomSize - zoomFactor);
      }
      wp += 3;
    }
  }

  // copy pixels to the screen using OpenGL
  glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, width, 0, height, 0.01, 1000.);
  glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity(); glTranslated(0., 0., -1.);

  glRasterPos2i(width - zoomSize, height - zoomSize);
  glDrawPixels(zoomSize, zoomSize, GL_RGB, GL_UNSIGNED_BYTE, &zoomPixels[0]);
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();

}

/**
 * Initializes the default viewport to center and reasonably zoom the SVG
 * with a bit of margin.
 */
void DrawRend::view_init() {
  float w = svgs[current_svg]->width, h = svgs[current_svg]->height;
  view_states[current_svg].angle = 0;
  set_view(w / 2, h / 2, 1.2 * std::max(w, h) / 2);
}

/**
 * Sets the viewing transform matrix corresponding to a view centered at
 * (x,y) in SVG space, extending 'span' units in all four directions.
 * This transform maps to 'normalized device coordinates' (ndc), where the window
 * corresponds to the [0,1]^2 rectangle.
 */
void DrawRend::set_view(float x, float y, float span) {
  if (!std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(span) || span <= 0) return;
  ViewState& view = view_states[current_svg];
  view.x = x; view.y = y; view.span = span;
  // The original SVG->NDC transform maps the view center to (0.5,0.5).
  // Rotate around that point before the unchanged, isotropic NDC->screen map.
  const Matrix3x3 base(1, 0, -x + span, 0, 1, -y + span, 0, 0, 2 * span);
  svg_to_ndc[current_svg] = translate(.5f, .5f) * rotate(view.angle)
                        * translate(-.5f, -.5f) * base;
}

/**
 * Shift and zoom the active SVG's stored view. Explicit state is necessary:
 * a rotated matrix's translation entries no longer directly encode its center.
 */
void DrawRend::move_view(float dx, float dy, float zoom) {
  if (!std::isfinite(zoom) || zoom <= 0) return;
  const ViewState view = view_states[current_svg];
  set_view(view.x - dx, view.y - dy, view.span * zoom);
}

void DrawRend::rotate_view(float degrees) {
  if (!std::isfinite(degrees)) return;
  ViewState& view = view_states[current_svg];
  view.angle = std::remainder(view.angle + degrees, 360.0f);
  set_view(view.x, view.y, view.span);
}

Matrix3x3 DrawRend::get_view_transform() const {
  return ndc_to_screen * svg_to_ndc[current_svg];
}

float DrawRend::get_view_angle() const {
  return view_states[current_svg].angle;
}

void DrawRend::set_analytic(bool enabled) {
  use_analytic = enabled;
  software_rasterizer = enabled ? analytic_rasterizer : regular_rasterizer;
  // Recreate the selected target after syncing controls so stale frames from
  // the other mode never survive a switch. SVG transforms are shared.
  software_rasterizer->set_psm(psm);
  software_rasterizer->set_lsm(lsm);
  software_rasterizer->set_sample_rate(sample_rate);
  software_rasterizer->set_framebuffer_target(framebuffer.data(), width, height);
}

}
