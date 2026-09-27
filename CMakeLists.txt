cmake_minimum_required(VERSION 3.24)
project(OTEditor VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
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
    src/editorbackend_slicer.cpp src/editorbackend_export.cpp
    src/projectmodel.cpp src/projectmodel.h)
target_include_directories(editorcore PUBLIC src)
target_link_libraries(editorcore PUBLIC otformats Qt6::Quick)
qt_add_executable(OTEditor src/main.cpp)
qt_add_qml_module(OTEditor URI OTEditor VERSION 1.0 QML_FILES
    qml/Main.qml
    qml/ItemInspector.qml qml/PaletteColorDialog.qml qml/Panel.qml qml/Tool.qml qml/Checker.qml qml/AssetToggle.qml
    qml/SlicerWindow.qml
    qml/LookTypeDialog.qml qml/ObjectViewerDialog.qml qml/AnimationEditorDialog.qml
    qml/FrameDurationsOptimizerDialog.qml qml/FrameDurationsConverterDialog.qml qml/FrameGroupsConverterDialog.qml
    qml/SpritesOptimizerDialog.qml qml/AboutDialog.qml
    qml/FindObjectsDialog.qml qml/ImportClientObjectsDialog.qml qml/ReplaceItemDialog.qml
    qml/CreateAssetFilesDialog.qml qml/ExportObjectsDialog.qml qml/OpenAssetFilesDialog.qml
    qml/CompileProgressPopup.qml
    qml/PreviewSidebar.qml qml/ObjectsBrowserPanel.qml qml/SpritesBrowserPanel.qml
    qml/EditorLogPanel.qml)
qt_add_resources(OTEditor branding PREFIX "/" FILES assets/Midhem.png)
target_link_libraries(OTEditor PRIVATE editorcore Qt6::QuickControls2)
set_target_properties(OTEditor PROPERTIES WIN32_EXECUTABLE TRUE RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(CTest)
if(BUILD_TESTING)
    qt_add_executable(editor_tests tests/editor_tests.cpp)
    qt_add_resources(editor_tests branding_test PREFIX "/" FILES assets/Midhem.png)
    target_link_libraries(editor_tests PRIVATE editorcore Qt6::Test Qt6::QuickControls2)
    set_target_properties(editor_tests PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME editor_roundtrip COMMAND editor_tests)
endif()
install(TARGETS OTEditor RUNTIME DESTINATION .)
