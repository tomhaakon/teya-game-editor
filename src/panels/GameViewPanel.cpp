#include "teya/editor/panels/GameViewPanel.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include <imgui.h>
#include <rlImGui.h>
#include <algorithm>
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

        const auto selected = c.selection.selected();
        if (selected && scale_ > 0.0f) {
            if (auto collider = h.editableCollider(selected)) {
                auto *draw = ImGui::GetWindowDrawList();
                const float left = rect_.x +
                                   (collider->ownerOrigin.x + collider->offset.x) * scale_;
                const float top = rect_.y +
                                  (collider->ownerOrigin.y + collider->offset.y) * scale_;
                const float right = left + collider->size.x * scale_;
                const float bottom = top + collider->size.y * scale_;
                const ImVec2 minimum{left, top}, maximum{right, bottom};
                constexpr float HandleSize = 10.0f;
                const ImVec2 handleMin{right - HandleSize * .5f, bottom - HandleSize * .5f};
                const ImVec2 handleMax{right + HandleSize * .5f, bottom + HandleSize * .5f};
                draw->AddRectFilled(minimum, maximum, IM_COL32(255, 70, 70, 28));
                draw->AddRect(minimum, maximum, IM_COL32(255, 75, 75, 255), 0, 0, 2.0f);
                draw->AddRectFilled(handleMin, handleMax, IM_COL32(255, 215, 65, 255));

                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const bool overHandle = mouse.x >= handleMin.x && mouse.x <= handleMax.x &&
                                        mouse.y >= handleMin.y && mouse.y <= handleMax.y;
                const bool overBody = mouse.x >= left && mouse.x <= right && mouse.y >= top &&
                                      mouse.y <= bottom;
                if (hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    colliderObject_ = selected;
                    if (overHandle) {
                        colliderResizing_ = true;
                        colliderMoving_ = false;
                    } else if (overBody) {
                        colliderMoving_ = true;
                        colliderResizing_ = false;
                        colliderGrabOffset_ = {(mouse.x - left) / scale_,
                                               (mouse.y - top) / scale_};
                    }
                }
                if ((colliderMoving_ || colliderResizing_) && colliderObject_ == selected &&
                    ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    const Vector2 canvasMouse{(mouse.x - rect_.x) / scale_,
                                              (mouse.y - rect_.y) / scale_};
                    Vector2 offset = collider->offset;
                    Vector2 size = collider->size;
                    if (colliderMoving_) {
                        offset = {canvasMouse.x - collider->ownerOrigin.x - colliderGrabOffset_.x,
                                  canvasMouse.y - collider->ownerOrigin.y - colliderGrabOffset_.y};
                    } else {
                        size = {std::max(.25f, canvasMouse.x - collider->ownerOrigin.x - offset.x),
                                std::max(.25f, canvasMouse.y - collider->ownerOrigin.y - offset.y)};
                    }
                    (void)h.applyEditableCollider(selected, offset, size);
                }
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    colliderMoving_ = false;
                    colliderResizing_ = false;
                    colliderObject_ = 0;
                }
                if (hovered_ && (overHandle || overBody))
                    ImGui::SetMouseCursor(overHandle ? ImGuiMouseCursor_ResizeNWSE
                                                     : ImGuiMouseCursor_ResizeAll);
            }
        }
    }
    ImGui::End();
}
const ViewportRect &GameViewPanel::imageRect() const { return rect_; }
bool GameViewPanel::hovered() const { return hovered_; }
bool GameViewPanel::focused() const { return focused_; }
float GameViewPanel::scale() const { return scale_; }
} // namespace teya::editor
