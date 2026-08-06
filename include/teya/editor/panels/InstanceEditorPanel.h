#pragma once
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/Panel.h"
namespace teya::editor {
class InstanceEditorPanel final : public Panel {
  public: void draw(EditorHost &, EditorContext &) override;
  private:
    void load(EditorHost &);
    std::vector<EditableWorldInstance> instances_;
    std::vector<EditableMonster> masters_;
    std::size_t selected_ = 0;
    bool loaded_ = false, dirty_ = false;
    std::string message_;
};
}
