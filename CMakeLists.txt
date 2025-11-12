cmake_minimum_required(VERSION 3.10)
project(DiningPhilosophersGLFW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm QUIET)

add_executable(philosophers
    src/main.cpp
    src/philosopher.cpp
    glad/src/glad.c
)

target_include_directories(philosophers PRIVATE
    ${PROJECT_SOURCE_DIR}/glad/include
    ${PROJECT_SOURCE_DIR}/inc
)

target_link_libraries(philosophers PRIVATE
    ${OPENGL_LIBRARIES}
    glfw
)

if(glm_FOUND AND TARGET glm::glm)
    target_link_libraries(philosophers PRIVATE glm::glm)
elseif(glm_FOUND AND DEFINED glm_INCLUDE_DIRS)
    target_include_directories(philosophers PRIVATE ${glm_INCLUDE_DIRS})
elseif(EXISTS "${PROJECT_SOURCE_DIR}/glm-1.0.2")
    target_include_directories(philosophers PRIVATE ${PROJECT_SOURCE_DIR}/glm-1.0.2)
else()
    message(FATAL_ERROR "GLM not found. Install glm or provide it under glm-1.0.2")
endif()

target_compile_definitions(philosophers PRIVATE PROJECT_ROOT="${PROJECT_SOURCE_DIR}")