cmake_minimum_required(VERSION 3.16)
project(GMP LANGUAGES C CXX)

file(GLOB SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(gmp STATIC ${SRC})
target_include_directories(gmp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Put the static lib under third_party/GMP/lib/… (handles multi-config generators too)
set(libdir "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set_target_properties(gmp PROPERTIES
  OUTPUT_NAME gmp
  ARCHIVE_OUTPUT_DIRECTORY "${libdir}"
)