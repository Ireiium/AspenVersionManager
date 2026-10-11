cmake_minimum_required(VERSION 3.19)
project(AspenVersionManager VERSION 0.1.0 LANGUAGES CXX)

find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.9)
    message(FATAL_ERROR "Use Qt 6.8.x; this project is validated with Qt 6.8.3")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AspenVersionManager requires 64-bit Windows")
endif()
enable_language(RC)

qt_standard_project_setup()

qt_add_library(appCore STATIC
    src/assoc/registry.cpp src/assoc/registry.h
    src/assoc/command.cpp src/assoc/command.h
    src/assoc/associations.cpp src/assoc/associations.h
    src/versions/versions.cpp src/versions/versions.h
    src/settings/backup.cpp src/settings/backup.h
    src/settings/language.cpp src/settings/language.h
)

qt_add_lrelease(TS_FILES src/translations/app_en.ts QM_FILES_OUTPUT_VARIABLE langFiles)
set_source_files_properties(${langFiles} PROPERTIES QT_RESOURCE_ALIAS "app_en.qm")
qt_add_resources(appCore "lang" PREFIX "/i18n" FILES ${langFiles})
target_include_directories(appCore PUBLIC src)
file(RELATIVE_PATH rootHint "${CMAKE_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(appCore PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    AVM_VERSION="${PROJECT_VERSION}" AVM_ROOT_HINT="${rootHint}")
target_link_libraries(appCore PUBLIC Qt::Core shlwapi shell32 advapi32 version ole32)
set(appUiSources
    src/pages/mainWindow.cpp src/pages/mainWindow.h
    src/pages/aboutPage.cpp src/pages/aboutPage.h
    src/ui/widgets.cpp src/ui/widgets.h
    src/ui/comboBox.cpp src/ui/comboBox.h
    src/ui/dialogs.cpp src/ui/dialogs.h
    src/settings/theme.cpp src/settings/theme.h
)
qt_add_executable(AspenVersionManager WIN32 src/main.cpp res/app.rc ${appUiSources})

qt_add_resources(AspenVersionManager "images"
    PREFIX "/images"
    BASE "res"
    FILES res/logo.png
)
target_include_directories(AspenVersionManager PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/res")

target_link_libraries(AspenVersionManager
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Concurrent
        appCore dwmapi
)
if(MSVC)
    target_compile_options(appCore PUBLIC /utf-8 /W4)
else()
    target_compile_options(appCore PUBLIC -Wall -Wextra -Wpedantic)
endif()
install(TARGETS AspenVersionManager
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET AspenVersionManager
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    NO_TRANSLATIONS
    DEPLOY_TOOL_OPTIONS --no-opengl-sw --no-system-d3d-compiler
        --no-system-dxc-compiler
        --skip-plugin-types generic,iconengines,imageformats,networkinformation,tls,styles
)
install(SCRIPT ${deploy_script})
get_filename_component(qtRoot "${Qt6_DIR}/../../.." ABSOLUTE)
install(FILES "${qtRoot}/translations/qtbase_zh_CN.qm"
    DESTINATION translations RENAME qt_zh_CN.qm)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION . COMPONENT Legal)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/licenses/" DESTINATION licenses COMPONENT Legal)

