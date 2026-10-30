from pathlib import Path
p=Path('homework/answer/task2/code/verification/geometry_tests.cpp');s=p.read_text(encoding='utf-8-sig').replace('int main() {','int main(int argc, char** argv) {')
s=s.replace('  std::cout<<"PASS "', '''  if(argc > 1) {
    Scene scene;require(ColladaParser::load(argv[1],&scene)==0,"Load teacher mesh");
    for(auto& node:scene.nodes) if(node.instance && node.instance->type==POLYMESH) {
      auto& polymesh=*static_cast<Polymesh*>(node.instance);
      std::vector<std::vector<Index>> faces;
      for(const auto& face:polymesh.polygons) faces.push_back(face.vertex_indices);
      HalfedgeMesh loaded;loaded.build(faces,polymesh.vertices);validate(loaded);
      for(int level=0;level<2;++level) {
        const size_t v=loaded.nVertices(),e=loaded.nEdges(),f=loaded.nFaces(),b=loaded.nBoundaries();
        resampler.upsample(loaded);validate(loaded);
        require(loaded.nVertices()==v+e,"Scene Loop vertex growth");
        require(loaded.nFaces()==4*f,"Scene Loop face growth");
        require(loaded.nBoundaries()==b,"Scene boundary component preservation");
      }
      std::cout<<"scene="<<argv[1]<<" final V/E/F="<<loaded.nVertices()<<"/"<<loaded.nEdges()<<"/"<<loaded.nFaces()<<std::endl;
    }
  }
  std::cout<<"PASS "''');p.write_text(s,encoding='utf-8')
p=Path('homework/answer/task2/code/CMakeLists.txt');s=p.read_text(encoding='utf-8-sig').replace('  add_test(NAME geometry_algorithms COMMAND geometry_tests)','''  add_test(NAME geometry_algorithms COMMAND geometry_tests)
  foreach(scene cube teapot open-square)
    add_test(NAME geometry_${scene} COMMAND geometry_tests ${CMAKE_CURRENT_SOURCE_DIR}/dae/${scene}.dae)
  endforeach()''');p.write_text(s,encoding='utf-8')
