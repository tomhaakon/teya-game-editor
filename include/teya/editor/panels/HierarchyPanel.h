#pragma once
#include "teya/editor/panels/Panel.h"
namespace teya::editor { class HierarchyPanel final: public Panel { public: void draw(EditorHost&,EditorContext&) override; }; }
