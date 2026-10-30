#include "CGL/CGL.h"
#include "collada.h"
#include "meshEdit.h"
#include "bezierPatch.h"
#include "bezierCurve.h"
#include "mergeVertices.h"
#include "shaderUtils.h"
#include <iostream>
#include <stdexcept>
#include <memory>
#include <cstring>
#include <algorithm>
using namespace std;
using namespace CGL;

struct FileHandle {
  FILE* file;
  explicit FileHandle(const char* path) : file(fopen(path,"r")) {
    if (!file) throw runtime_error(string("Cannot open input: ") + path);
  }
  ~FileHandle() { fclose(file); }
};
int loadFile(MeshEdit* editor, const char* path) {
  Scene* scene = new Scene();
  string file(path), suffix = file.size() >= 4 ? file.substr(file.size()-4) : "";
  if (suffix == ".dae") {
    if (ColladaParser::load(path, scene) < 0) { delete scene; return -1; }
  } else if (suffix == ".bez") {
    auto* camera = new Camera(); camera->type = CAMERA;
    Node node; node.instance = camera; scene->nodes.push_back(node);
    auto* mesh = new Polymesh();
    FileHandle input(path); int count = 0;
    if (fscanf(input.file,"%d",&count) != 1 || count < 1) throw runtime_error("Invalid patch count");
    for (int i=0; i<count; ++i) {
      BezierPatch patch; patch.loadControlPoints(input.file); patch.add2mesh(mesh); mergeVertices(mesh);
    }
    mesh->type = POLYMESH; node.instance = mesh; scene->nodes.push_back(node);
  } else { delete scene; return -1; }
  editor->load(scene);
  GLuint texture = makeTex("envmap/envmap.png");
  if (!texture) texture = makeTex("code/envmap/envmap.png");
  glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,texture); glActiveTexture(GL_TEXTURE0);
  return 0;
}

void saveFrame(Renderer* renderer, const string& path) {
  GLint dimensions[4]; glGetIntegerv(GL_VIEWPORT, dimensions);
  PNG png; png.width = dimensions[2]; png.height = dimensions[3];
  png.pixels.resize(png.width*png.height*4);
  glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  renderer->render(); glFinish();
  glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadPixels(0,0,png.width,png.height,GL_RGBA,GL_UNSIGNED_BYTE,png.pixels.data());
  for (int y=0; y<png.height/2; ++y)
    for (int x=0; x<png.width*4; ++x)
      swap(png.pixels[y*png.width*4+x],png.pixels[(png.height-1-y)*png.width*4+x]);
  if (PNGParser::save(path.c_str(),png) != 0) throw runtime_error("Cannot save screenshot: " + path);
  cout << "Saved " << path << " (" << png.width << "x" << png.height << ")" << endl;
}
int main(int argc,char** argv) {
  try {
    if (argc < 2 || string(argv[1]) == "--help") {
      cout << "meshedit scene.bzc|scene.bez|scene.dae [--screenshot image.png] [--ops smooth,loop,flip:0,split:1,symmetric] [--view azimuth elevation zoom]\n";
      return argc < 2 ? 1 : 0;
    }
    string path(argv[1]), output, operations;
    double azimuth=0,elevation=0,zoom=1;
    for(int i=2; i<argc; ++i) {
      string option(argv[i]);
      if(option=="--screenshot" && i+1<argc) output=argv[++i];
      else if(option=="--ops" && i+1<argc) operations=argv[++i];
      else if(option=="--view" && i+3<argc) {
        azimuth=stod(argv[++i]);elevation=stod(argv[++i]);zoom=stod(argv[++i]);
        if(zoom<=0) throw invalid_argument("Zoom must be positive");
      } else throw invalid_argument("Invalid option: " + option);
    }
    FileHandle existence(path.c_str());
    string suffix=path.size()>=4 ? path.substr(path.size()-4) : "";
    Viewer viewer;
    Renderer* renderer=nullptr;
    if(suffix==".bzc") {
      int count=0;
      if(fscanf(existence.file,"%d",&count)!=1 || count<2 || count>10000) throw runtime_error("Invalid control point count");
      auto* curve=new BezierCurve(count); curve->loadControlPoints(existence.file);
      renderer=curve; viewer.set_renderer(renderer); viewer.init(output.empty());
      for(char key:operations) if(key!=',') curve->key_event(key);
    } else {
      auto* editor=new MeshEdit(); renderer=editor;
      viewer.set_renderer(renderer); viewer.init(output.empty());
      if(loadFile(editor,path.c_str())<0) throw runtime_error("Cannot load mesh scene");
      editor->batchView(azimuth,elevation,zoom);
      stringstream stream(operations);string operation;
      while(getline(stream,operation,',')) if(!operation.empty()) editor->batchOperation(operation);
    }
    if(output.empty()) viewer.start(); else saveFrame(renderer,output);
    return 0;
  } catch(const exception& error) { cerr << error.what() << endl;return 1; }
}
