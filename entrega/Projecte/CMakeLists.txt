cmake_minimum_required(VERSION 3.10)
project(SamuraiVengeance)

set(CMAKE_CXX_STANDARD 11)

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)


set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

add_subdirectory(external/freetype)

add_definitions(-DGLEW_STATIC)
set(GLEW_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/external/glew/src/glew.c")
include_directories(external/glew/include)

set(SOIL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/soil")
set(SOIL_SOURCES 
    "${SOIL_ROOT}/src/soil.c"
    "${SOIL_ROOT}/src/image_dxt.c"
    "${SOIL_ROOT}/src/image_helper.c"
    "${SOIL_ROOT}/src/stb_image_aug.c"
)

include_directories(
    src
    include
    external/soil/include
    external/soil/include/SOIL
    external/json
)

file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES} ${SOIL_SOURCES} ${GLEW_SOURCES})

target_link_libraries(${PROJECT_NAME}
    glfw
    freetype
    ${OPENGL_LIBRARIES}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} winmm)
endif()

if(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${COREFOUNDATION_FRAMEWORK})
endif()

foreach(dir images levels shaders sounds fonts)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/${dir}
        ${CMAKE_CURRENT_BINARY_DIR}/${dir}
        COMMENT "Copying directory ${dir}"
    )
endforeach()