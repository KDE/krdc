set(BUILD_KONSOLE true)
if (BUILD_KONSOLE)
  set(konsoleplugin_SRCS
      konsoleviewfactory.cpp
      konsoleview.cpp
  )

  add_library(krdc_konsoleplugin ${konsoleplugin_SRCS})

  kcoreaddons_desktop_to_json(krdc_konsoleplugin krdc_test.desktop)

  find_package(KF5 REQUIRED COMPONENTS Parts)
  target_link_libraries(krdc_konsoleplugin
          Qt5::Core
          KF5::Wallet
          KF5::ConfigCore
          KF5::Parts
          Qt5::Gui
          krdccore
      )

   install(TARGETS krdc_konsoleplugin DESTINATION ${PLUGIN_INSTALL_DIR}/krdc)
endif(BUILD_KONSOLE)
