cmake_minimum_required(VERSION 3.22)

project(vulkan-study)

set(CMAKE_CXX_STANDARD 17)

add_executable(app main.cpp)

find_package(Vulkan REQUIRED)
target_include_directories(app PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(app PRIVATE ${Vulkan_LIBRARIES})

# GLFW（apt install 済み）
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
target_include_directories(app PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_libraries(app PRIVATE ${GLFW_LIBRARIES})

# GLM（ヘッダオンリーなので include だけでOK）
find_path(GLM_INCLUDE_DIR glm/glm.hpp)
target_include_directories(app PRIVATE ${GLM_INCLUDE_DIR})