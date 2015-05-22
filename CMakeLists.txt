cmake_minimum_required(VERSION 2.6)
project(alure)

set(CMAKE_MODULE_PATH "${alure_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)

find_package(OpenAL REQUIRED)

include_directories(${alure_SOURCE_DIR}/include/AL ${alure_BINARY_DIR} ${OPENAL_INCLUDE_DIR})

option(ALURE_USE_RTTI  "Enable run-time type information"  OFF)

check_cxx_compiler_flag(-std=c++11 HAVE_STD_CXX11)
if(HAVE_STD_CXX11)
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif()

check_cxx_compiler_flag(-Wall HAVE_WALL_SWITCH)
if(HAVE_WALL_SWITCH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()
check_cxx_compiler_flag(-Wextra HAVE_WEXTRA_SWITCH)
if(HAVE_WEXTRA_SWITCH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
endif()

if(NOT ALURE_USE_RTTI)
    check_cxx_compiler_flag(-fno-rtti HAVE_NO_RTTI_SWITCH)
    if(HAVE_NO_RTTI_SWITCH)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
    else()
        check_cxx_compiler_flag(/GR- HAVE_GRX_SWITCH)
        if(HAVE_GRX_SWITCH)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
        endif()
    endif()
endif()


set(alure_srcs src/devicemanager.cpp
               src/device.cpp
               src/context.cpp
               src/buffer.cpp
               src/source.cpp
               src/auxeffectslot.cpp
               src/effect.cpp
)
set(alure_libs ${OPENAL_LIBRARY})

find_package(Ogg)
if(OGG_FOUND)
    find_package(Vorbis)
    if(VORBIS_FOUND)
        include_directories(${OGG_INCLUDE_DIR} ${VORBIS_INCLUDE_DIR})
        set(alure_libs ${alure_libs} ${VORBIS_LIBRARIES} ${OGG_LIBRARIES})
        set(alure_srcs ${alure_srcs} src/decoders/vorbisfile1.cpp)
        set(HAVE_VORBISFILE 1)
    endif()
endif()

find_package(SndFile)
if(SNDFILE_FOUND)
    include_directories(${SNDFILE_INCLUDE_DIRS})
    set(alure_libs ${alure_libs} ${SNDFILE_LIBRARIES})
    set(alure_srcs ${alure_srcs} src/decoders/sndfile1.cpp)
    set(HAVE_LIBSNDFILE 1)
endif()

find_package(MPG123)
if(MPG123_FOUND)
    include_directories(${MPG123_INCLUDE_DIRS})
    set(alure_libs ${alure_libs} ${MPG123_LIBRARIES})
    set(alure_srcs ${alure_srcs} src/decoders/mpg123-1.cpp)
    set(HAVE_MPG123 1)
endif()


CONFIGURE_FILE(
    "${alure_SOURCE_DIR}/config.h.in"
    "${alure_BINARY_DIR}/config.h")


add_library(alure2 SHARED ${alure_srcs})
target_link_libraries(alure2 ${alure_libs})

add_library(alure2_s STATIC ${alure_srcs})
set_property(TARGET alure2_s APPEND PROPERTY COMPILE_DEFINITIONS ALURE_STATIC_LIB)


add_executable(alure-enum examples/alure-enum.cpp)
target_link_libraries(alure-enum alure2)

add_executable(alure-play examples/alure-play.cpp)
target_link_libraries(alure-play alure2)

add_executable(alure-stream examples/alure-stream.cpp)
target_link_libraries(alure-stream alure2)

add_executable(alure-reverb examples/alure-reverb.cpp)
target_link_libraries(alure-reverb alure2)

find_package(PhysFS)
if(PHYSFS_FOUND)
    add_executable(alure-physfs examples/alure-physfs.cpp)
    set_property(TARGET alure-physfs APPEND PROPERTY INCLUDE_DIRECTORIES "${PHYSFS_INCLUDE_DIR}")
    target_link_libraries(alure-physfs alure2 "${PHYSFS_LIBRARY}")
endif()

find_package(DUMB)
if(DUMB_FOUND)
    add_executable(alure-dumb examples/alure-dumb.cpp)
    set_property(TARGET alure-dumb APPEND PROPERTY INCLUDE_DIRECTORIES "${DUMB_INCLUDE_DIRS}")
    target_link_libraries(alure-dumb alure2 "${DUMB_LIBRARIES}")
endif()


if(NOT DEFINED LIB_SUFFIX)
    set(LIB_SUFFIX "")
endif()

install(TARGETS alure2 alure2_s
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION "lib${LIB_SUFFIX}"
        ARCHIVE DESTINATION "lib${LIB_SUFFIX}"
)
