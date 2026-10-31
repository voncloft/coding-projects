cmake_minimum_required(VERSION 3.20)
project(radicaleagenda VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default install prefix to ./test for local development.
# You can still override with -DCMAKE_INSTALL_PREFIX=...
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/test" CACHE PATH "Install prefix" FORCE)
endif()

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Xml)
find_package(KF6 REQUIRED COMPONENTS I18n Config CoreAddons)

add_subdirectory(plugin)

# Install the plasmoid package (QML + metadata + config) into ./test/share/...
install(DIRECTORY package/
        DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids/org.nick.radicaleagenda
        PATTERN ".git" EXCLUDE
)
