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
    KF6::XmlGui
    Qt::Gui
    PkgConfig::VNCCLIENT
    krdccore
)

if(QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(krdc_vncplugin KF6::KCMUtils)
endif()

if (LIBSSH_FOUND)
    target_compile_definitions(krdc_vncplugin PRIVATE -DLIBSSH_FOUND)
    target_sources(krdc_vncplugin PRIVATE vncsshtunnelthread.cpp)
    target_include_directories(krdc_vncplugin PRIVATE ${LIBSSH_INCLUDE_DIR})
    target_link_libraries(krdc_vncplugin ${LIBSSH_LIBRARIES})
endif()


add_library(kcm_krdc_vncplugin)

target_sources(kcm_krdc_vncplugin PRIVATE
    vncpreferences.cpp
)

if (LIBSSH_FOUND)
    target_compile_definitions(kcm_krdc_vncplugin PRIVATE -DLIBSSH_FOUND)
endif()

target_link_libraries(kcm_krdc_vncplugin
    Qt::Core
    KF6::Wallet
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::XmlGui
    Qt::Gui
    krdccore
)

if(QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kcm_krdc_vncplugin KF6::KCMUtils)
endif()

add_dependencies(kcm_krdc_vncplugin krdc_vncplugin)

install(TARGETS kcm_krdc_vncplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc/kcms)
install(TARGETS krdc_vncplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)
