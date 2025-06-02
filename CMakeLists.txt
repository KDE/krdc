cmake_minimum_required(VERSION 3.16)

set (RELEASE_SERVICE_VERSION_MAJOR "25")
set (RELEASE_SERVICE_VERSION_MINOR "04")
set (RELEASE_SERVICE_VERSION_MICRO "2")

set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

project(krdc VERSION ${RELEASE_SERVICE_VERSION})

set (QT_MIN_VERSION "6.6.0")
set (KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)
include(ECMDeprecationSettings)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(ECMInstallIcons)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui)
if (Qt6Gui_VERSION VERSION_GREATER_EQUAL "6.10.0")
    find_package(Qt6GuiPrivate ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)
endif()
if (UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(Qt6WaylandClient ${QT_MIN_VERSION} CONFIG)
    set(HAVE_WAYLAND ${Qt6WaylandClient_FOUND})
endif()

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
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
    KIO
    Crash
)

find_package(KF6StatusNotifierItem ${KF_MIN_VERSION} REQUIRED)

find_package(PlasmaActivities)
set_package_properties(PlasmaActivities PROPERTIES DESCRIPTION "KActivities libraries"
                    URL "https://www.kde.org"
                    TYPE OPTIONAL
                    PURPOSE "For tracking which remote desktops are frequently accessed"
                    )
if (PlasmaActivities_FOUND)
    set(HAVE_KACTIVITIES TRUE)
endif()
find_package(KF6DocTools)

find_package(Threads REQUIRED)

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

option(WITH_VNC "Enable VNC support." ON)
add_feature_info(VNC WITH_VNC "Building with support for the VNC protocol. Disable with -DWITH_VNC=NO")
option(WITH_RDP "Enable RDP support using FreeRDP" ON)
add_feature_info(RDP WITH_RDP "Building with support for the RDP protocol. Disable with -DWITH_RDP=NO")

ecm_set_disabled_deprecation_versions(QT 5.15.2
     KF 6.5.0
    )


set(CMAKE_CXX_STANDARD 20)

add_subdirectory(core)

if(WITH_VNC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(VNCCLIENT libvncclient REQUIRED IMPORTED_TARGET)

    find_package(LibSSH)
    set_package_properties(LibSSH PROPERTIES
        DESCRIPTION "ssh library"
        URL "https://libssh.org/"
        PURPOSE "Needed to build VNC+SSH tunnel support "
        TYPE OPTIONAL
    )
    add_subdirectory(vnc)
endif()

if(WITH_RDP)
    # Workaround https://github.com/FreeRDP/FreeRDP/issues/11072
    if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|DragonFly")
        list(APPEND CMAKE_PREFIX_PATH /usr/local/share/cmake/Modules/FreeRDP3)
        list(APPEND CMAKE_PREFIX_PATH /usr/local/share/cmake/Modules/FreeRDP-Client3)
        list(APPEND CMAKE_PREFIX_PATH /usr/local/share/cmake/Modules/WinPR3)
    endif()

    find_package(FreeRDP 3 REQUIRED)
    find_package(WinPR 3 REQUIRED)
    find_package(FreeRDP-Client 3 REQUIRED)

    MESSAGE(STATUS "Found FreeRDP version ${FreeRDP_VERSION_MAJOR}.${FreeRDP_VERSION_MINOR}.${FreeRDP_VERSION_REVISION}")

    set_package_properties(FreeRDP PROPERTIES
        DESCRIPTION "A free Remote Desktop Protocol (RDP) Implementation"
        URL "https://www.freerdp.com/"
        PURPOSE "FreeRDP provides RDP support"
        TYPE REQUIRED
    )
    set_package_properties(WinPR PROPERTIES
        DESCRIPTION "Windows Portable Runtime"
        URL "https://www.freerdp.com/"
        PURPOSE "Runtime used by FreeRDP"
        TYPE REQUIRED
    )
    set_package_properties(FreeRDP-Client PROPERTIES
        DESCRIPTION "A free Remote Desktop Protocol (RDP) Implementation, Client libraries"
        URL "https://www.freerdp.com/"
        PURPOSE "FreeRDP provides RDP support"
        TYPE REQUIRED
    )

    add_subdirectory(rdp)
endif()

add_subdirectory(test)

if(KF6DocTools_FOUND)
    add_subdirectory(doc)
else()
    message("KF6DocTools not found. Not building documentation.")
endif()


add_executable(krdc)

target_compile_definitions(krdc PRIVATE -DBUILD_ZEROCONF)

configure_file(config-kactivities.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kactivities.h)

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
    DEFAULT_SEVERITY Warning
)

target_link_libraries(krdc
    KF6::ConfigCore
    KF6::KCMUtils
    KF6::DNSSD
    KF6::NotifyConfig
    KF6::Bookmarks
    KF6::IconThemes
    KF6::XmlGui
    KF6::Completion
    KF6::WidgetsAddons
    KF6::NotifyConfig
    KF6::Notifications
    KF6::Crash
    krdccore
)

    target_link_libraries(krdc KF6::StatusNotifierItem KF6::BookmarksWidgets)

if (HAVE_KACTIVITIES)
    target_link_libraries(krdc Plasma::Activities)
endif()

install(TARGETS krdc ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.krdc.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.krdc.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

ecm_install_icons(
    ICONS sc-apps-krdc.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}
)

find_package(SharedMimeInfo)
if(SharedMimeInfo_FOUND)
    install(FILES org.kde.krdc-mime.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
    update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})
endif()

ki18n_install(po)
if(KF6DocTools_FOUND)
    kdoctools_install(po)
endif()

ecm_qt_install_logging_categories(
    EXPORT KRDC
    DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}"
)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
