cmake_minimum_required(VERSION 3.16)
project(GitClientQt VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Concurrent Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Concurrent Svg)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindowContextMenus.cpp
    src/MainWindowDialogs.cpp
    src/MainWindowGitOps.cpp
    src/MainWindowDiff.cpp
    src/Theme.cpp
    src/GitExecutor.cpp
    src/GitRepository.cpp
    src/DiffPresenter.cpp
    src/CommitModel.cpp
    src/LaneGraph.cpp
    src/Lanes.cpp
    src/WorkingTreeModel.cpp
    src/widgets/DiffViewWidget.cpp
    src/widgets/RepoPanelWidget.cpp
    src/widgets/CommitTableWidget.cpp
    src/widgets/FileTreeWidget.cpp
    src/widgets/SpellCheckHighlighter.cpp
)

set(HEADERS
    src/MainWindow.h
    src/Theme.h
    src/GitExecutor.h
    src/GitRepository.h
    src/DiffPresenter.h
    src/CommitModel.h
    src/LaneGraph.h
    src/Lanes.h
    src/WorkingTreeModel.h
    src/widgets/DiffViewWidget.h
    src/widgets/RepoPanelWidget.h
    src/widgets/CommitTableWidget.h
    src/widgets/SpellCheckHighlighter.h
    src/widgets/FileTreeWidget.h
)

set(FORMS
    src/MainWindow.ui
)

set(RESOURCES
    resources/resources.qrc
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE
      /FI"${CMAKE_CURRENT_SOURCE_DIR}/src/msvc_stdext_shim.h")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Svg
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}")

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .)

install(FILES resources/GitClientQt.desktop
        DESTINATION share/applications)
install(FILES resources/icons/appicon.svg
        DESTINATION share/pixmaps
        RENAME GitClientQt.svg)

set(CPACK_PACKAGE_NAME "GitClientQt")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "tamas.hegyi")
set(CPACK_PACKAGE_CONTACT "tamas.hegyi@adaptiverecognition.tech")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt-based Git client")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GitClientQt")
set(CPACK_PACKAGE_FILE_NAME "GitClientQt-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}")
set(CPACK_GENERATOR "TGZ;ZIP")
if (UNIX AND NOT APPLE)
  list(APPEND CPACK_GENERATOR "DEB" "RPM")
endif()
if (APPLE)
  list(APPEND CPACK_GENERATOR "DragNDrop")
endif()

include(CPack)

enable_testing()
add_subdirectory(tests)
