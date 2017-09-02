cmake_minimum_required(VERSION 2.8.12)
project(krdc)

set (KDE_APPLICATIONS_VERSION_MAJOR "17")
set (KDE_APPLICATIONS_VERSION_MINOR "08")
set (KDE_APPLICATIONS_VERSION_MICRO "1")

set (KDE_APPLICATIONS_VERSION "${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}.${KDE_APPLICATIONS_VERSION_MICRO}")

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF5 REQUIRED COMPONENTS
    Config
    KCMUtils
    DNSSD
    NotifyConfig
    Notifications
    Bookmarks
    IconThemes
    XmlGui
    Completion
    Wallet
    WidgetsAddons
    NotifyConfig
    I18n
)

find_package(KF5DocTools)

include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(FeatureSummary)

# use sane compile flags
add_definitions(
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_CAST_TO_ASCII
  -DQT_NO_CAST_FROM_ASCII
  -DQT_STRICT_ITERATORS
  -DQT_NO_URL_CAST_FROM_STRING
  -DQT_NO_CAST_FROM_BYTEARRAY
  -DQT_NO_SIGNALS_SLOTS_KEYWORDS
  -DQT_USE_FAST_OPERATOR_PLUS
)
ecm_setup_version(${KDE_APPLICATIONS_VERSION}
    VARIABLE_PREFIX KRDC
    SOVERSION 5
    VERSION_HEADER krdc_version.h
)

if(WIN32)
    set(CMAKE_REQUIRED_LIBRARIES ${KDEWIN32_LIBRARIES})
    set(CMAKE_REQUIRED_INCLUDES  ${KDEWIN32_INCLUDES})
endif(WIN32)
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

find_package(LibVNCServer)
set_package_properties("libvncserver" PROPERTIES
    DESCRIPTION "VNC server / client library"
    URL "http://libvncserver.sourceforge.net/"
    PURPOSE "Needed to build VNC client support in KRDC"
)

# Needs porting
FIND_PROGRAM(FREERDP_EXECUTABLE xfreerdp)
if(FREERDP_EXECUTABLE)
    set(FREERDP_EXECUTABLE_FOUND true)
endif(FREERDP_EXECUTABLE)

set_package_properties("freerdp" PROPERTIES
        DESCRIPTION "A free Remote Desktop Protocol (RDP) Implementation"
        URL "http://www.freerdp.com"
        PURPOSE "Needed for RDP support in KRDC (at runtime)"
                                     )
# NX support is not ready for KDE 4.2; disabled (uwolfer)
# macro_optional_find_package(LibNXCL)
# macro_log_feature(LIBNXCL_FOUND "libnxcl" "NX X compression client library" "http://svn.berlios.de/svnroot/repos/freenx/trunk/freenx-client/nxcl/" FALSE "1.0" "Needed to build Krdc with NX support")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core/
    ${CMAKE_CURRENT_BINARY_DIR}/core/
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(core)
add_subdirectory(vnc)
add_subdirectory(nx)
add_subdirectory(rdp)
add_subdirectory(test)

if(KF5DocTools_FOUND)
    add_subdirectory(doc)
else()
    message("KF5DocTools not found. Not building documentation.")
endif()

add_definitions(-DBUILD_ZEROCONF)

set(krdc_SRCS
    config/hostpreferenceslist.cpp
    config/preferencesdialog.cpp
    floatingtoolbar.cpp
    bookmarkmanager.cpp
    connectiondelegate.cpp
    remotedesktopsmodel.cpp
    systemtrayicon.cpp
    tabbedviewwidget.cpp
    mainwindow.cpp
    main.cpp
)

ki18n_wrap_ui(krdc_SRCS
    config/general.ui
)

ecm_qt_declare_logging_category(krdc_SRCS HEADER krdc_debug.h IDENTIFIER KRDC CATEGORY_NAME KRDC)

add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc
    KF5::ConfigCore
    KF5::KCMUtils
    KF5::DNSSD
    KF5::NotifyConfig
    KF5::Bookmarks
    KF5::IconThemes
    KF5::XmlGui
    KF5::Completion
    KF5::WidgetsAddons
    KF5::NotifyConfig
    KF5::Notifications
    krdccore
)

install(TARGETS krdc ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES krdcui.rc DESTINATION ${KXMLGUI_INSTALL_DIR}/krdc)
install(PROGRAMS org.kde.krdc.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES pointcursor.png pointcursormask.png DESTINATION ${DATA_INSTALL_DIR}/krdc/pics)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
