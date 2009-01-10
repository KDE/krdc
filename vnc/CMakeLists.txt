
if(LIBVNCSERVER_FOUND)
    add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5011)

    include_directories(${LIBVNCCLIENT_INCLUDE_DIR})

    set(vncplugin_SRCS
        vnchostpreferences.cpp
        vncclientthread.cpp
        vncviewfactory.cpp
        vncview.cpp
    )

    kde4_add_ui_files(vncplugin_SRCS
        vncpreferences.ui
    )

    kde4_add_plugin(krdc_vncplugin ${vncplugin_SRCS})

    target_link_libraries(krdc_vncplugin 
        ${KDE4_KDECORE_LIBS}
        ${KDE4_KDEUI_LIBS}
        ${LIBVNCCLIENT_LIBRARIES}
        krdccore
    )

    if(JPEG_FOUND)
        target_link_libraries(krdc_vncplugin ${JPEG_LIBRARIES})
    endif(JPEG_FOUND)

    set(kcm_krdc_vncplugin_SRCS
        vncpreferences.cpp
    )

    kde4_add_plugin(kcm_krdc_vncplugin ${kcm_krdc_vncplugin_SRCS})

    target_link_libraries(kcm_krdc_vncplugin
        ${KDE4_KDEUI_LIBS}
        krdccore
    )

    install(TARGETS kcm_krdc_vncplugin DESTINATION ${PLUGIN_INSTALL_DIR})
    install(TARGETS krdc_vncplugin DESTINATION ${PLUGIN_INSTALL_DIR})

    install(FILES krdc_vnc.desktop DESTINATION ${SERVICES_INSTALL_DIR})
    install(FILES krdc_vnc_config.desktop DESTINATION ${SERVICES_INSTALL_DIR})

    install(FILES vnc.protocol DESTINATION ${SERVICES_INSTALL_DIR})
endif(LIBVNCSERVER_FOUND)
