
include_directories( ${CMAKE_SOURCE_DIR}/krdc  )

########### next target ###############

set(krdc_rdp_SRCS 
    krdpview.cpp
    rdphostpref.cpp
    rdpprefs.cpp
 )

kde4_automoc(${krdc_rdp_SRCS})

kde4_add_ui_files(krdc_rdp_SRCS rdpprefs.ui )

kde4_add_library(rdp STATIC ${krdc_rdp_SRCS})

target_link_libraries( rdp ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY} )
########### install files ###############





