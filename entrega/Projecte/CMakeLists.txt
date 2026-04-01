cmake_minimum_required(VERSION 3.10)
project(SamuraiVengeance)

set(CMAKE_CXX_STANDARD 11)

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

# 1. Locate System OpenGL (This is the ONLY thing that should come from the OS)
find_package(OpenGL REQUIRED)

# 2. Configure and Add Local GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# 3. Configure and Add Local FreeType
# Disable external dependencies so FreeType compiles 100% standalone
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)

# Force FreeType to accept modern CMake policies without terminal flags
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

add_subdirectory(external/freetype)

# 4. Local GLEW (GLEW is simple enough to compile directly as a source file)
add_definitions(-DGLEW_STATIC)
set(GLEW_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/external/glew/src/glew.c")
include_directories(external/glew/include)

# 5. Local SOIL
set(SOIL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/soil")
set(SOIL_SOURCES 
    "${SOIL_ROOT}/src/soil.c"
    "${SOIL_ROOT}/src/image_dxt.c"
    "${SOIL_ROOT}/src/image_helper.c"
    "${SOIL_ROOT}/src/stb_image_aug.c"
)

# 6. Include Directories for your game
include_directories(
    src
    include
    external/soil/include
    external/soil/include/SOIL
    external/json
)

# 7. Source Files
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# 8. Build Executable (We compile GLEW and SOIL right into the exe)
add_executable(${PROJECT_NAME} ${SOURCES} ${SOIL_SOURCES} ${GLEW_SOURCES})

# 9. Linking
# Link against the local CMake targets (glfw, freetype) and system OpenGL
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

# 10. AUTOMATIC SYMLINKS
foreach(dir images levels shaders sounds fonts)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/${dir}
        ${CMAKE_CURRENT_BINARY_DIR}/${dir}
        COMMENT "Copying directory ${dir}"
    )
endforeach()