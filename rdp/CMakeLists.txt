add_library(krdc_rdpplugin)

target_sources(krdc_rdpplugin PRIVATE
    rdphostpreferences.cpp
    rdpviewfactory.cpp
    rdpview.cpp
)

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
    KF${QT_MAJOR_VERSION}::ConfigCore
    KF${QT_MAJOR_VERSION}::CoreAddons
    KF${QT_MAJOR_VERSION}::WindowSystem
    KF${QT_MAJOR_VERSION}::XmlGui
    krdccore
)

if(QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(krdc_rdpplugin KF6::KCMUtils)
endif()

add_library(kcm_krdc_rdpplugin)

target_sources(kcm_krdc_rdpplugin PRIVATE
    rdppreferences.cpp
)

target_link_libraries(kcm_krdc_rdpplugin
    Qt::Core
    Qt::Gui
    KF${QT_MAJOR_VERSION}::Wallet
    KF${QT_MAJOR_VERSION}::ConfigCore
    KF${QT_MAJOR_VERSION}::CoreAddons
    KF${QT_MAJOR_VERSION}::XmlGui
    krdccore
)

if(QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kcm_krdc_rdpplugin KF6::KCMUtils)
endif()

add_dependencies(kcm_krdc_rdpplugin krdc_rdpplugin)

install(TARGETS kcm_krdc_rdpplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc/kcms)
install(TARGETS krdc_rdpplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)

install(FILES smb2rdc.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus)
