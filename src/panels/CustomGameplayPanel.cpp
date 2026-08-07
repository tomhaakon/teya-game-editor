#include "teya/editor/panels/CustomGameplayPanel.h"
#include "teya/editor/EditorContext.h"
#include <algorithm>
#include <imgui.h>
namespace teya::editor {
void CustomGameplayPanel::load(EditorHost &host) {
    features_ = host.customGameplayFeatures();
    selected_ = features_.empty() ? 0 : std::min(selected_, features_.size() - 1);
    loaded_ = true;
}
void CustomGameplayPanel::draw(EditorHost &host, EditorContext &) {
    if (!open) return;
    if (!ImGui::Begin("Custom Gameplay", &open)) { ImGui::End(); return; }
    if (!loaded_) load(host);
    if (ImGui::Button("Reload")) load(host);
    if (!message_.empty()) { ImGui::SameLine(); ImGui::TextUnformatted(message_.c_str()); }
    ImGui::Separator();
    ImGui::BeginChild("gameplay-features", {220, 0}, true);
    for (std::size_t i = 0; i < features_.size(); ++i)
        if (ImGui::Selectable(features_[i].name.c_str(), selected_ == i)) selected_ = i;
    ImGui::EndChild(); ImGui::SameLine(); ImGui::BeginGroup();
    if (features_.empty()) ImGui::TextDisabled("No custom gameplay features registered.");
    else {
        auto &feature = features_[selected_ = std::min(selected_, features_.size() - 1)];
        ImGui::TextUnformatted(feature.name.c_str());
        ImGui::Checkbox("Enabled", &feature.enabled);
        ImGui::PushItemWidth(260.0f);
        for (auto &setting : feature.settings) {
            ImGui::PushID(setting.key.c_str());
            if (setting.type == GameplaySettingType::Boolean)
                ImGui::Checkbox(setting.label.c_str(), &setting.boolValue);
            else if (setting.type == GameplaySettingType::Integer)
                ImGui::DragInt(setting.label.c_str(), &setting.intValue,
                               std::max(1.0f, setting.step), static_cast<int>(setting.minimum),
                               static_cast<int>(setting.maximum));
            else
                ImGui::DragFloat(setting.label.c_str(), &setting.floatValue, setting.step,
                                 setting.minimum, setting.maximum, "%.2f");
            ImGui::PopID();
        }
        ImGui::PopItemWidth();
        if (ImGui::Button("Save & Apply")) {
            auto result = host.saveAndApplyCustomGameplayFeature(feature);
            message_ = result ? "Saved and applied" : result.error;
        }
        ImGui::SeparatorText("Live Diagnostics");
        for (const auto &diagnostic : host.customGameplayDiagnostics(feature.id))
            ImGui::Text("%s: %s", diagnostic.label.c_str(), diagnostic.value.c_str());
    }
    ImGui::EndGroup(); ImGui::End();
}
void CustomGameplayPanel::drawOutcome(EditorHost &host) {
    for (const auto &feature : host.customGameplayFeatures()) {
        const auto actions = host.customGameplayActions(feature.id);
        if (actions.empty()) continue;
        ImGui::SetNextWindowSize({360, 0}, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                                {0.5f, 0.5f});
        const std::string title = feature.name + " Result###custom-gameplay-result";
        if (ImGui::Begin(title.c_str(), nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
            for (const auto &diagnostic : host.customGameplayDiagnostics(feature.id))
                ImGui::Text("%s: %s", diagnostic.label.c_str(), diagnostic.value.c_str());
            ImGui::Separator();
            for (std::size_t i = 0; i < actions.size(); ++i) {
                if (i) ImGui::SameLine();
                if (ImGui::Button(actions[i].label.c_str())) {
                    auto result = host.invokeCustomGameplayAction(feature.id, actions[i].key);
                    message_ = result ? "Action applied" : result.error;
                }
            }
        }
        ImGui::End();
    }
}
}
