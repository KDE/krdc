cmake_minimum_required(VERSION 3.16)

set (RELEASE_SERVICE_VERSION_MAJOR "23")
set (RELEASE_SERVICE_VERSION_MINOR "11")
set (RELEASE_SERVICE_VERSION_MICRO "70")

set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

project(krdc-vnc-qtonly VERSION ${RELEASE_SERVICE_VERSION})

set (QT_MIN_VERSION "5.15.2")
set (CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/qt5")

find_package(ECM REQUIRED NO_MODULE)
set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/modules ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)
include(ECMDeprecationSettings)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Widgets)
find_package(LibVNCServer)
set_package_properties(LibVNCServer PROPERTIES
    DESCRIPTION "VNC server / client library"
    URL "http://libvncserver.sourceforge.net/"
    PURPOSE "Provides core VNC functionality"
    TYPE REQUIRED
)

add_executable(krdc-vnc-qtonly)

target_compile_definitions(krdc-vnc-qtonly PRIVATE QTONLY)

target_include_directories(krdc-vnc-qtonly PRIVATE
    ${LIBVNCCLIENT_INCLUDE_DIR}
    ..
    ../../core
)

ecm_qt_declare_logging_category(krdc-vnc-qtonly
    HEADER krdc_debug.h
    IDENTIFIER KRDC
    CATEGORY_NAME KRDC
    DESCRIPTION "KRDC"
    EXPORT KRDC
)

target_link_libraries(krdc-vnc-qtonly
    Qt::Core
    Qt::Gui
    Qt::Widgets
    ${LIBVNCCLIENT_LIBRARIES}
)

target_sources(krdc-vnc-qtonly PRIVATE
    ../../core/remoteview.cpp
    ../vncview.cpp
    ../vncclientthread.cpp
    krdc_debug.cpp
    main.cpp
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
