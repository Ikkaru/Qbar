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
    src/platform/autostart.h
    src/platform/autostart.cpp
    src/platform/single_instance.h
    src/platform/single_instance.cpp
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

# --- deployment --------------------------------------------------------------
# Without this the exe cannot start at all outside a Qt-enabled shell: it exits
# with 0xC0000135 STATUS_DLL_NOT_FOUND, silently, because it is built as a GUI
# subsystem app with no console to print anything. windeployqt copies the Qt
# runtime next to the binary and config.json goes with it, which is what makes
# the bar relocatable and what autostart needs in order to work at all.
#
# A manual target rather than POST_BUILD: it copies ~30 MB and takes several
# seconds, which is not something to pay on every rebuild.
find_program(WINDEPLOYQT windeployqt HINTS "${QT6_INSTALL_PREFIX}/bin" NO_CACHE)

if(WINDEPLOYQT)
    add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/config.json"
                "$<TARGET_FILE_DIR:qbar>/config.json"
        # --qmldir is not optional. windeployqt finds QML imports by scanning loose
        # .qml files, and ours all live inside resources.qrc inside the exe, which
        # it does not read. Without this it copies no QML plugins at all and the
        # deployed bar comes up with a window but no content:
        #     QML error: module "QtQuick.Layouts" is not installed
        # The path is src/qml because that is the root holding Bar.qml and the
        # modules/ and popups/ subdirectories beneath it.
        COMMAND ${WINDEPLOYQT}
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/src/qml"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --compiler-runtime
                "$<TARGET_FILE:qbar>"
        COMMENT "Copying Qt runtime next to qbar.exe so it runs without Qt on PATH"
        VERBATIM
    )
else()
    message(WARNING "windeployqt not found - qbar.exe will not start outside a Qt shell")
endif()

# --- tests -------------------------------------------------------------------
# QTest from the Qt already in the toolchain, so no new dependency. The
# services under test are plain QObjects, so every binary here runs guiless.
enable_testing()

find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

set(QBAR_TESTABLE_SOURCES
    src/core/config.h
    src/core/config.cpp
    src/core/hotkeys.h
    src/core/hotkeys.cpp
    src/core/theme.h
    src/core/theme.cpp
    src/core/module_registry.h
    src/core/module_registry.cpp
    src/platform/autostart.h
    src/platform/autostart.cpp
)

# A tiny helper rather than repeating the list per test.
function(qbar_add_test name)
    qt_add_executable(${name} ${ARGN} ${QBAR_TESTABLE_SOURCES})
    target_include_directories(${name} PRIVATE src)
    # Gui and Qml are needed by the sources under test (QColor in theme.h,
    # QQmlContext in module_registry.h), not by the tests themselves.
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
    # Keep the console window: without this the test binary is built as a GUI app
    # and its output vanishes, which is the opposite of what a test runner wants.
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

qbar_add_test(test_config tests/test_config.cpp)
qbar_add_test(test_autostart tests/test_autostart.cpp)
# WORKING_DIRECTORY so tests that read the real resources/config.json resolve it
# relative to the source tree, not to wherever ctest was invoked from.
qbar_add_test(test_theme tests/test_theme.cpp)
set_tests_properties(test_theme PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# The registry test links resources.qrc so it can prove that each id it resolves
# to actually exists inside the resource bundle. That is the check that would
# have caught "graph" being mapped twice, and media/tray/notifications/volume
# being listed with no QML file at all.
qt_add_executable(test_module_registry
    tests/test_module_registry.cpp
    ${QBAR_TESTABLE_SOURCES}
    resources/resources.qrc
)
target_include_directories(test_module_registry PRIVATE src)
target_link_libraries(test_module_registry PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
set_target_properties(test_module_registry PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)
add_test(NAME test_module_registry
         COMMAND test_module_registry
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
