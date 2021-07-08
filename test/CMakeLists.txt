if(NOT NDEBUG)
    set(BUILD_TEST true)
endif()

if(NOT BUILD_TEST)
    return()
endif()

add_library(krdc_testplugin)

target_sources(krdc_testplugin PRIVATE
    testviewfactory.cpp
    testview.cpp
)

kcoreaddons_desktop_to_json(krdc_testplugin krdc_test.desktop)

target_link_libraries(krdc_testplugin
    Qt::Core
    KF5::Wallet
    KF5::ConfigCore
    KF5::I18n
    KF5::CoreAddons
    Qt::Gui
    krdccore
)

install(TARGETS krdc_testplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)
