set(BUILD_KONSOLE true)
if (NOT BUILD_KONSOLE)
    return()
endif()

add_library(krdc_konsoleplugin)

target_sources(krdc_konsoleplugin PRIVATE
    konsoleviewfactory.cpp
    konsoleview.cpp
)

kcoreaddons_desktop_to_json(krdc_konsoleplugin krdc_test.desktop)

find_package(KF5 REQUIRED COMPONENTS Parts)
target_link_libraries(krdc_konsoleplugin
        Qt::Core
        KF5::Wallet
        KF5::ConfigCore
        KF5::Parts
        Qt::Gui
        krdccore
    )

install(TARGETS krdc_konsoleplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/krdc)
