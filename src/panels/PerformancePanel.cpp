#include "teya/editor/panels/PerformancePanel.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include <imgui.h>

namespace teya::editor {
void PerformancePanel::draw(EditorHost &host, EditorContext &context) {
    if (!open)
        return;
    if (ImGui::Begin("Performance", &open)) {
        const bool running = context.simulationMode() == SimulationMode::Playing ||
                             context.simulationMode() == SimulationMode::StepRequested;
        if (running) {
            const auto metrics = host.frameMetrics();
            frames_[offset_] = metrics.frameMilliseconds;
            offset_ = (offset_ + 1) % Capacity;
            if (count_ < Capacity)
                ++count_;
            ImGui::Text("FPS: %.1f", metrics.fps);
            ImGui::Text("Frame: %.2f ms", metrics.frameMilliseconds);
            ImGui::Text("Game update: %.2f ms", metrics.updateMilliseconds);
            ImGui::Text("Game draw: %.2f ms", metrics.drawMilliseconds);
            ImGui::Text("Editor: %.2f ms", metrics.editorMilliseconds);
            ImGui::Text("Canvas: %d x %d", metrics.canvasWidth, metrics.canvasHeight);
            ImGui::Text("Game View: %d x %d (%.2fx)", metrics.imageWidth, metrics.imageHeight,
                        metrics.gameViewScale);
            ImGui::Text("State: %s", metrics.currentState.c_str());
            if (!metrics.currentMap.empty())
                ImGui::Text("Map: %s", metrics.currentMap.c_str());
            if (metrics.colliderCount >= 0)
                ImGui::Text("Colliders: %d", metrics.colliderCount);
            ImGui::PlotLines("Frame time", frames_.data(), count_, offset_, nullptr, 0, 40,
                             {0, 80});
        } else {
            ImGui::TextDisabled("Performance sampling is paused while the game is not running.");
        }

        ImGui::SeparatorText("Debug Rendering");
        auto &settings = context.settings;
        ImGui::Checkbox("Player collider", &settings.showPlayerCollider);
        ImGui::Checkbox("World bounds", &settings.showWorldBounds);
        ImGui::Checkbox("Animation hitboxes", &settings.showAnimationHitboxes);
        ImGui::Checkbox("Animation sockets", &settings.showAnimationSockets);
        ImGui::Checkbox("Animation markers", &settings.showAnimationMarkers);
        ImGui::Checkbox("Player origin", &settings.showPlayerOrigin);
        host.setDebugDrawSettings({settings.showPlayerOrigin,
                                   settings.showPlayerCollider,
                                   settings.showWorldBounds,
                                   settings.showAnimationHitboxes,
                                   settings.showAnimationSockets,
                                   settings.showAnimationMarkers});
    }
    ImGui::End();
}
} // namespace teya::editor
