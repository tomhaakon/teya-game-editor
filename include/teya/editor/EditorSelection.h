#pragma once
#include "teya/editor/EditorHost.h"
namespace teya::editor { class EditorSelection { public: void select(RuntimeObjectId id); void clear(); RuntimeObjectId selected() const; bool reconcile(const std::vector<RuntimeNode>& nodes); private: RuntimeObjectId selected_ = 0; }; }
