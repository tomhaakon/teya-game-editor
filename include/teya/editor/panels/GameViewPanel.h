#pragma once
#include "teya/editor/GameViewport.h"
#include "teya/editor/panels/Panel.h"
namespace teya::editor { class GameViewPanel final: public Panel { public: void draw(EditorHost&,EditorContext&) override; const ViewportRect& imageRect() const; bool hovered() const; bool focused() const; float scale() const; private: ViewportRect rect_{}; bool hovered_=false,focused_=false; float scale_=0; }; }
