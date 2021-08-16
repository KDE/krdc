add_library(krdc_rdpplugin)

target_sources(krdc_rdpplugin PRIVATE
    rdphostpreferences.cpp
    rdpviewfactory.cpp
    rdpview.cpp
)

kcoreaddons_desktop_to_json(krdc_rdpplugin krdc_rdp.desktop)

ki18n_wrap_ui(krdc_rdpplugin
    rdppreferences.ui
)

ecm_qt_declare_logging_category(krdc_rdpplugin
    HEADER krdc_debug.h
    IDENTIFIER KRDC
    CATEGORY_NAME KRDC
)

target_link_libraries(krdc_rdpplugin
    Qt::Core
    Qt::Gui
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::WindowSystem
    KF5::XmlGui
    krdccore
)

add_library(kcm_krdc_rdpplugin)

kcoreaddons_desktop_to_json(kcm_krdc_rdpplugin krdc_rdp_config.desktop)

target_sources(kcm_krdc_rdpplugin PRIVATE
    rdppreferences.cpp
)

target_link_libraries(kcm_krdc_rdpplugin
    Qt::Core
    Qt::Gui
    KF5::Wallet
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::XmlGui
    krdccore
)

add_dependencies(kcm_krdc_rdpplugin krdc_rdpplugin)

install(TARGETS kcm_krdc_rdpplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc/kcms)
install(TARGETS krdc_rdpplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)

install(FILES krdc_rdp_config.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})

install(FILES smb2rdc.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/ServiceMenus)
