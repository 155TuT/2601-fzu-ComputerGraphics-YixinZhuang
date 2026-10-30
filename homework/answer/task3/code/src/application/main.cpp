#include <sys/types.h>
#include "CGL/CGL.h"
#include "CGL/viewer.h"

#define TINYEXR_IMPLEMENTATION
#include "CGL/tinyexr.h"

#include "application.h"
#include "pathtracer/render_options.h"
#include "util/image.h"

#include <iostream>
#include <cerrno>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#include "util/win32/getopt.h"
#else
#include <unistd.h>
#endif
#include "pathtracer_launcher_gui.h"

using namespace std;
using namespace CGL;

#define msg(s) cerr << "[PathTracer] " << s << endl;

size_t parse_integer(const char* text, const char* option, size_t minimum = 1,
                     size_t maximum = std::numeric_limits<int>::max()) {
  char* end = nullptr;
  errno = 0;
  if (!text || !*text || *text == '-')
    throw std::invalid_argument(string(option) + " requires an integer in [" +
                                std::to_string(minimum) + ", " + std::to_string(maximum) + "]");
  const unsigned long long value = std::strtoull(text, &end, 10);
  if (errno == ERANGE || end == text || *end || value < minimum || value > maximum)
    throw std::invalid_argument(string("Invalid value for ") + option + ": " + text);
  return static_cast<size_t>(value);
}

double parse_real(const char* text, const char* option, bool allow_zero) {
  char* end = nullptr;
  errno = 0;
  const double value = text ? std::strtod(text, &end) : -1;
  if (!text || end == text || *end || errno == ERANGE || !std::isfinite(value) ||
      value < 0 || (!allow_zero && value == 0))
    throw std::invalid_argument(string("Invalid value for ") + option + ": " + (text ? text : "<missing>"));
  return value;
}

const char* next_argument(int argc, char** argv, int& index, const char* option) {
  if (index >= argc) throw std::invalid_argument(string("Missing argument for ") + option);
  return argv[index++];
}

void usage(const char *binaryName) {
  printf("Usage: %s [options] <scenefile>\n", binaryName);
  printf("Program Options:\n");
  printf("  -s  <INT>        Number of camera rays per pixel\n");
  printf("  -l  <INT>        Number of samples per area light\n");
  printf("  -t  <INT>        Number of render threads\n");
  printf("  -m  <INT>        Maximum ray depth\n");
  printf("  -o  <INT>        Accumulate Bounces of Light \n");
  printf("  -e  <PATH>       Path to environment map\n");
  printf("  -b  <FLOAT>      The size of the aperture\n");
  printf("  -d  <FLOAT>      The focal distance\n");
  printf("  -f  <FILENAME>   Image (.png) file to save output to in windowless "
         "mode\n");
  printf(
      "  -r  <INT> <INT>  Width and height of output image (if windowless)\n");
  printf("  -N               Surface normal diagnostic colors\n");
  printf("  -B               Brute-force intersections (comparison)\n");
  printf("  -U               Fixed maximum depth without roulette\n");
  printf("  -q  <UINT>       Random seed (default 1)\n");
  printf("  -H               Uniform hemisphere direct lighting\n");
  printf("  -h               Print this help message\n");
  printf("\n");
}

HDRImageBuffer *load_exr(const char *file_path) {

  const char *err;

  EXRImage exr;
  InitEXRImage(&exr);

  int ret = ParseMultiChannelEXRHeaderFromFile(&exr, file_path, &err);
  if (ret != 0) {
    msg("Error parsing OpenEXR file: " << err);
    return NULL;
  }

  for (int i = 0; i < exr.num_channels; i++) {
    if (exr.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
      exr.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    }
  }

  ret = LoadMultiChannelEXRFromFile(&exr, file_path, &err);
  if (ret != 0) {
    msg("Error loading OpenEXR file: " << err);
    exit(EXIT_FAILURE);
  }

  HDRImageBuffer *envmap = new HDRImageBuffer();
  envmap->resize(exr.width, exr.height);
  float *channel_r = (float *)exr.images[2];
  float *channel_g = (float *)exr.images[1];
  float *channel_b = (float *)exr.images[0];
  for (size_t i = 0; i < exr.width * exr.height; i++) {
    envmap->data[i] = Vector3D(channel_r[i], channel_g[i], channel_b[i]);
  }

  return envmap;
}

