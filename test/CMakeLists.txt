if(NOT NDEBUG)
    set(BUILD_TEST true)
endif()

if(NOT BUILD_TEST)
    return()
endif()

set(testplugin_SRCS
    testviewfactory.cpp
    testview.cpp
)

add_library(krdc_testplugin ${testplugin_SRCS})

kcoreaddons_desktop_to_json(krdc_testplugin krdc_test.desktop)

target_link_libraries(krdc_testplugin
    Qt5::Core
    KF5::Wallet
    KF5::ConfigCore
    KF5::I18n
    KF5::CoreAddons
    Qt5::Gui
    krdccore
)
install(TARGETS krdc_testplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)
