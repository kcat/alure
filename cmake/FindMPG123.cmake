# - Find mpg123
# Find the native mpg123 includes and library
#
#  MPG123_INCLUDE_DIRS - where to find mpg123.h
#  MPG123_LIBRARIES    - List of libraries when using mpg123.
#  MPG123_FOUND        - True if mpg123 found.

IF(MPG123_INCLUDE_DIR AND MPG123_LIBRARY)
  # Already in cache, be silent
  SET(MPG123_FIND_QUIETLY TRUE)
ENDIF(MPG123_INCLUDE_DIR AND MPG123_LIBRARY)

FIND_PATH(MPG123_INCLUDE_DIR mpg123.h
          PATHS "${MPG123_DIR}"
          PATH_SUFFIXES include
          )

FIND_LIBRARY(MPG123_LIBRARY NAMES mpg123 mpg123-0
             PATHS "${MPG123_DIR}"
             PATH_SUFFIXES lib
             )

# handle the QUIETLY and REQUIRED arguments and set MPG123_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MPG123 DEFAULT_MSG MPG123_LIBRARY MPG123_INCLUDE_DIR)

IF(MPG123_FOUND)
    SET(MPG123_LIBRARIES ${MPG123_LIBRARY})
    SET(MPG123_INCLUDE_DIRS ${MPG123_INCLUDE_DIR})
ENDIF(MPG123_FOUND)
