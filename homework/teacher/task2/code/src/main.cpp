#include "CGL/CGL.h"

#include "collada.h"
#include "meshEdit.h"
#include "bezierPatch.h"
#include "bezierCurve.h"
#include "mergeVertices.h"
#include "shaderUtils.h"

#include <iostream>

using namespace std;
using namespace CGL;

#define msg(s) cerr << "[Collada Viewer] " << s << endl;

// automatically close FD when unwinding stackframe during exception/termination
struct FileHandle
{
  FILE* file = NULL;
  ~FileHandle() { if (file) fclose(file); }
};

int loadFile(MeshEdit* collada_viewer, const char* path) {

  Scene* scene = new Scene();

  std::string path_str = path;
  if (path_str.substr(path_str.length()-4, 4) == ".dae")
  {
    if (ColladaParser::load(path, scene) < 0) {
      delete scene;
      return -1;
    }
  }
  else if (path_str.substr(path_str.length()-4, 4) == ".bez")
  {
    Camera* cam = new Camera();
    cam->type = CAMERA;
    Node node;
    node.instance = cam;
    scene->nodes.push_back(node);
    Polymesh* mesh = new Polymesh();
    FileHandle file_handle;
    file_handle.file = fopen(path, "r");
    int n = 0;
    fscanf(file_handle.file, "%d", &n);
    for (int i = 0; i < n; i++)
    {
      BezierPatch patch;
      patch.loadControlPoints(file_handle.file);
      patch.add2mesh(mesh);
      mergeVertices(mesh);
    }

    mesh->type = POLYMESH;
    node.instance = mesh;
    scene->nodes.push_back(node);
  }
  else
  {
    return -1;
  }

  collada_viewer->load( scene );

  GLuint tex = makeTex("envmap/envmap.png");
  if(!tex) tex = makeTex("../envmap/envmap.png");
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, tex);
  glActiveTexture(GL_TEXTURE2);

  return 0;
}


int main( int argc, char** argv ) {

  // sanity check for argument passing
  if (argc != 2) {
    msg("Please make sure to launch the executable with a path to the scene file as an argument! \n Example: ./meshedit ../../curve1.bzc"); 
    exit(0);
  }

  const char* path = argv[1];
  std::string path_str = path;
 
  //////////////////////////////
  // Bezier curve viewer code //
  //////////////////////////////

  if (path_str.substr(path_str.length()-4, 4) == ".bzc")
  {
    // Each file contains a single Bezier curve's control points
    FileHandle file_handle;
    file_handle.file = fopen(path, "r");

    int numControlPoints;
    fscanf(file_handle.file, "%d", &numControlPoints);

    BezierCurve curve(numControlPoints);
    curve.loadControlPoints(file_handle.file);

    // Create viewer
    Viewer viewer = Viewer();
    viewer.set_renderer(&curve);
    viewer.init();
    viewer.start();

    exit(EXIT_SUCCESS);

    return 0;
  }

  // create viewer
  Viewer viewer = Viewer();

  // create collada_viewer
  MeshEdit* collada_viewer = new MeshEdit();

  // set collada_viewer as renderer
  viewer.set_renderer(collada_viewer);

  // init viewer
  viewer.init();

  // load tests
  if (loadFile(collada_viewer, argv[1]) < 0) 
  {
    msg("Failed loading the scene file. Make sure the file pathing is correct!");
    exit(0);
  }

  // start viewer
  viewer.start();

  return 0;
}
