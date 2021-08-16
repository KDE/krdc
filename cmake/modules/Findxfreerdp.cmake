# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2021 Harald Sitter <sitter@kde.org>

find_program(${CMAKE_FIND_PACKAGE_NAME}_PATH xfreerdp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(${CMAKE_FIND_PACKAGE_NAME}
    FOUND_VAR ${CMAKE_FIND_PACKAGE_NAME}_FOUND
    REQUIRED_VARS ${CMAKE_FIND_PACKAGE_NAME}_PATH
)
mark_as_advanced(${CMAKE_FIND_PACKAGE_NAME}_PATH)
