
include(GenerateExportHeader)

set(krdccore_SRCS
    remoteviewfactory.cpp
    remoteview.cpp
    hostpreferences.cpp
)

kconfig_add_kcfg_files(krdccore_SRCS settings.kcfgc)

ecm_qt_declare_logging_category(krdccore_SRCS HEADER krdc_debug.h IDENTIFIER KRDC CATEGORY_NAME KRDC)

add_library(krdccore SHARED ${krdccore_SRCS})

kcoreaddons_desktop_to_json(krdccore krdc_plugin.desktop)
generate_export_header(krdccore BASE_NAME krdccore)

target_link_libraries(krdccore
    Qt5::Core
    KF5::Wallet
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::WidgetsAddons
    KF5::I18n
    KF5::ConfigGui
    KF5::Completion
    Qt5::Gui
    Qt5::Widgets)

set_target_properties(krdccore PROPERTIES VERSION ${KRDC_VERSION} SOVERSION ${KRDC_SOVERSION})

install(TARGETS krdccore ${INSTALL_TARGETS_DEFAULT_ARGS})

# Install headers
set(krdccore_HDRS
    remoteviewfactory.h
    remoteview.h
    hostpreferences.h
)

install(FILES ${krdccore_HDRS} DESTINATION ${INCLUDE_INSTALL_DIR}/krdc COMPONENT Devel)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/krdccore_export.h DESTINATION ${KDE_INSTALL_INCLUDEDIR} COMPONENT Devel)
install(FILES krdc.kcfg DESTINATION ${KCFG_INSTALL_DIR})
