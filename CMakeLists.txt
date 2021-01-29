cmake_minimum_required(VERSION 3.0)

set (RELEASE_SERVICE_VERSION_MAJOR "20")
set (RELEASE_SERVICE_VERSION_MINOR "12")
set (RELEASE_SERVICE_VERSION_MICRO "2")

set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

project(krdc VERSION ${RELEASE_SERVICE_VERSION})

set (KF5_MIN_VERSION "5.25.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
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
    WindowSystem
    NotifyConfig
    I18n
)

find_package(KF5DocTools)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0.0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsuggest-override" )
    endif()
endif()


# use sane compile flags
add_definitions(
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_CAST_TO_ASCII
  -DQT_NO_CAST_FROM_ASCII
  -DQT_STRICT_ITERATORS
  -DQT_NO_URL_CAST_FROM_STRING
  -DQT_NO_CAST_FROM_BYTEARRAY
  -DQT_NO_SIGNALS_SLOTS_KEYWORDS
)
ecm_setup_version(${RELEASE_SERVICE_VERSION}
    VARIABLE_PREFIX KRDC
    SOVERSION 5
    VERSION_HEADER krdc_version.h
)

if(WIN32)
    set(CMAKE_REQUIRED_LIBRARIES ${KDEWIN32_LIBRARIES})
    set(CMAKE_REQUIRED_INCLUDES  ${KDEWIN32_INCLUDES})
endif(WIN32)

find_package(LibVNCServer)
set_package_properties("libvncserver" PROPERTIES
    DESCRIPTION "VNC server / client library"
    URL "http://libvncserver.sourceforge.net/"
    PURPOSE "Needed to build VNC client support in KRDC"
)

find_package(LibSSH)
set_package_properties("libssh" PROPERTIES
    DESCRIPTION "ssh library"
    URL "https://libssh.org/"
    PURPOSE "Needed to build VNC+SSH tunnel support "
)

# Needs porting
find_program(FREERDP_EXECUTABLE xfreerdp)
if(FREERDP_EXECUTABLE)
    set(FREERDP_FOUND true) # mark the package found
    set(FREERDP_EXECUTABLE_FOUND true)
endif(FREERDP_EXECUTABLE)

# dud to allow the dep showing up in feature summary.
# note that this must be after _FOUND is set so it doesn't incorrectly
# set the metadata to not-found.
find_package(freerdp QUIET)

set_package_properties(freerdp PROPERTIES
        DESCRIPTION "A free Remote Desktop Protocol (RDP) Implementation"
        URL "https://www.freerdp.com/"
        PURPOSE "The xfreerdp binary is needed for RDP support in KRDC (at runtime)"
        TYPE RUNTIME
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core/
    ${CMAKE_CURRENT_BINARY_DIR}/core/
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(core)
add_subdirectory(vnc)
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
install(FILES org.kde.krdc.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
