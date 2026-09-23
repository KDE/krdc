# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPICEGLIB REQUIRED spice-client-glib-2.0 IMPORTED_TARGET)

if(TARGET PkgConfig::SPICEGLIB AND NOT TARGET Spiceglib::Spiceglib)
    add_library(Spiceglib::Spiceglib ALIAS PkgConfig::SPICEGLIB)
endif()

mark_as_advanced(SPICEGLIB_INCLUDE_DIR SPICEGLIB_LIBRARY)