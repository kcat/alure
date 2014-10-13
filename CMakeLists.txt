cmake_minimum_required(VERSION 2.6)
project(alure)

include(CheckCXXCompilerFlag)

find_package(OpenAL REQUIRED)

check_cxx_compiler_flag(-std=c++11 HAVE_STD_CXX11)
if(HAVE_STD_CXX11)
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif()

include_directories(${alure_SOURCE_DIR}/include/AL ${OPENAL_INCLUDE_DIR})

set(alure_srcs src/devicemanager.cpp
               src/device.cpp
)

add_library(alure SHARED ${alure_srcs})
target_link_libraries(alure ${OPENAL_LIBRARY})

add_library(alure_s STATIC ${alure_srcs})
set_property(TARGET alure_s APPEND PROPERTY COMPILE_DEFINITIONS ALURE_STATIC_LIB)


add_executable(alure-enum examples/alure-enum.cpp)
target_link_libraries(alure-enum alure ${OPENAL_LIBRARY})


if(NOT DEFINED LIB_SUFFIX)
    set(LIB_SUFFIX "")
endif()

install(TARGETS alure
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION "lib${LIB_SUFFIX}"
        ARCHIVE DESTINATION "lib${LIB_SUFFIX}"
)
