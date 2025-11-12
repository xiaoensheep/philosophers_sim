cmake_minimum_required(VERSION 3.10)
project(DiningPhilosophersGLFW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(philosophers
    /home/zxy/Development/OShomeworks/philosophers_sim/src/main.cpp
    /home/zxy/Development/OShomeworks/philosophers_sim/src/philosopher.cpp
    /home/zxy/Development/OpenGL/glad/src/glad.c
)

target_include_directories(philosophers PRIVATE
    /home/zxy/Development/OpenGL/glm-1.0.2
    /home/zxy/Development/OShomeworks/philosophers_sim/inc
)

# 链接 OpenGL 和 GLFW
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# target_link_libraries(LearnTrans PRIVATE OPENGL::GL glfw)
target_link_libraries(philosophers PRIVATE ${OPENGL_LIBRARIES} glfw)
include_directories(${OPENGL_INCLUDE_DIR})