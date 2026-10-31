add_library(radicaleagenda_qmlplugin SHARED
  plugin.cpp
  caldavmodel.cpp
  configstore.cpp
)

target_include_directories(radicaleagenda_qmlplugin PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(radicaleagenda_qmlplugin
  Qt6::Core
  Qt6::Qml
  Qt6::Network
  Qt6::Xml
  KF6::I18n
  KF6::ConfigCore
  KF6::CoreAddons
)

set_target_properties(radicaleagenda_qmlplugin PROPERTIES
  OUTPUT_NAME "radicaleagenda"
)

# IMPORTANT: install into Qt6 QML import tree under the prefix (./test)
install(TARGETS radicaleagenda_qmlplugin
  LIBRARY DESTINATION ${KDE_INSTALL_QMLDIR}/org/nick/radicaleagenda
)

install(FILES
  radicaleagenda_plugin.json
  qmldir
  DESTINATION ${KDE_INSTALL_QMLDIR}/org/nick/radicaleagenda
)
