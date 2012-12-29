
project(krdc)

if(NOT INSIDE_KDENETWORK)
    message("Not building inside KDENetwork, loading KDE CMake Macros.")

    set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

    find_package(KDE4 REQUIRED)

    include(KDE4Defaults)
    include(MacroLibrary)

    include(CheckIncludeFile)
    include(CheckIncludeFiles)
    include(CheckSymbolExists)
    include(CheckFunctionExists)
    include(CheckLibraryExists)
    include(CheckPrototypeExists)
    include(CheckTypeSize)

    set(CMAKE_REQUIRED_DEFINITIONS ${_KDE4_PLATFORM_DEFINITIONS})
    if(WIN32)
       set(CMAKE_REQUIRED_LIBRARIES ${KDEWIN32_LIBRARIES})
       set(CMAKE_REQUIRED_INCLUDES  ${KDEWIN32_INCLUDES})
    endif(WIN32)
    add_definitions(${QT_DEFINITIONS} ${QT_QTDBUS_DEFINITIONS} ${KDE4_DEFINITIONS})
    include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

    # NX support is not ready for KDE 4.2; disabled (uwolfer)
    # macro_optional_find_package(LibNXCL)
    # macro_log_feature(LIBNXCL_FOUND "libnxcl" "NX X compression client library" "http://svn.berlios.de/svnroot/repos/freenx/trunk/freenx-client/nxcl/" FALSE "1.0" "Needed to build Krdc with NX support")

    macro_optional_find_package(TelepathyQt4)
    macro_log_feature(TelepathyQt4_FOUND "telepathy-qt" "Telepathy Qt Bindings" "http://telepathy.freedesktop.org" FALSE "0.9" "Needed to build Telepathy Tubes support.")

endif(NOT INSIDE_KDENETWORK)

macro_optional_find_package(LibVNCServer)
macro_log_feature(LIBVNCSERVER_FOUND "libvncserver" "VNC Server library" "http://libvncserver.sourceforge.net/" FALSE "0.9" "Needed to build Krfb and VNC support in KRDC")

FIND_PROGRAM(FREERDP_EXECUTABLE xfreerdp)
if(FREERDP_EXECUTABLE)
    set(FREERDP_EXECUTABLE_FOUND true)
endif(FREERDP_EXECUTABLE)
macro_log_feature(FREERDP_EXECUTABLE_FOUND "freerdp" "A free Remote Desktop Protocol (RDP) Implementation" "http://www.freerdp.com" FALSE "1.0.2" "Needed for RDP support in KRDC (at runtime)")

if(TelepathyQt4_FOUND)
    add_definitions(-DTELEPATHY_SUPPORT)
    include_directories(${TELEPATHY_QT4_INCLUDE_DIR})
endif()

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

if(TelepathyQt4_FOUND)
    add_subdirectory(krdc_approver)
endif()

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5010)
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

if(TelepathyQt4_FOUND)
    set(krdc_SRCS ${krdc_SRCS}
        tubesmanager.cpp
    )
endif()

kde4_add_ui_files(krdc_SRCS
    config/general.ui
)

kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc
    ${KDE4_KFILE_LIBS}
    ${KDE4_KIO_LIBS}
    ${KDE4_KNOTIFYCONFIG_LIBS}
    ${KDE4_KCMUTILS_LIBS}
    krdccore
)

if(TelepathyQt4_FOUND)
    target_link_libraries(krdc
        ${TELEPATHY_QT4_LIBRARIES}
    )
endif()

target_link_libraries(krdc ${KDE4_KDNSSD_LIBS})

install(TARGETS krdc ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES krdcui.rc DESTINATION ${DATA_INSTALL_DIR}/krdc)
install(PROGRAMS krdc.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES pointcursor.png pointcursormask.png DESTINATION ${DATA_INSTALL_DIR}/krdc/pics)

if(TelepathyQt4_FOUND)
    configure_file(org.freedesktop.Telepathy.Client.krdc_rfb_handler.service.in
                    ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Telepathy.Client.krdc_rfb_handler.service)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Telepathy.Client.krdc_rfb_handler.service
            DESTINATION ${DBUS_SERVICES_INSTALL_DIR})

    install(FILES krdc_rfb_handler.client DESTINATION ${SHARE_INSTALL_PREFIX}/telepathy/clients/)
endif()

if(NOT INSIDE_KDENETWORK)
    macro_display_feature_log()
endif(NOT INSIDE_KDENETWORK)
