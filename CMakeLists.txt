cmake_minimum_required(VERSION 3.16)
project(GMP LANGUAGES C CXX)

file(GLOB SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(gmp STATIC ${SRC})
target_include_directories(gmp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../NVTX/c/include>
    $<INSTALL_INTERFACE:include>
)

# Put the static lib under third_party/GMP/lib/… (handles multi-config generators too)
set(libdir "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set_target_properties(gmp PROPERTIES
  OUTPUT_NAME gmp
  ARCHIVE_OUTPUT_DIRECTORY "${libdir}"
) 

# --- CUPTI ---
find_package(CUDAToolkit REQUIRED COMPONENTS cupti)

# Some CMake/CUDA versions provide an imported target CUDA::cupti; otherwise use variables.
if (TARGET CUDA::cupti)
  target_link_libraries(gmp PUBLIC CUDA::cupti     
                                   CUDA::cuda_driver      # libcuda (driver API)
                                   CUDA::cudart           # runtime
                                   )
else()
  # Fallback names/vars provided by FindCUDAToolkit
  # CUDAToolkit_CUPTI_LIBRARY and CUDAToolkit_CUPTI_INCLUDE_DIR should exist
  target_link_libraries(gmp PUBLIC "${CUDAToolkit_CUPTI_LIBRARY}")
  target_include_directories(gmp PUBLIC "${CUDAToolkit_CUPTI_INCLUDE_DIR}")
endif()