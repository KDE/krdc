
if(Q_WS_X11)
    set(HAVE_RDESKTOP true) # TODO: implement cmake check?
endif(Q_WS_X11)

if(HAVE_RDESKTOP)
    add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5012)

    set(rdpplugin_SRCS
        rdphostpreferences.cpp
        rdpviewfactory.cpp
        rdpview.cpp
    )

    kde4_add_ui_files(rdpplugin_SRCS
        rdppreferences.ui
    )

    kde4_add_plugin(krdc_rdpplugin ${rdpplugin_SRCS})

    target_link_libraries(krdc_rdpplugin 
        ${KDE4_KDECORE_LIBS}
        ${KDE4_KDEUI_LIBS}
        krdccore
    )

    set(kcm_krdc_rdpplugin_SRCS
        rdppreferences.cpp
    )

    kde4_add_plugin(kcm_krdc_rdpplugin ${kcm_krdc_rdpplugin_SRCS})

    target_link_libraries(kcm_krdc_rdpplugin
        ${KDE4_KDEUI_LIBS}
        krdccore
    )

    install(TARGETS kcm_krdc_rdpplugin DESTINATION ${PLUGIN_INSTALL_DIR})
    install(TARGETS krdc_rdpplugin DESTINATION ${PLUGIN_INSTALL_DIR})

    install(FILES krdc_rdp.desktop DESTINATION ${SERVICES_INSTALL_DIR})
    install(FILES krdc_rdp_config.desktop DESTINATION ${SERVICES_INSTALL_DIR})

    install(FILES rdp.protocol DESTINATION ${SERVICES_INSTALL_DIR})
    install(FILES smb2rdc.desktop DESTINATION ${SERVICES_INSTALL_DIR}/ServiceMenus)
endif(HAVE_RDESKTOP)
