add_library(krdc_spiceplugin MODULE)

target_sources(krdc_spiceplugin PRIVATE
    spicehostpreferences.cpp
    spiceviewfactory.cpp
    spiceview.cpp
)

ecm_qt_declare_logging_category(krdc_spiceplugin
    HEADER krdc_debug.h
    IDENTIFIER KRDC
    CATEGORY_NAME KRDC
)

ki18n_wrap_ui(krdc_spiceplugin
    spicepreferences.ui
)

target_link_libraries(krdc_spiceplugin
    Qt::Core
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::KCMUtils
    KF6::XmlGui
    Qt::Gui
    Spiceglib::Spiceglib
    Glib::Glib
    krdccore
)

add_library(kcm_krdc_spiceplugin)

target_sources(kcm_krdc_spiceplugin PRIVATE
    spicepreferences.cpp
)

target_link_libraries(kcm_krdc_spiceplugin
    Qt::Core
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::KCMUtils
    KF6::XmlGui
    Qt::Gui
    krdccore
)

add_dependencies(kcm_krdc_spiceplugin krdc_spiceplugin)

install(TARGETS kcm_krdc_spiceplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc/kcms)
install(TARGETS krdc_spiceplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)
