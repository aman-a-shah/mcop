cmake_minimum_required(VERSION 3.18)
project(mcop
        VERSION 0.1.0
        DESCRIPTION "High-performance Monte Carlo options pricing engine"
        LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global build configuration
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Aggressive optimization flags for the Release build. `-march=native` lets the
# compiler emit SIMD (AVX/NEON) for the hot path-generation loops.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)

add_library(mcop_flags INTERFACE)
target_compile_options(mcop_flags INTERFACE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-funroll-loops>
    -Wall -Wextra)
if(HAS_MARCH_NATIVE)
    target_compile_options(mcop_flags INTERFACE $<$<CONFIG:Release>:-march=native>)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Eigen (header-only) — required from Phase 5 (Longstaff-Schwartz) onward.
# Detected here so early phases configure cleanly even before it is used.
# ---------------------------------------------------------------------------
find_path(EIGEN3_INCLUDE_DIR
    NAMES Eigen/Dense
    PATHS /opt/homebrew/include/eigen3 /usr/local/include/eigen3 /usr/include/eigen3
          ${EIGEN3_ROOT})
if(EIGEN3_INCLUDE_DIR)
    message(STATUS "Eigen found: ${EIGEN3_INCLUDE_DIR}")
else()
    message(STATUS "Eigen NOT found — LSM engine will be unavailable")
endif()

# ---------------------------------------------------------------------------
# Core static library — sources are globbed so later phases drop in cleanly.
# ---------------------------------------------------------------------------
file(GLOB MCOP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(mcop_core STATIC ${MCOP_SOURCES})
target_include_directories(mcop_core PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(mcop_core PUBLIC mcop_flags Threads::Threads)
if(EIGEN3_INCLUDE_DIR)
    target_include_directories(mcop_core PUBLIC "${EIGEN3_INCLUDE_DIR}")
    target_compile_definitions(mcop_core PUBLIC MCOP_HAVE_EIGEN=1)
endif()

# ---------------------------------------------------------------------------
# Command-line apps (added incrementally per phase).
# ---------------------------------------------------------------------------
file(GLOB MCOP_APPS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/apps/*.cpp")
foreach(app_src ${MCOP_APPS})
    get_filename_component(app_name ${app_src} NAME_WE)
    add_executable(${app_name} ${app_src})
    target_link_libraries(${app_name} PRIVATE mcop_core mcop_flags)
endforeach()

# ---------------------------------------------------------------------------
# Benchmarks.
# ---------------------------------------------------------------------------
file(GLOB MCOP_BENCH CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/benchmarks/*.cpp")
foreach(bench_src ${MCOP_BENCH})
    get_filename_component(bench_name ${bench_src} NAME_WE)
    add_executable(${bench_name} ${bench_src})
    target_link_libraries(${bench_name} PRIVATE mcop_core mcop_flags)
endforeach()

# ---------------------------------------------------------------------------
# Tests — one executable per tests/test_*.cpp, registered with CTest.
# ---------------------------------------------------------------------------
enable_testing()
file(GLOB MCOP_TESTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/test_*.cpp")
foreach(test_src ${MCOP_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE mcop_core mcop_flags)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ---------------------------------------------------------------------------
# Optional pybind11 module (Phase 8). Enable with -DMCOP_BUILD_PYTHON=ON.
# ---------------------------------------------------------------------------
option(MCOP_BUILD_PYTHON "Build the pybind11 Python module" OFF)
if(MCOP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
