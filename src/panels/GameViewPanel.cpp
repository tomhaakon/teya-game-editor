#include "teya/editor/panels/GameViewPanel.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include <imgui.h>
#include <rlImGui.h>
namespace teya::editor {
void GameViewPanel::draw(EditorHost &h, EditorContext &c) {
    if (!open)
        return;
    if (ImGui::Begin("Game View", &open)) {
        focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        auto a = ImGui::GetContentRegionAvail();
        auto p = ImGui::GetCursorScreenPos();
        rect_ = GameViewport::centered(h.gameCanvasWidth(), h.gameCanvasHeight(), p.x, p.y, a.x,
                                       a.y, c.settings.fitGameView);
        scale_ = h.gameCanvasWidth() > 0 ? rect_.width / h.gameCanvasWidth() : 0;
        ImGui::SetCursorScreenPos({rect_.x, rect_.y});
        if (c.simulationMode() == SimulationMode::Stopped) {
            const ImVec2 size{rect_.width, rect_.height};
            ImGui::InvisibleButton("##stopped-game-view", size);
            auto *draw = ImGui::GetWindowDrawList();
            const ImVec2 min{rect_.x, rect_.y};
            const ImVec2 max{rect_.x + rect_.width, rect_.y + rect_.height};
            draw->AddRectFilled(min, max, IM_COL32(24, 26, 31, 255));
            draw->AddRect(min, max, IM_COL32(72, 78, 90, 255));
            constexpr const char *message = "Game stopped";
            constexpr const char *hint = "Press Play or Restart to run the game";
            const ImVec2 messageSize = ImGui::CalcTextSize(message);
            const ImVec2 hintSize = ImGui::CalcTextSize(hint);
            const float centerX = (min.x + max.x) * 0.5f;
            const float centerY = (min.y + max.y) * 0.5f;
            draw->AddText({centerX - messageSize.x * 0.5f, centerY - messageSize.y - 4.0f},
                          IM_COL32(225, 228, 235, 255), message);
            draw->AddText({centerX - hintSize.x * 0.5f, centerY + 4.0f},
                          IM_COL32(145, 151, 164, 255), hint);
        } else {
            auto rt = h.gameViewTexture();
            rlImGuiImageRect(&rt.texture, (int)rect_.width, (int)rect_.height,
                             {0, 0, (float)rt.texture.width, -(float)rt.texture.height});
        }
        hovered_ = ImGui::IsItemHovered();
    }
    ImGui::End();
}
const ViewportRect &GameViewPanel::imageRect() const { return rect_; }
bool GameViewPanel::hovered() const { return hovered_; }
bool GameViewPanel::focused() const { return focused_; }
float GameViewPanel::scale() const { return scale_; }
} // namespace teya::editor
