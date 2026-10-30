from pathlib import Path
r=Path('homework/answer/task2')
p=r/'code/CGL/CMakeLists.txt'; p.write_text(p.read_text(encoding='utf-8-sig').replace('add_subdirectory(deps/freetype)','add_subdirectory(deps/freetype/freetype)'),encoding='utf-8')
p=r/'code/CMakeLists.txt'; p.write_text(p.read_text(encoding='utf-8-sig').replace('src/png.cpp)','src/png.cpp src/lodepng.cpp)'),encoding='utf-8')
p=r/'code/src/png.cpp'; s=p.read_text(); s=s.replace('#include "png.h"','#include "png.h"\n#include "lodepng.h"'); s=s.replace('int PNGParser::save(const char *filename, const PNG& png) {\n  return 0;', 'int PNGParser::save(const char *filename, const PNG& png) {\n  return lodepng::encode(filename, png.pixels, png.width, png.height);'); p.write_text(s)
for name in ['code/CGL/include/CGL/viewer.h','code/CGL/src/viewer.h']:
 p=r/name;s=p.read_text();s=s.replace('void init( void );','void init( bool visible = true );');p.write_text(s)
p=r/'code/CGL/src/viewer.cpp';s=p.read_text();s=s.replace('void Viewer::init()','void Viewer::init(bool visible)');s=s.replace('// create window','glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);\n\n  // create window');p.write_text(s)
p=r/'code/src/meshEdit.h';s=p.read_text();s=s.replace('void load( Scene* scene );','void load( Scene* scene );\n  void batchOperation(const std::string& operation);\n  void batchView(double azimuth, double elevation, double zoom);');p.write_text(s)
p=r/'code/src/meshEdit.cpp';s=p.read_text();s=s.replace('smoothShading = false;','light_num = 0;\n    smoothShading = false;',1); p.write_text(s)
