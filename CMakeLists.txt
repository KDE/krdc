cmake_minimum_required(VERSION 3.16)

set (RELEASE_SERVICE_VERSION_MAJOR "21")
set (RELEASE_SERVICE_VERSION_MINOR "08")
set (RELEASE_SERVICE_VERSION_MICRO "1")

set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

project(krdc VERSION ${RELEASE_SERVICE_VERSION})

set (QT_MIN_VERSION "5.15.0")
set (KF5_MIN_VERSION "5.83.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core)

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

# use sane compile flags
add_definitions(
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_CAST_TO_ASCII
  -DQT_NO_CAST_FROM_ASCII
  -DQT_STRICT_ITERATORS
  -DQT_NO_URL_CAST_FROM_STRING
  -DQT_NO_CAST_FROM_BYTEARRAY
  -DQT_NO_KEYWORDS
  -DQT_NO_FOREACH
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

add_subdirectory(core)
add_subdirectory(vnc)
add_subdirectory(rdp)
add_subdirectory(test)

if(KF5DocTools_FOUND)
    add_subdirectory(doc)
else()
    message("KF5DocTools not found. Not building documentation.")
endif()


add_executable(krdc)

target_compile_definitions(krdc PRIVATE -DBUILD_ZEROCONF)

target_sources(krdc PRIVATE
    config/hostpreferenceslist.cpp
    config/preferencesdialog.cpp
    floatingtoolbar.cpp
    bookmarkmanager.cpp
    connectiondelegate.cpp
    remotedesktopsmodel.cpp
    systemtrayicon.cpp
    tabbedviewwidget.cpp
    factorwidget.cpp
    mainwindow.cpp
    main.cpp

    krdc.qrc
)

ki18n_wrap_ui(krdc
    config/general.ui
)

ecm_qt_declare_logging_category(krdc
    HEADER krdc_debug.h
    IDENTIFIER KRDC
    CATEGORY_NAME KRDC
    DESCRIPTION "KRDC"
    EXPORT KRDC
)

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

install(TARGETS krdc ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.krdc.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.krdc.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

ecm_qt_install_logging_categories(
    EXPORT KRDC
    DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}"
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
