cmake_minimum_required(VERSION 3.16)
project(gmp_python_wrapper)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
# Locate Python and add pybind11's CMake directory from the pip installation
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import pybind11, sys; sys.stdout.write(pybind11.get_cmake_dir())"
    OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
)
list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_CMAKE_DIR}")
find_package(pybind11 CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)

# Find CUDA
find_package(CUDA REQUIRED)
enable_language(CUDA)

# Find CUPTI
find_package(CUDAToolkit REQUIRED COMPONENTS cupti)
if(NOT DEFINED CUPTI_ROOT)
    if(DEFINED ENV{CUPTI_ROOT})
        set(CUPTI_ROOT $ENV{CUPTI_ROOT})
    elseif(DEFINED CUDA_TOOLKIT_ROOT_DIR)
        set(CUPTI_ROOT "${CUDA_TOOLKIT_ROOT_DIR}/extras/CUPTI")
    elseif(DEFINED ENV{CUDA_HOME})
        set(CUPTI_ROOT "$ENV{CUDA_HOME}/extras/CUPTI")
    elseif(DEFINED ENV{CUDA_PATH})
        set(CUPTI_ROOT "$ENV{CUDA_PATH}/extras/CUPTI")
    else()
        set(CUPTI_ROOT "/usr/local/cuda/extras/CUPTI")
    endif()
endif()

find_library(CUPTI_LIBRARY
    NAMES cupti
    PATHS ${CUPTI_ROOT}/lib64 ${CUPTI_ROOT}/lib
    REQUIRED
)

find_path(CUPTI_INCLUDE_DIR
    NAMES cupti.h
    PATHS ${CUPTI_ROOT}/include
    REQUIRED
)

message(STATUS "Found CUPTI: ${CUPTI_LIBRARY}")
message(STATUS "CUPTI include: ${CUPTI_INCLUDE_DIR}")

# Set include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CUPTI_INCLUDE_DIR}
    ${CUDA_INCLUDE_DIRS}
)

# Define preprocessor macros
add_definitions(-DUSE_CUPTI)

# Find all source files from the main GMP library
file(GLOB GMP_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp"
)

# Create the Python module
pybind11_add_module(gmp_py_wrapper 
    gmp_profiler_wrapper.cpp
    ${GMP_SOURCES}
)

# Link libraries
target_link_libraries(gmp_py_wrapper PRIVATE 
    ${CUPTI_LIBRARY}
    ${CUDA_LIBRARIES}
    cuda
    cudart
)

# Set properties
set_target_properties(gmp_py_wrapper PROPERTIES
    CXX_VISIBILITY_PRESET "hidden"
    CUDA_VISIBILITY_PRESET "hidden"
)

# Compiler-specific options
target_compile_definitions(gmp_py_wrapper PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO})

# Add CUDA-specific compile options
if(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL "11.0")
    target_compile_options(gmp_py_wrapper PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>)
endif()

# Set RPATH for the shared library
if(APPLE)
    set_target_properties(gmp_py_wrapper PROPERTIES
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(gmp_py_wrapper PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# Installation rules
install(TARGETS gmp_py_wrapper
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

install(FILES gmp_python.py
    DESTINATION .
)

# Create output directory for profiler results
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/output)