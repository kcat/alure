Alure
=====

[![Build Status](https://api.travis-ci.org/kcat/alure.svg)](https://travis-ci.org/kcat/alure)

Alure is a C++ 3D audio API. It uses OpenAL for audio rendering, and provides
common higher-level features such as file loading and decoding, buffer caching,
background streaming, and source management for virtually unlimited sound
source handles.

Features
--------

Alure supports 3D sound rendering based on standard OpenAL features. Extra 3D
sound features may be available depending on the extensions supported by OpenAL
(newer versions of OpenAL Soft provide the necessary extensions and is thus
recommended, but is not required if those features aren't of interest).

Environmental audio effects are supported with the ALC_EXT_EFX extension. This
provides multi-zone reverb, sound occlusion and obstruction (simulating sounds
being behind doors or walls), and atmospheric air absorption.

Binaural (HRTF) rendering is provided with the ALC_SOFT_HRTF extension. When
used with headphones, this provides an unparalleled sense of 3D sound
positioning. Multiple and custom profiles can also be select from to get a
closer match for different people.

Alure supports decoding audio files using external libraries: VorbisFile,
OpusFile, libFLAC, libsndfile, and libmpg123. A built-in standard wave file
reader is also available, supporting basic PCM formats. Application-defined
decoders are also supported in case the default set are insufficient.

And much more...

Building
--------

#### - Dependencies -
Before even building, Alure requires the OpenAL development files installed,
for example, through Creative's OpenAL SDK (available from openal.org) or from
OpenAL Soft. Additionally you will need a C++11 or above compliant compiler to
be able to build Alure.

These following dependencies are only needed to *automatically* support the
formats they handle;

* [ogg](https://xiph.org/ogg/) : ogg playback
* [vorbis](https://xiph.org/vorbis/) : ogg vorbis playback
* [flac](https://xiph.org/flac/) : flac playback
* [opusfile](http://opus-codec.org/) : opus playback
* [SndFile](http://www.mega-nerd.com/libsndfile/) : various multi-format playback
* [mpg123](https://www.mpg123.de/) : mpeg audio playback

Two of the packaged examples require the following dependencies to be built.

* [PhysFS](https://icculus.org/physfs/) : alure-physfs
* [dumb](https://github.com/kode54/dumb) : alure-dumb

If any dependency isn't found at build time the relevant decoders or examples
will be disabled and skipped during build.

#### - Windows -

If your are using [MinGW-w64](https://mingw-w64.org/doku.php), the easiest way
to get all of the dependencies above is to use [MSYS2](http://www.msys2.org/),
which has up-to-date binaries for all of the optional and required dependencies
above (so you don't need to build each from scratch).

Follow the MSYS2 installation guide and then look for each dependency on MSYS2
package repo and `pacman -S [packagename]` each package to acquire all
dependencies.

After acquiring all dependencies, you will need to make sure that the includes,
libraries, and binaries for each file are in your path. For most dependencies
this isn't a big deal, if you are using msys these directories are simply
`msys/mingw64/bin`, `msys/mingw64/lib` and `msys/mingw64/include`. However the
cmake file for Alure requires you to use the direct directory where OpenAL Soft
headers are located (so instead of `msys/mingw64/include`, it's
`msys/mingw64/include/AL`).

After cmake generation you should have something that looks like the following
output if you have every single dependency:

    -- Found OpenAL: C:/msys64/mingw64/lib/libopenal.dll.a
    -- Performing Test HAVE_STD_CXX11
    -- Performing Test HAVE_STD_CXX11 - Success
    -- Performing Test HAVE_WALL_SWITCH
    -- Performing Test HAVE_WALL_SWITCH - Success
    -- Performing Test HAVE_WEXTRA_SWITCH
    -- Performing Test HAVE_WEXTRA_SWITCH - Success
    -- Found OGG: C:/msys64/mingw64/lib/libogg.dll.a
    -- Found VORBIS: C:/msys64/mingw64/lib/libvorbisfile.dll.a
    -- Found FLAC: C:/msys64/mingw64/lib/libFLAC.dll.a
    -- Found OPUS: C:/msys64/mingw64/lib/libopusfile.dll.a
    -- Found SndFile: C:/msys64/mingw64/lib/libsndfile.dll.a
    -- Found MPG123: C:/msys64/mingw64/lib/libmpg123.dll.a
    -- Found PhysFS: C:/msys64/mingw64/lib/libphysfs.dll.a
    -- Found DUMB: C:/msys64/mingw64/lib/libdumb.dll.a
    -- Configuring done
    -- Generating done
    -- Build files have been written to: .../alure/cmake-build-debug


Use `make install` to install Alure library in `C:\Program Files (x86)` for it
to be available on your system. Otherwise simply run `make` to build the
library and each example you have the dependencies for. Note if you use mingw
(or mingw-w64, the name is the same for both) you may need to use
`mingw32-make.exe` instead of `make`, and make sure that file is located in
your path.  Note you may need to run `make install` as admin.

#### - Linux -

If you are using Ubuntu, many of the pre-requisites may be installed, however
you may find that many of the header files are not.  Here is the full list of
packages that must be installed. The list is in the format:

>[dependency name]: [library package name], [header package name]

* openal-soft : libopenal1, libopenal-dev
* ogg : libogg0, libogg-dev
* vorbis : libvorbis0a, libvorbis-dev
* flac : libflac8, and libflac-dev
* opusfile : libopusfule0, libopusfile-dev
* SndFile : libsndfile1, libsndfile1-dev
* mpg123 : libmpg123-0, libmpg123-dev
* physfs : libphysfs1, libphysfs1-dev
* dumb : libdumb1, libdumb1-dev

For each package pair, run `sudo apt-get install [packagename]`. After doing so
you should get a cmake output that looks something like:


    -- Found OpenAL: /usr/lib/x86_64-linux-gnu/libopenal.so
    -- Performing Test HAVE_STD_CXX11
    -- Performing Test HAVE_STD_CXX11 - Success
    -- Performing Test HAVE_WALL_SWITCH
    -- Performing Test HAVE_WALL_SWITCH - Success
    -- Performing Test HAVE_WEXTRA_SWITCH
    -- Performing Test HAVE_WEXTRA_SWITCH - Success
    -- Performing Test HAVE_GCC_DEFAULT_VISIBILITY
    -- Performing Test HAVE_GCC_DEFAULT_VISIBILITY - Success
    -- Performing Test HAVE_VISIBILITY_HIDDEN_SWITCH
    -- Performing Test HAVE_VISIBILITY_HIDDEN_SWITCH - Success
    -- Found OGG: /usr/lib/x86_64-linux-gnu/libogg.so
    -- Found VORBIS: /usr/lib/x86_64-linux-gnu/libvorbisfile.so
    -- Found FLAC: /usr/lib/x86_64-linux-gnu/libFLAC.so
    -- Found OPUS: /usr/lib/libopusfile.so
    -- Found SndFile: /usr/lib/x86_64-linux-gnu/libsndfile.so
    -- Found MPG123: /usr/lib/x86_64-linux-gnu/libmpg123.so
    -- Found PhysFS: /usr/lib/x86_64-linux-gnu/libphysfs.so
    -- Found DUMB: /usr/lib/x86_64-linux-gnu/libdumb.so
    -- Configuring done
    -- Generating done
    -- Build files have been written to: .../alure/cmake-build-debug

Use `sudo make install` to install Alure library on your system. Otherwise
simply run `make` to build the library and each example you have the
dependencies for.

#### - OSX - 

```
$ cd <path-to-repo>
$ mkdir build && cd build
$ cmake .. # -DCMAKE_INSTAL_PREFIX=<where-to-install-optionally>
$ cmake --build . -- -j4
$ make install # to install to specified destination or system default
```
