#pragma once
#include "teya/editor/panels/Panel.h"
#include <array>
namespace teya::editor { class PerformancePanel final: public Panel { public: void draw(EditorHost&,EditorContext&) override; private: static constexpr int Capacity=180; std::array<float,Capacity> frames_{}; int offset_=0,count_=0; }; }
