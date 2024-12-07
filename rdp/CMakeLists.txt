add_library(krdc_rdpplugin MODULE)

target_sources(krdc_rdpplugin PRIVATE
    rdpcliprdr.cpp
    rdphostpreferences.cpp
    rdpviewfactory.cpp
    rdpview.cpp
    rdpsession.cpp
)

ki18n_wrap_ui(krdc_rdpplugin
    rdppreferences.ui
)

ecm_qt_declare_logging_category(krdc_rdpplugin
    HEADER krdc_debug.h
    IDENTIFIER KRDC
    CATEGORY_NAME KRDC
)

target_include_directories(krdc_rdpplugin PUBLIC ${FreeRDP_INCLUDE_DIR} ${WinPR_INCLUDE_DIR})

target_link_libraries(krdc_rdpplugin
    Qt::Core
    Qt::Gui
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::WindowSystem
    KF6::XmlGui
    KF6::KIOWidgets
    Threads::Threads
    krdccore
    freerdp
    freerdp-client
    winpr
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
    KF6::Wallet
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::XmlGui
    KF6::KIOWidgets
    krdccore
)

if(HAIKU)
    target_link_libraries(kcm_krdc_rdpplugin network)
endif()

if(QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kcm_krdc_rdpplugin KF6::KCMUtils)
endif()

add_dependencies(kcm_krdc_rdpplugin krdc_rdpplugin)

install(TARGETS kcm_krdc_rdpplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc/kcms)
install(TARGETS krdc_rdpplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)

# install(FILES smb2rdc.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus)
