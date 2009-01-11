
project(krdc)

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

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5010)

add_definitions(-DBUILD_ZEROCONF)
set(krdc_zeroconf_SRCS
    zeroconf/zeroconfpage.cpp
)

set(krdc_SRCS ${krdc_zeroconf_SRCS}
    config/hostpreferenceslist.cpp
    config/preferencesdialog.cpp
    floatingtoolbar.cpp
    bookmarkmanager.cpp
    remotedesktopsitem.cpp
    remotedesktopsmodel.cpp
    systemtrayicon.cpp
    mainwindow.cpp
    main.cpp
)

kde4_add_ui_files(krdc_SRCS
    config/general.ui
)

kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc
    ${KDE4_KFILE_LIBS}
    ${KDE4_KNOTIFYCONFIG_LIBS}
    ${KDE4_KUTILS_LIBS}
    krdccore
)

target_link_libraries(krdc ${KDE4_KDNSSD_LIBS})

install(TARGETS krdc ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES krdcui.rc DESTINATION ${DATA_INSTALL_DIR}/krdc)
install(FILES krdc.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES pointcursor.png pointcursormask.png DESTINATION ${DATA_INSTALL_DIR}/krdc/pics)
