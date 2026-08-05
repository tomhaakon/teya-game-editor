#pragma once
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/Panel.h"
#include <array>
#include <optional>

namespace teya::editor {
class AnimationAssetBrowserPanel final : public Panel {
  public:
    void draw(EditorHost &, EditorContext &) override;
    std::optional<std::uint64_t> consumeOpenRequest();

  private:
    void refresh(EditorHost &);
    std::vector<EditableAnimationAssetInfo> assets_;
    std::optional<std::uint64_t> selected_, openRequest_;
    std::array<char, 128> search_{}, name_{};
    std::string message_;
    bool refreshRequested_ = true;
    enum class Modal { None, Create, Duplicate, Rename, Delete } modal_ = Modal::None;
};
} // namespace teya::editor
