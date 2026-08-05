#pragma once
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/GameViewPanel.h"
#include "teya/editor/panels/HierarchyPanel.h"
#include "teya/editor/panels/InspectorPanel.h"
#include "teya/editor/panels/PerformancePanel.h"
namespace teya::editor {
class Editor { public: explicit Editor(EditorHost&); ~Editor(); Editor(const Editor&)=delete; Editor& operator=(const Editor&)=delete; bool initialize(); void shutdown(); void draw(); bool shouldUpdateGame(); bool gameInputEnabled() const; EditorContext& context(); private: void menu(); void toolbar(); void defaultLayout(); EditorHost& host_; EditorContext context_; GameViewPanel gameView_; HierarchyPanel hierarchy_; InspectorPanel inspector_; PerformancePanel performance_; bool initialized_=false,showDemo_=false,firstLayout_=false; };
}
