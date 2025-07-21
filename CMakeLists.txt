cmake_minimum_required(VERSION 3.22)

project(vulkan-study)

set(CMAKE_CXX_STANDARD 17)

add_executable(app main.cpp)

find_package(Vulkan REQUIRED)
target_include_directories(app PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(app PRIVATE ${Vulkan_LIBRARIES})
