#pragma once
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/AnimationAssetBrowserPanel.h"
#include "teya/editor/panels/AnimationEditorPanel.h"
#include "teya/editor/panels/ConsolePanel.h"
#include "teya/editor/panels/CustomGameplayPanel.h"
#include "teya/editor/panels/GameViewPanel.h"
#include "teya/editor/panels/HierarchyPanel.h"
#include "teya/editor/panels/InspectorPanel.h"
#include "teya/editor/panels/MonsterEditorPanel.h"
#include "teya/editor/panels/InstanceEditorPanel.h"
#include "teya/editor/panels/PerformancePanel.h"
#include <optional>
namespace teya::editor {
class Editor {
  public:
    explicit Editor(EditorHost &);
    ~Editor();
    Editor(const Editor &) = delete;
    Editor &operator=(const Editor &) = delete;
    bool initialize();
    void shutdown();
    void draw();
    bool shouldUpdateGame();
    bool gameInputEnabled() const;
    std::optional<Vector2> gamePointerCanvasPosition() const;
    EditorContext &context();

  private:
    void menu();
    void toolbar();
    void defaultLayout();
    EditorHost &host_;
    EditorContext context_;
    GameViewPanel gameView_;
    HierarchyPanel hierarchy_;
    InspectorPanel inspector_;
    PerformancePanel performance_;
    AnimationEditorPanel animationEditor_;
    AnimationAssetBrowserPanel assets_;
    ConsolePanel console_;
    MonsterEditorPanel monsters_;
    InstanceEditorPanel instances_;
    CustomGameplayPanel customGameplay_;
    bool initialized_ = false, showDemo_ = false, firstLayout_ = false, focusGameView_ = true;
};
} // namespace teya::editor
