#pragma once
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/Panel.h"
namespace teya::editor {
class CustomGameplayPanel final : public Panel {
  public: void draw(EditorHost &, EditorContext &) override;
    void drawOutcome(EditorHost &);
  private:
    void load(EditorHost &);
    std::vector<CustomGameplayFeature> features_;
    std::size_t selected_ = 0;
    bool loaded_ = false;
    std::string message_;
};
}
