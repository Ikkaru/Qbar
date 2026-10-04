cmake_minimum_required(VERSION 3.21)
project(qbar VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Widgets QuickWidgets QuickControls2)

qt_standard_project_setup()

qt_add_executable(qbar
    src/main.cpp
    src/core/config.h
    src/core/config.cpp
    src/core/hotkeys.h
    src/core/hotkeys.cpp
    src/core/theme.h
    src/core/theme.cpp
    src/core/module_registry.h
    src/core/module_registry.cpp
    src/platform/dwm.h
    src/platform/dwm.cpp
    src/services/clock_service.h
    src/services/clock_service.cpp
    src/services/workspaces_service.h
    src/services/workspaces_service.cpp
    src/services/window_status_service.h
    src/services/window_status_service.cpp
    src/services/indicators_service.h
    src/services/indicators_service.cpp
    src/services/network_service.h
    src/services/network_service.cpp
    src/services/volume_service.h
    src/services/volume_service.cpp
    src/services/graph_service.h
    src/services/graph_service.cpp
    src/services/contrast_service.h
    src/services/contrast_service.cpp
    src/ui/popup_host.h
    src/ui/popup_host.cpp
    src/ui/bar_window.h
    src/ui/bar_window.cpp
    src/ui/bar_manager.h
    src/ui/bar_manager.cpp
    resources/resources.qrc
)

target_include_directories(qbar PRIVATE src)

target_link_libraries(qbar PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Widgets
    Qt6::QuickWidgets
    Qt6::QuickControls2
    # C++/WinRT activation for the network module. windowsapp.lib carries the
    # RoActivateInstance / RoGetActivationFactory entry points.
    windowsapp
)

set_target_properties(qbar PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
