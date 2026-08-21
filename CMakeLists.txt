cmake_minimum_required(VERSION 3.16)
project(MatrixMultiplication C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Optimize the CPU baseline.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS_RELEASE "-O2")
endif()

# Silence MSVC standard C warnings.
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Use installed GLFW or fetch it.
list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
find_package(glfw3 3.3 QUIET)

if(NOT glfw3_FOUND)
    message(STATUS "glfw3 bulunamadi, FetchContent ile indiriliyor")
    include(FetchContent)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4)
    FetchContent_MakeAvailable(glfw)
endif()

# Load OpenGL symbols through GLAD.
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)

# Build each variant as a standalone app.
if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
endif()

function(add_variant name main_source)
    add_executable(${name}
        ${main_source}
        src/platform/platform_support.c
        ${ARGN})
    target_include_directories(${name} PRIVATE
        src/platform
        src/runtime
        src/operations)
    target_link_libraries(${name} PRIVATE glad glfw)

    # Embed the runtime shader path.
    target_compile_definitions(${name} PRIVATE
        MM_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    if(APPLE)
        target_link_libraries(${name} PRIVATE "-framework OpenGL")
    elseif(UNIX)
        target_link_libraries(${name} PRIVATE OpenGL::GL m dl pthread)
    elseif(WIN32)
        target_link_libraries(${name} PRIVATE opengl32)
    endif()
endfunction()

add_variant(fragment_main src/app/fragment_main.c
    src/runtime/fragment_compute.c
    src/operations/matrix_operation.c)
add_variant(fragment_matrix_cli src/app/fragment_matrix_cli.c
    src/runtime/fragment_compute.c
    src/operations/matrix_operation.c)
add_variant(compute_main src/app/compute_main.c
    src/operations/matrix_operation.c)

