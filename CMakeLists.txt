cmake_minimum_required(VERSION 2.6)
project(alure)

set(CMAKE_MODULE_PATH "${alure_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)

find_package(OpenAL REQUIRED)

include_directories(${alure_SOURCE_DIR}/include/AL ${alure_BINARY_DIR} ${OPENAL_INCLUDE_DIR})

check_cxx_compiler_flag(-std=c++11 HAVE_STD_CXX11)
if(HAVE_STD_CXX11)
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif()

set(alure_srcs src/devicemanager.cpp
               src/device.cpp
               src/context.cpp
)
set(alure_libs ${OPENAL_LIBRARY})

find_package(SndFile)
if(SNDFILE_FOUND)
    include_directories(${SNDFILE_INCLUDE_DIRS})
    set(alure_libs ${alure_libs} ${SNDFILE_LIBRARIES})
    set(alure_srcs ${alure_srcs} src/decoders/sndfile1.cpp)
    set(HAVE_LIBSNDFILE 1)
endif()


CONFIGURE_FILE(
    "${alure_SOURCE_DIR}/config.h.in"
    "${alure_BINARY_DIR}/config.h")


add_library(alure SHARED ${alure_srcs})
target_link_libraries(alure ${alure_libs})

add_library(alure_s STATIC ${alure_srcs})
set_property(TARGET alure_s APPEND PROPERTY COMPILE_DEFINITIONS ALURE_STATIC_LIB)


add_executable(alure-enum examples/alure-enum.cpp)
target_link_libraries(alure-enum alure)


if(NOT DEFINED LIB_SUFFIX)
    set(LIB_SUFFIX "")
endif()

install(TARGETS alure
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION "lib${LIB_SUFFIX}"
        ARCHIVE DESTINATION "lib${LIB_SUFFIX}"
)
