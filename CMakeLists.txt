cmake_minimum_required(VERSION 3.10)
project(BugsBunnyCrazyCastle)

set(CMAKE_CXX_STANDARD 11)

# 1. Load PkgConfig FIRST (This fixes the "Unknown CMake command" error!)
find_package(PkgConfig REQUIRED)

# 2. Locate Dependencies
find_package(GLEW REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

# 3. SOIL Sources
set(SOIL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/soil")
set(SOIL_SOURCES 
    "${SOIL_ROOT}/src/soil.c"
    "${SOIL_ROOT}/src/image_dxt.c"
    "${SOIL_ROOT}/src/image_helper.c"
    "${SOIL_ROOT}/src/stb_image_aug.c"
)

# 4. Include Directories
include_directories(
    src
    include
    external/soil/include
    external/soil/include/SOIL
    external/json/include        
    ${GLEW_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
)

# 5. Source Files
file(GLOB SOURCES "src/*.cpp")

# 6. Build Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${SOIL_SOURCES})

# 7. Linking
target_link_libraries(${PROJECT_NAME}
    ${GLEW_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${FREETYPE_LIBRARIES}
)

# 8. AUTOMATIC SYMLINKS
# This ensures your executable can find your assets!
foreach(dir images levels shaders)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/${dir}
        ${CMAKE_CURRENT_BINARY_DIR}/${dir}
        COMMENT "Creating symlink for ${dir}"
    )
endforeach()