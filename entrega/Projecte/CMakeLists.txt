cmake_minimum_required(VERSION 3.10)
project(BugsBunnyCrazyCastle)

set(CMAKE_CXX_STANDARD 11)

# 1. Load PkgConfig FIRST (This fixes the "Unknown CMake command" error!)
find_package(PkgConfig REQUIRED)

# 2. Locate Dependencies
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND)
    pkg_check_modules(GLFW REQUIRED glfw3)
endif()
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

# Handle both modern imported target and legacy variable-based GLEW discovery.
set(GLEW_INCLUDE_PATHS ${GLEW_INCLUDE_DIRS})
if(NOT GLEW_INCLUDE_PATHS AND DEFINED GLEW_INCLUDE_DIR)
    set(GLEW_INCLUDE_PATHS ${GLEW_INCLUDE_DIR})
endif()

set(GLEW_LINK_LIBS ${GLEW_LIBRARIES})
if(NOT GLEW_LINK_LIBS AND DEFINED GLEW_LIBRARY)
    set(GLEW_LINK_LIBS ${GLEW_LIBRARY})
endif()

set(GLFW_INCLUDE_PATHS ${GLFW_INCLUDE_DIRS})
set(GLFW_LINK_LIBS ${GLFW_LIBRARIES})

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
    external/json
    ${GLEW_INCLUDE_PATHS}
    ${GLFW_INCLUDE_PATHS}
    ${FREETYPE_INCLUDE_DIRS}
)

# 5. Source Files
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# 6. Build Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${SOIL_SOURCES})

# 7. Linking
target_link_libraries(${PROJECT_NAME}
    ${OPENGL_LIBRARIES}
    ${FREETYPE_LIBRARIES}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} winmm)
endif()

if(TARGET GLEW::GLEW)
    target_link_libraries(${PROJECT_NAME} GLEW::GLEW)
else()
    target_link_libraries(${PROJECT_NAME} ${GLEW_LINK_LIBS})
endif()

if(TARGET glfw)
    target_link_libraries(${PROJECT_NAME} glfw)
elseif(TARGET glfw::glfw)
    target_link_libraries(${PROJECT_NAME} glfw::glfw)
else()
    target_link_libraries(${PROJECT_NAME} ${GLFW_LINK_LIBS})
endif()

if(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${COREFOUNDATION_FRAMEWORK})
endif()

# 8. AUTOMATIC SYMLINKS
# This ensures your executable can find your assets!
foreach(dir images levels shaders sounds fonts)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/${dir}
        ${CMAKE_CURRENT_BINARY_DIR}/${dir}
        COMMENT "Copying directory ${dir}"
    )
endforeach()