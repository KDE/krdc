if(FREERDP_EXECUTABLE_FOUND)
    set(HAVE_XFREERDP true)
endif(FREERDP_EXECUTABLE_FOUND)

if(HAVE_XFREERDP)
    add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5012)

    include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    set(rdpplugin_SRCS
        rdphostpreferences.cpp
        rdpviewfactory.cpp
        rdpview.cpp
    )

    ki18n_wrap_ui(rdpplugin_SRCS
        rdppreferences.ui
    )

    ecm_qt_declare_logging_category(rdpplugin_SRCS HEADER krdc_debug.h IDENTIFIER KRDC CATEGORY_NAME KRDC)

    add_library(krdc_rdpplugin ${rdpplugin_SRCS})
    target_link_libraries(krdc_rdpplugin 
        Qt5::Core
        Qt5::Gui
        KF5::ConfigCore
        KF5::CoreAddons
        KF5::WindowSystem
        KF5::XmlGui
        krdccore
    )

    set(kcm_krdc_rdpplugin_SRCS
        rdppreferences.cpp
    )

    add_library(kcm_krdc_rdpplugin ${kcm_krdc_rdpplugin_SRCS})
    target_link_libraries(kcm_krdc_rdpplugin
        Qt5::Core
        Qt5::Gui
        KF5::Wallet
        KF5::ConfigCore
        KF5::CoreAddons
        KF5::XmlGui
        krdccore
    )

    kcoreaddons_desktop_to_json(krdc_rdpplugin krdc_rdp.desktop)
    kcoreaddons_desktop_to_json(kcm_krdc_rdpplugin krdc_rdp_config.desktop)

    add_dependencies(kcm_krdc_rdpplugin krdc_rdpplugin)

    install(TARGETS kcm_krdc_rdpplugin DESTINATION ${PLUGIN_INSTALL_DIR}/krdc/kcms)
    install(TARGETS krdc_rdpplugin DESTINATION ${PLUGIN_INSTALL_DIR}/krdc)

    install(FILES krdc_rdp_config.desktop DESTINATION ${SERVICES_INSTALL_DIR})

    install(FILES rdp.protocol DESTINATION ${SERVICES_INSTALL_DIR})
    install(FILES smb2rdc.desktop DESTINATION ${SERVICES_INSTALL_DIR}/ServiceMenus)
endif(HAVE_XFREERDP)
