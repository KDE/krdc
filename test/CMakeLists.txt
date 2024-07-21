if(NOT NDEBUG)
    set(BUILD_TEST true)
endif()

if(NOT BUILD_TEST)
    return()
endif()

add_library(krdc_testplugin MODULE)

target_sources(krdc_testplugin PRIVATE
    testviewfactory.cpp
    testview.cpp
)

target_link_libraries(krdc_testplugin
    Qt::Core
    KF6::Wallet
    KF6::ConfigCore
    KF6::I18n
    KF6::CoreAddons
    Qt::Gui
    krdccore
)

install(TARGETS krdc_testplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)
