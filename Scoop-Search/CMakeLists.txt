# CMakeList.txt: Scoop-Search 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.15)

set(CMAKE_CXX_STANDARD 23)

find_package(jsoncpp CONFIG REQUIRED)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
aux_source_directory(. SRC_FILES)
add_executable (Scoop-Search ${SRC_FILES})

target_link_libraries(Scoop-Search PRIVATE jsoncpp_object)

set_property(TARGET Scoop-Search  PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")
