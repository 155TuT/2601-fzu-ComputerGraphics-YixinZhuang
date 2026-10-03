cmake_minimum_required(VERSION 3.12...3.31)
project(Assignment1)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#-------------------------------------------------------------------------------
# Build options
#-------------------------------------------------------------------------------
option(BUILD_DEBUG     "Build with debug settings"    OFF)
option(BUILD_DOCS      "Build documentation"          OFF)
option(BUILD_CUSTOM    "Build without reference"      OFF)

set(BUILD_DEBUG ${BUILD_DEBUG} CACHE BOOL "Build debug" FORCE)

if (BUILD_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
endif()

#-------------------------------------------------------------------------------
# Set target
#-------------------------------------------------------------------------------
set(APPLICATION_SOURCE
    
    src/texture.cpp
    src/triangulation.cpp
    src/svgparser.cpp
    src/transforms.cpp
    src/rasterizer.cpp
    src/analytic_rasterizer.cpp
    src/drawrend.cpp
    src/svg.cpp
    src/main.cpp
    # Add headers for the sake of Xcode/Visual Studio projects
    src/rasterizer.h
    src/drawrend.h
    src/svg.h
    src/svgparser.h
    src/texture.h
    src/transforms.h
    src/triangulation.h
)

if (WIN32)
    include_directories(src/dirent)
    list(APPEND APPLICATION_SOURCE src/dirent/dirent.c)
endif(WIN32)

add_executable(draw ${APPLICATION_SOURCE})

target_include_directories(draw PUBLIC src)

set(CGL_INCLUDE_DIRS CGL/include CGL/deps/glew/include CGL/deps/glfw/include)

#-------------------------------------------------------------------------------
# Find dependencies
#-------------------------------------------------------------------------------
# CGL's original configure step enables AVX globally whenever the build machine
# supports it. Keep distributed binaries portable; rasterizer.cpp dispatches
# its isolated AVX function at runtime and otherwise uses SSE2/scalar.
set(HAVE_AVX_EXTENSIONS FALSE CACHE BOOL "Do not enable global AVX" FORCE)
set(HAVE_AVX2_EXTENSIONS FALSE CACHE BOOL "Do not enable global AVX2" FORCE)
add_subdirectory(CGL)
if(WIN32)
  # CGL's path helper uses WIN32; MinGW defines _WIN32 by default.
  target_compile_definitions(CGL PRIVATE WIN32)
endif()
target_include_directories(draw PUBLIC ${CGL_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CGL_CXX_FLAGS}")

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)
target_link_libraries(draw PUBLIC OpenGL::GL)
target_link_libraries(draw PUBLIC OpenGL::GLU)

target_link_libraries(draw PRIVATE CGL)
target_compile_definitions(draw PRIVATE GLEW_STATIC)

#-------------------------------------------------------------------------------
# Add subdirectories
#-------------------------------------------------------------------------------

# build documentation
if(BUILD_DOCS)
  find_package(DOXYGEN)
  if(DOXYGEN_FOUND AND BUILD_DOCS)
    add_subdirectory(docs)
  endif()
endif()

# Install settings
set(CMAKE_INSTALL_PREFIX "${Assignment1_SOURCE_DIR}/")

# Reproducible algorithm and GUI screenshot verification for the report.
option(BUILD_VERIFICATION "Build assignment verification programs" OFF)
if(BUILD_VERIFICATION)
  enable_testing()
  add_executable(algorithm_tests verification/algorithm_tests.cpp
    src/rasterizer.cpp src/transforms.cpp src/texture.cpp)
  target_include_directories(algorithm_tests PRIVATE src ${CGL_INCLUDE_DIRS})
  target_link_libraries(algorithm_tests PRIVATE CGL OpenGL::GL OpenGL::GLU)
  add_test(NAME rasterizer_algorithms COMMAND algorithm_tests)

  add_executable(rasterizer_benchmark verification/rasterizer_benchmark.cpp
    src/rasterizer.cpp src/texture.cpp)
  target_include_directories(rasterizer_benchmark PRIVATE src ${CGL_INCLUDE_DIRS})
  target_link_libraries(rasterizer_benchmark PRIVATE CGL OpenGL::GL OpenGL::GLU)

  add_executable(analytic_tests verification/analytic_tests.cpp
    src/analytic_rasterizer.cpp src/rasterizer.cpp src/texture.cpp)
  target_include_directories(analytic_tests PRIVATE src ${CGL_INCLUDE_DIRS})
  target_link_libraries(analytic_tests PRIVATE CGL OpenGL::GL OpenGL::GLU)
  add_test(NAME analytic_coverage COMMAND analytic_tests)

  add_executable(aa_benchmark verification/aa_benchmark.cpp
    src/analytic_rasterizer.cpp src/rasterizer.cpp src/texture.cpp)
  target_include_directories(aa_benchmark PRIVATE src ${CGL_INCLUDE_DIRS})
  target_link_libraries(aa_benchmark PRIVATE CGL OpenGL::GL OpenGL::GLU)

  set(SCREENSHOT_SOURCES ${APPLICATION_SOURCE})
  list(REMOVE_ITEM SCREENSHOT_SOURCES src/main.cpp)
  add_executable(screenshot_runner verification/screenshot_runner.cpp ${SCREENSHOT_SOURCES})
  target_include_directories(screenshot_runner PRIVATE src ${CGL_INCLUDE_DIRS})
  target_link_libraries(screenshot_runner PRIVATE CGL OpenGL::GL OpenGL::GLU)
  target_compile_definitions(screenshot_runner PRIVATE GLEW_STATIC)
endif()

# Keep scalar and SIMD multiplication/addition order comparable at boundaries.
# AVX is enabled only by the function target attribute, never for an executable.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(draw PRIVATE -ffp-contract=off)
  if(BUILD_VERIFICATION)
    target_compile_options(algorithm_tests PRIVATE -ffp-contract=off)
    target_compile_options(rasterizer_benchmark PRIVATE -ffp-contract=off)
    target_compile_options(analytic_tests PRIVATE -ffp-contract=off)
    target_compile_options(aa_benchmark PRIVATE -ffp-contract=off)
    target_compile_options(screenshot_runner PRIVATE -ffp-contract=off)
  endif()
endif()
