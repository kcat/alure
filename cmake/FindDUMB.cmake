# - Try to find DUMB
# Once done this will define
#
#  DUMB_FOUND - system has DUMB
#  DUMB_INCLUDE_DIRS - the DUMB include directory
#  DUMB_LIBRARIES - Link these to use DUMB
#
#  Copyright © 2006  Wengo
#  Copyright © 2009 Guillaume Martres
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

find_path(DUMB_INCLUDE_DIR NAMES dumb.h)

find_library(DUMB_LIBRARY NAMES dumb)

set(DUMB_INCLUDE_DIRS ${DUMB_INCLUDE_DIR})
set(DUMB_LIBRARIES ${DUMB_LIBRARY})

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DUMB_FOUND to TRUE if all
# listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DUMB DEFAULT_MSG DUMB_LIBRARY DUMB_INCLUDE_DIR)

# show the DUMB_INCLUDE_DIRS and DUMB_LIBRARIES variables only in the advanced view
mark_as_advanced(DUMB_INCLUDE_DIRS DUMB_LIBRARIES)
