include(GenerateExportHeader)

add_library(krdccore SHARED)
set_target_properties(krdccore PROPERTIES
    VERSION   ${KRDC_VERSION}
    SOVERSION ${KRDC_SOVERSION}
)

target_sources(krdccore PRIVATE
    remoteviewfactory.cpp
    remoteview.cpp
    hostpreferences.cpp
)

kconfig_add_kcfg_files(krdccore settings.kcfgc)

ecm_qt_declare_logging_category(krdccore
    HEADER krdc_debug.h
    IDENTIFIER KRDC
    CATEGORY_NAME KRDC
)

generate_export_header(krdccore BASE_NAME krdccore)

target_link_libraries(krdccore
    Qt::Core
    KF6::Wallet
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::WidgetsAddons
    KF6::I18n
    KF6::ConfigGui
    KF6::Completion
    KF6::GuiAddons
    Qt::Gui
    Qt::Widgets)

if(HAVE_WAYLAND)
    qt_generate_wayland_protocol_client_sources(krdccore
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/keyboard-shortcuts-inhibit-unstable-v1.xml"
    )
    target_sources(krdccore PRIVATE
        waylandinhibition.cpp
    )
    target_compile_definitions(krdccore PUBLIC -DHAVE_WAYLAND)
    target_link_libraries(krdccore Qt::GuiPrivate Qt::WaylandClient)
endif()

if (LIBSSH_FOUND)
    target_compile_definitions(krdccore PUBLIC -DLIBSSH_FOUND)
    target_sources(krdccore PRIVATE
        sshtunnelthread.cpp
        sshtunnelwidget.cpp
    )
    ki18n_wrap_ui(krdccore
        sshtunnelwidget.ui
    )
    target_include_directories(krdccore PRIVATE ${LIBSSH_INCLUDE_DIR})
    target_link_libraries(krdccore ${LIBSSH_LIBRARIES})
endif()

install(TARGETS krdccore ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# Install headers
set(krdccore_HDRS
    remoteviewfactory.h
    remoteview.h
    hostpreferences.h
)

install(FILES ${krdccore_HDRS} DESTINATION ${KDE_INSTALL_INCLUDEDIR}/krdc COMPONENT Devel)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/krdccore_export.h DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel)
install(FILES krdc.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
