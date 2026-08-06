#pragma once
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/Panel.h"
#include <vector>
namespace teya::editor {
class MonsterEditorPanel final : public Panel {
  public:
    void draw(EditorHost &, EditorContext &) override;
  private:
    void load(EditorHost &);
    std::vector<EditableMonster> monsters_;
    std::size_t selected_ = 0;
    bool loaded_ = false, dirty_ = false;
    std::string message_;
};
}
