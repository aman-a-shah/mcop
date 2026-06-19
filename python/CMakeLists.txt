# Python module build. Enabled from the top-level with -DMCOP_BUILD_PYTHON=ON.
find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)

# Locate the pip-installed pybind11's CMake config.
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
    OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE PYBIND11_FIND_RESULT)

if(NOT PYBIND11_FIND_RESULT EQUAL 0)
    message(FATAL_ERROR "pybind11 not found — install it with 'pip install pybind11'")
endif()

list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_CMAKE_DIR}")
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(mcop_pricer bindings.cpp)
target_link_libraries(mcop_pricer PRIVATE mcop_core mcop_flags)

# Drop the built extension next to the Python sources for easy importing.
set_target_properties(mcop_pricer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Register the Python binding smoke tests with CTest.
add_test(NAME test_bindings
         COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test_bindings.py")
set_tests_properties(test_bindings PROPERTIES
    DEPENDS mcop_pricer
    ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}")
