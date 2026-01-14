add_library(krdc_vncplugin MODULE)

target_sources(krdc_vncplugin PRIVATE
    vnchostpreferences.cpp
    vncclientthread.cpp
    vncviewfactory.cpp
    vncview.cpp
)

ecm_qt_declare_logging_category(krdc_vncplugin
    HEADER krdc_debug.h
    IDENTIFIER KRDC
    CATEGORY_NAME KRDC
)

ki18n_wrap_ui(krdc_vncplugin
    vncpreferences.ui
)

target_link_libraries(krdc_vncplugin
    Qt::Core
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::KCMUtils
    KF6::XmlGui
    Qt::Gui
    PkgConfig::VNCCLIENT
    krdccore
)

if(HAIKU)
    target_link_libraries(krdc_vncplugin network)
endif()

add_library(kcm_krdc_vncplugin)

target_sources(kcm_krdc_vncplugin PRIVATE
    vncpreferences.cpp
)

target_link_libraries(kcm_krdc_vncplugin
    Qt::Core
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::KCMUtils
    KF6::XmlGui
    Qt::Gui
    krdccore
)

add_dependencies(kcm_krdc_vncplugin krdc_vncplugin)

install(TARGETS kcm_krdc_vncplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc/kcms)
install(TARGETS krdc_vncplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)