int run_application(int argc, char **argv) {

  // get the options
  AppConfig config;
  int opt;
  bool write_to_file = false;
  size_t w = 0, h = 0, x = -1, y = 0, dx = 0, dy = 0;
  string output_file_name, cam_settings = "";
  string sceneFilePath;
  if (argc == 1) { // no argument specifiers, launch GUI to get the settings
#define SETTINGSFILE_PATH "settings.txt"
    PathtracerLauncherGUI::GUISettings settings;
    settings.deserialize(SETTINGSFILE_PATH);
    PathtracerLauncherGUI::draw(settings);
    // done drawing
    { // extract settings that don't belong to config
      w = settings.w;
      h = settings.h;
      x = settings.x;
      y = settings.y;
      dx = settings.dx;
      dy = settings.dy;
      write_to_file = settings.write_to_file;
      sceneFilePath = settings.scene_file_path;
      output_file_name = settings.output_file_name;
      cam_settings = settings.cam_settings;
    }
    { // extract settings that belong to config
      config.pathtracer_direct_hemisphere_sample =
          settings.pathtracer_direct_hemisphere_sample;
      config.pathtracer_focalDistance = settings.pathtracer_focalDistance;
      config.pathtracer_lensRadius = settings.pathtracer_lensRadius;
      config.pathtracer_max_ray_depth = settings.pathtracer_max_ray_depth;
      config.pathtracer_max_tolerance = settings.pathtracer_max_tolerance;
      config.pathtracer_ns_aa = settings.pathtracer_ns_aa;
      config.pathtracer_ns_area_light = settings.pathtracer_ns_area_light;
      config.pathtracer_ns_diff = settings.pathtracer_ns_diff;
      config.pathtracer_ns_glsy = settings.pathtracer_ns_glsy;
      config.pathtracer_ns_refr = settings.pathtracer_ns_refr;
      config.pathtracer_num_threads = settings.pathtracer_num_threads;
      config.pathtracer_samples_per_patch =
          settings.pathtracer_samples_per_patch;
      config.pathtracer_accumulate_bounces = settings.pathtracer_accumulate_bounces;
    }
  } else {
    while ((opt = getopt(argc, argv, "s:l:t:m:o:e:hHf:r:c:b:d:a:p:NBUq:")) !=
           -1) { // for each option...
      switch (opt) {
      case 'N': render_options().normals=true; break;
      case 'B': render_options().brute_force=true; break;
      case 'U': render_options().roulette=false; break;
      case 'q': render_options().seed=static_cast<uint32_t>(parse_integer(optarg,"-q",0,UINT32_MAX)); break;
      case 'f':
        write_to_file = true;
        output_file_name = string(optarg);
        break;
      case 'r':
        w = parse_integer(optarg,"-r width");
        h = parse_integer(next_argument(argc,argv,optind,"-r height"),"-r height");
        break;
      case 'p':
        x = parse_integer(optarg,"-p x",0);
        y = parse_integer(next_argument(argc,argv,optind,"-p y"),"-p y",0);
        dx = parse_integer(next_argument(argc,argv,optind,"-p width"),"-p width");
        dy = parse_integer(next_argument(argc,argv,optind,"-p height"),"-p height");
        break;
      case 's':
        config.pathtracer_ns_aa = parse_integer(optarg,"-s");
        break;
      case 'l':
        config.pathtracer_ns_area_light = parse_integer(optarg,"-l");
        break;
      case 't':
        config.pathtracer_num_threads = parse_integer(optarg,"-t",1,256);
        break;
      case 'm':
        config.pathtracer_max_ray_depth = parse_integer(optarg,"-m",0);
        break;
      case 'o':
        config.pathtracer_accumulate_bounces = parse_integer(optarg,"-o",0,1) != 0;
        break;
      case 'e':
        std::cout << "[PathTracer] Loading environment map " << optarg
                  << std::endl;
        config.pathtracer_envmap = load_exr(optarg);
        break;
      case 'c':
        cam_settings = string(optarg);
        break;
      case 'b':
        config.pathtracer_lensRadius = parse_real(optarg,"-b",true);
        break;
      case 'd':
        config.pathtracer_focalDistance = parse_real(optarg,"-d",false);
        break;
      case 'a':
        config.pathtracer_samples_per_patch = parse_integer(optarg,"-a batch size");
        config.pathtracer_max_tolerance = parse_real(next_argument(argc,argv,optind,"-a tolerance"),"-a tolerance",false);
        break;
      case 'H':
        config.pathtracer_direct_hemisphere_sample = true;
        break;
      case 'h':
        usage(argv[0]);
        return 0;
      default:
        usage(argv[0]);
        return 1;
      }
    }

    // print usage if no argument given
    if (optind >= argc) {
      usage(argv[0]);
      return 1;
    }

    sceneFilePath = argv[optind];
  }
  // Apply the same bounds to settings supplied through the launcher.
  if (config.pathtracer_num_threads < 1 || config.pathtracer_num_threads > 256)
    throw std::invalid_argument("Render thread count must be between 1 and 256");
  const size_t integer_limit = std::numeric_limits<int>::max();
  if (config.pathtracer_ns_aa < 1 || config.pathtracer_ns_aa > integer_limit ||
      config.pathtracer_ns_area_light < 1 || config.pathtracer_ns_area_light > integer_limit ||
      config.pathtracer_max_ray_depth > integer_limit)
    throw std::invalid_argument("Sample counts must be positive and ray depth must be nonnegative integers");
  if ((w == 0) != (h == 0) || w > integer_limit || h > integer_limit ||
      (w && h > integer_limit / w))
    throw std::invalid_argument("Image dimensions must be positive and fit the image buffer");
  if (x != size_t(-1) && (!w || !h || !dx || !dy || x >= w || y >= h || dx > w-x || dy > h-y))
    throw std::invalid_argument("The render region must lie within the image dimensions supplied with -r");
  if (write_to_file && output_file_name.empty())
    throw std::invalid_argument("The output PNG path is empty");
  msg("Input scene file: " << sceneFilePath);
  string sceneFile = sceneFilePath.substr(sceneFilePath.find_last_of('/') + 1);
  sceneFile = sceneFile.substr(0, sceneFile.find(".dae"));
  config.pathtracer_filename = sceneFile;

  // parse scene
  Collada::SceneInfo *sceneInfo = new Collada::SceneInfo();
  if (Collada::ColladaParser::load(sceneFilePath.c_str(), sceneInfo) < 0) {
    delete sceneInfo;
    exit(EXIT_FAILURE);
  }

  // create application
  Application *app = new Application(config, !write_to_file);

  msg("Rendering using " << config.pathtracer_num_threads << " threads");

  // write straight to file without opening a window if -f option provided
  if (write_to_file) {
    app->init();
    if (w && h) app->resize(w, h);
    app->load(sceneInfo);
    delete sceneInfo;


    if (cam_settings != "")
      app->load_camera(cam_settings);

    app->render_to_file(output_file_name, x, y, dx, dy);
    return 0;
  }

  // create viewer
  Viewer viewer = Viewer();

  // set renderer
  viewer.set_renderer(app);

  // init viewer
  viewer.init();

  // load scene
  app->load(sceneInfo);

  delete sceneInfo;

  if (w && h)
    viewer.resize(w, h);

  if (cam_settings != "")
    app->load_camera(cam_settings);

  // start viewer
  viewer.start();

  return 0;
}

int main(int argc, char** argv) {
  try {
    return run_application(argc,argv);
  } catch (const std::exception& error) {
    msg(error.what());
    return EXIT_FAILURE;
  }
}
