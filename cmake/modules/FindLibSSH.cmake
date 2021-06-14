# - Try to find LibSSH
# Once done this will define
#
#  LIBSSH_FOUND - system has LibSSH
#  LIBSSH_INCLUDE_DIR - the LibSSH include directory
#  LIBSSH_LIBRARIES - Link these to use LibSSH
#
# SPDX-FileCopyrightText: 2009-2014 Andreas Schneider <asn@cryptomilk.org>
#
# SPDX-License-Identifier: BSD-3-Clause

find_path(LIBSSH_INCLUDE_DIR
  NAMES
    libssh/libssh.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    ${CMAKE_INCLUDE_PATH}
    ${CMAKE_INSTALL_PREFIX}/include
)

find_library(SSH_LIBRARY
  NAMES
    ssh
    libssh
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ${CMAKE_LIBRARY_PATH}
    ${CMAKE_INSTALL_PREFIX}/lib
)

set(LIBSSH_LIBRARIES
    ${LIBSSH_LIBRARIES}
    ${SSH_LIBRARY}
)

if (LIBSSH_INCLUDE_DIR AND LibSSH_FIND_VERSION)
  file(STRINGS ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h LIBSSH_VERSION_MAJOR
    REGEX "#define[ ]+LIBSSH_VERSION_MAJOR[ ]+[0-9]+")

  # Older versions of libssh like libssh-0.2 have LIBSSH_VERSION but not LIBSSH_VERSION_MAJOR
  if (LIBSSH_VERSION_MAJOR)
    string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_MAJOR ${LIBSSH_VERSION_MAJOR})
    file(STRINGS ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h LIBSSH_VERSION_MINOR
      REGEX "#define[ ]+LIBSSH_VERSION_MINOR[ ]+[0-9]+")
    string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_MINOR ${LIBSSH_VERSION_MINOR})
    file(STRINGS ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h LIBSSH_VERSION_PATCH
      REGEX "#define[ ]+LIBSSH_VERSION_MICRO[ ]+[0-9]+")
    string(REGEX MATCH "[0-9]+" LIBSSH_VERSION_PATCH ${LIBSSH_VERSION_PATCH})

    set(LIBSSH_VERSION ${LIBSSH_VERSION_MAJOR}.${LIBSSH_VERSION_MINOR}.${LIBSSH_VERSION_PATCH})

  else (LIBSSH_VERSION_MAJOR)
    message(STATUS "LIBSSH_VERSION_MAJOR not found in ${LIBSSH_INCLUDE_DIR}/libssh/libssh.h, assuming libssh is too old")
    set(LIBSSH_FOUND FALSE)
  endif (LIBSSH_VERSION_MAJOR)
endif (LIBSSH_INCLUDE_DIR AND LibSSH_FIND_VERSION)

# If the version is too old, but libs and includes are set,
# find_package_handle_standard_args will set LIBSSH_FOUND to TRUE again,
# so we need this if() here.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibSSH
                                  REQUIRED_VARS
                                    LIBSSH_LIBRARIES
                                    LIBSSH_INCLUDE_DIR
                                  VERSION_VAR
                                    LIBSSH_VERSION)

# show the LIBSSH_INCLUDE_DIRS and LIBSSH_LIBRARIES variables only in the advanced view
mark_as_advanced(LIBSSH_INCLUDE_DIR LIBSSH_LIBRARIES)
