# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0 gio-2.0 IMPORTED_TARGET)

if(TARGET PkgConfig::GLIB AND NOT TARGET Glib::Glib)
  add_library(Glib::Glib ALIAS PkgConfig::GLIB)
endif()

mark_as_advanced(GLIB_INCLUDE_DIR GLIB_LIBRARY)
