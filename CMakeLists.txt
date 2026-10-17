cmake_minimum_required(VERSION 3.20)
project(ORBITGUARD LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# freeglut 3.6 still declares an ancient cmake_minimum_required; CMake 4 refuses that without this.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

set(FREEGLUT_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(FREEGLUT_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(FREEGLUT_GLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(freeglut
  URL https://github.com/freeglut/freeglut/releases/download/v3.6.0/freeglut-3.6.0.tar.gz)
FetchContent_Declare(imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.tar.gz)
FetchContent_Declare(glm
  URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(freeglut glm json imgui)  # imgui has no CMakeLists; we build its sources below

find_package(OpenGL REQUIRED)
find_package(OpenMP)

if(MSVC)
  add_compile_options(/utf-8 /W3 /permissive-)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# ---- core: simulation + algorithms, no OpenGL (unit-testable) ----
file(GLOB CORE_SRC CONFIGURE_DEPENDS
  src/math/*.cpp src/orbit/*.cpp src/ssa/*.cpp src/geom/*.cpp
  src/raster/*.cpp src/color/*.cpp src/io/*.cpp)
add_library(orbitguard_core STATIC ${CORE_SRC})
target_include_directories(orbitguard_core PUBLIC src)
target_link_libraries(orbitguard_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(orbitguard_core PUBLIC ORBITGUARD_ROOT="${CMAKE_SOURCE_DIR}")
if(OpenMP_CXX_FOUND)
  target_link_libraries(orbitguard_core PUBLIC OpenMP::OpenMP_CXX)
endif()

# ---- Dear ImGui (GLUT + fixed-function OpenGL2 backends) ----
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glut.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends
  ${freeglut_SOURCE_DIR}/include)
target_compile_definitions(imgui PUBLIC FREEGLUT_STATIC)
target_link_libraries(imgui PUBLIC freeglut_static OpenGL::GL)

# ---- app ----
file(GLOB APP_SRC CONFIGURE_DEPENDS
  src/main.cpp src/app/*.cpp src/render/*.cpp src/hud/*.cpp src/ui/*.cpp src/lab/*.cpp)
add_executable(orbitguard ${APP_SRC})
target_include_directories(orbitguard PRIVATE external/stb)
target_link_libraries(orbitguard PRIVATE orbitguard_core imgui freeglut_static OpenGL::GL OpenGL::GLU)
if(WIN32)
  target_link_libraries(orbitguard PRIVATE winmm gdi32)
endif()

# ---- tests: plain assert executables ----
enable_testing()
foreach(t math kepler raster color ssa)
  add_executable(test_${t} tests/test_${t}.cpp)
  target_link_libraries(test_${t} PRIVATE orbitguard_core glm::glm)
  add_test(NAME ${t} COMMAND test_${t} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
