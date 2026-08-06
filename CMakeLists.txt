cmake_minimum_required(VERSION 3.24)
project(teya_game_editor VERSION 0.1.0 LANGUAGES CXX)

option(TEYA_GAME_EDITOR_BUILD_TESTS "Build teya-game-editor pure tests" ${PROJECT_IS_TOP_LEVEL})

if(NOT TARGET Teya::Core OR NOT TARGET Teya::2D OR NOT TARGET raylib)
    message(FATAL_ERROR "teya-game-editor requires Teya::Core, Teya::2D, and raylib targets")
endif()

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.1-docking
    GIT_SHALLOW TRUE)
FetchContent_Declare(rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG Raylib_5_5
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(rlimgui)
if(NOT rlimgui_POPULATED)
    FetchContent_Populate(rlimgui)
endif()

add_library(teya_game_editor STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
    src/Editor.cpp src/EditorContext.cpp src/EditorSelection.cpp src/GameViewport.cpp src/AnimationDocument.cpp
    src/panels/GameViewPanel.cpp src/panels/HierarchyPanel.cpp
    src/panels/InspectorPanel.cpp src/panels/PerformancePanel.cpp src/panels/AnimationEditorPanel.cpp
    src/panels/ConsolePanel.cpp src/panels/AnimationAssetBrowserPanel.cpp
    src/panels/MonsterEditorPanel.cpp src/panels/InstanceEditorPanel.cpp)
add_library(Teya::GameEditor ALIAS teya_game_editor)
target_compile_features(teya_game_editor PUBLIC cxx_std_17)
set_target_properties(teya_game_editor PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(teya_game_editor
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${imgui_SOURCE_DIR} ${rlimgui_SOURCE_DIR})
target_link_libraries(teya_game_editor PUBLIC Teya::Core Teya::2D raylib)

if(TEYA_GAME_EDITOR_BUILD_TESTS)
    enable_testing()
    add_executable(teya_game_editor_tests tests/EditorTests.cpp
        src/EditorContext.cpp src/EditorSelection.cpp src/GameViewport.cpp src/AnimationDocument.cpp)
    target_compile_features(teya_game_editor_tests PRIVATE cxx_std_17)
    target_include_directories(teya_game_editor_tests PRIVATE include)
    target_link_libraries(teya_game_editor_tests PRIVATE Teya::Core Teya::2D)
    add_test(NAME teya_game_editor_tests COMMAND teya_game_editor_tests)
endif()
