cmake_minimum_required(VERSION 3.24)
project(OTEditor VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Widgets Test)
get_filename_component(TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(TOOLCHAIN_ROOT "${TOOLCHAIN_BIN_DIR}" DIRECTORY)
find_path(LZMA_INCLUDE_DIR lzma.h HINTS "${TOOLCHAIN_ROOT}/opt/include")
find_library(LZMA_LIBRARY NAMES lzma HINTS "${TOOLCHAIN_ROOT}/opt/lib")
if(NOT LZMA_INCLUDE_DIR OR NOT LZMA_LIBRARY)
    message(FATAL_ERROR "liblzma is required for Object Builder OBD support")
endif()
qt_standard_project_setup(REQUIRES 6.5)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()
file(GLOB OTFORMATS_SOURCES CONFIGURE_DEPENDS libs/otformats/*.cpp libs/otformats/*.h)
add_library(otformats STATIC ${OTFORMATS_SOURCES})
target_include_directories(otformats PUBLIC libs/otformats)
target_link_libraries(otformats PUBLIC Qt6::Core Qt6::Gui Qt6::Qml)
add_library(editorcore STATIC
    src/editorbackend.cpp src/editorbackend.h
    src/editorbackend_slicer.cpp src/editorbackend_export.cpp src/editorbackend_sheet.cpp src/editorbackend_merge.cpp src/editorbackend_convert.cpp
    src/obdcodec.cpp src/obdcodec.h src/editorbackend_obd.cpp
    src/projectmodel.cpp src/projectmodel.h
    src/updateservice.cpp src/updateservice.h src/updateversion.cpp src/updateversion.h)
target_include_directories(editorcore PUBLIC src)
target_include_directories(editorcore PRIVATE "${LZMA_INCLUDE_DIR}")
target_link_libraries(editorcore PUBLIC otformats Qt6::Quick Qt6::Network "${LZMA_LIBRARY}")
qt_add_executable(OTEditor src/main.cpp)
add_library(aispritecore STATIC src/aispriteservice.cpp src/aispriteservice.h)
target_include_directories(aispritecore PUBLIC src)
target_link_libraries(aispritecore PUBLIC Qt6::Gui Qt6::Network)
qt_add_qml_module(OTEditor URI OTEditor VERSION 1.0 QML_FILES
    qml/Main.qml qml/FluentToolBar.qml
    qml/ItemInspector.qml qml/ServerAttributesPanel.qml qml/PaletteColorDialog.qml qml/PixelEditorDialog.qml qml/Panel.qml qml/Tool.qml qml/Checker.qml qml/AssetToggle.qml
    qml/SlicerWindow.qml
    qml/LookTypeDialog.qml qml/ObjectViewerDialog.qml qml/AnimationEditorDialog.qml
    qml/FrameDurationsOptimizerDialog.qml qml/FrameDurationsConverterDialog.qml qml/FrameGroupsConverterDialog.qml
    qml/SpritesOptimizerDialog.qml qml/PreferencesDialog.qml qml/MergeProjectDialog.qml qml/ConvertProjectDialog.qml qml/XmlAttributesDialog.qml qml/AboutDialog.qml
    qml/FindObjectsDialog.qml qml/ImportClientObjectsDialog.qml qml/ReplaceItemDialog.qml
    qml/CreateAssetFilesDialog.qml qml/ExportObjectsDialog.qml qml/OpenAssetFilesDialog.qml
    qml/CompileProgressPopup.qml qml/AiSpriteGeneratorDialog.qml
    qml/CompileAssetFilesDialog.qml
    qml/PreviewSidebar.qml qml/ObjectsBrowserPanel.qml qml/SpritesBrowserPanel.qml
    qml/EditorLogPanel.qml)
qt_add_resources(OTEditor branding PREFIX "/" FILES assets/Midhem.png assets/ObjectBuilder-versions.xml assets/icons/oteditor.ico
    assets/ui/new.svg assets/ui/open.svg assets/ui/save.svg assets/ui/add.svg assets/ui/copy.svg assets/ui/remove.svg assets/ui/undo.svg assets/ui/redo.svg assets/ui/play.svg assets/ui/pause.svg assets/ui/settings.svg assets/ui/chevron.svg)
target_link_libraries(OTEditor PRIVATE editorcore aispritecore Qt6::QuickControls2)
target_compile_definitions(OTEditor PRIVATE OTEDITOR_VERSION="${PROJECT_VERSION}")
set_target_properties(OTEditor PROPERTIES WIN32_EXECUTABLE TRUE RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(WIN32)
    target_sources(OTEditor PRIVATE assets/oteditor.rc)
    qt_add_executable(OTEditorUpdater src/updater/main.cpp)
    target_link_libraries(OTEditorUpdater PRIVATE Qt6::Widgets)
    set_target_properties(OTEditorUpdater PROPERTIES WIN32_EXECUTABLE TRUE RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    install(TARGETS OTEditorUpdater RUNTIME DESTINATION .)
endif()
include(CTest)
if(BUILD_TESTING)
    qt_add_executable(editor_tests tests/editor_tests.cpp)
    qt_add_resources(editor_tests branding_test PREFIX "/" FILES assets/Midhem.png assets/ObjectBuilder-versions.xml assets/ui/add.svg assets/ui/chevron.svg assets/ui/copy.svg assets/ui/new.svg assets/ui/open.svg assets/ui/pause.svg assets/ui/play.svg assets/ui/redo.svg assets/ui/remove.svg assets/ui/save.svg assets/ui/settings.svg assets/ui/undo.svg)
    target_link_libraries(editor_tests PRIVATE editorcore aispritecore Qt6::Test Qt6::QuickControls2)
    set_target_properties(editor_tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME editor_roundtrip COMMAND editor_tests -o "${CMAKE_BINARY_DIR}/editor-test-results.txt,txt")
    qt_add_executable(ai_sprite_tests tests/ai_sprite_tests.cpp)
    target_link_libraries(ai_sprite_tests PRIVATE aispritecore editorcore Qt6::Test)
    set_target_properties(ai_sprite_tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME ai_sprite_generator COMMAND ai_sprite_tests -o "${CMAKE_BINARY_DIR}/ai-test-results.txt,txt")
    if(WIN32)
        set_tests_properties(editor_roundtrip ai_sprite_generator PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endif()
install(TARGETS OTEditor RUNTIME DESTINATION .)
