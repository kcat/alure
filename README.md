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
