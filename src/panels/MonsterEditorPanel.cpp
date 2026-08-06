#include "teya/editor/panels/MonsterEditorPanel.h"
#include "teya/editor/EditorContext.h"
#include <algorithm>
#include <array>
#include <imgui.h>

namespace teya::editor {
namespace {
template <std::size_t N> std::array<char, N> textBuffer(const std::string &value) {
    std::array<char, N> result{};
    std::copy_n(value.data(), std::min(value.size(), N - 1), result.data());
    return result;
}
}
void MonsterEditorPanel::load(EditorHost &host) {
    auto result = host.loadEditableMonsters();
    if (!result) { message_ = result.error; return; }
    monsters_ = std::move(result.monsters);
    selected_ = monsters_.empty() ? 0 : std::min(selected_, monsters_.size() - 1);
    loaded_ = true;
    dirty_ = false;
    message_ = "Loaded monsters";
}
void MonsterEditorPanel::draw(EditorHost &host, EditorContext &) {
    if (!open) return;
    if (!ImGui::Begin("Monsters", &open)) { ImGui::End(); return; }
    if (!loaded_) load(host);
    if (ImGui::Button("Add Monster")) {
        std::uint64_t nextId = 1;
        for (const auto &monster : monsters_) nextId = std::max(nextId, monster.id + 1);
        monsters_.push_back({nextId, "Monster " + std::to_string(nextId), "",
                             {240, 160}, {16, 16}, {255, 0, 255, 255}});
        selected_ = monsters_.size() - 1; dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save & Apply")) {
        auto result = host.saveAndApplyEditableMonsters(monsters_);
        message_ = result ? "Saved and applied" : result.error;
        if (result) dirty_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) { loaded_ = false; load(host); }
    ImGui::SameLine();
    ImGui::TextColored(dirty_ ? ImVec4(1, .7f, .2f, 1) : ImVec4(.4f, 1, .4f, 1),
                       dirty_ ? "Unsaved *" : "Saved");
    if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
    ImGui::Separator();
    ImGui::BeginChild("monster-list", {220, 0}, true);
    for (std::size_t i = 0; i < monsters_.size(); ++i)
        if (ImGui::Selectable(monsters_[i].name.c_str(), selected_ == i)) selected_ = i;
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (monsters_.empty()) ImGui::TextDisabled("Add a monster to begin.");
    else {
        selected_ = std::min(selected_, monsters_.size() - 1);
        auto &monster = monsters_[selected_];
        ImGui::PushID(static_cast<int>(monster.id));
        auto name = textBuffer<96>(monster.name);
        bool changed = false;
        if (ImGui::InputText("Name", name.data(), name.size())) { monster.name = name.data(); changed = true; }
        const char *animationLabel = monster.animationAssetPath.empty()
                                         ? "None (fallback square)"
                                         : monster.animationAssetPath.c_str();
        if (ImGui::BeginCombo("Animation asset", animationLabel)) {
            if (ImGui::Selectable("None (fallback square)", monster.animationAssetPath.empty())) {
                monster.animationAssetPath.clear(); changed = true;
            }
            for (const auto &asset : host.editableAnimationAssets())
                if (ImGui::Selectable(asset.displayName.c_str(),
                                      monster.animationAssetPath == asset.assetPath)) {
                    monster.animationAssetPath = asset.assetPath; changed = true;
                }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Create and edit assets in the Animation Editor.");
        changed |= ImGui::DragFloat2("Size", &monster.size.x, .5f, 1, 1000, "%.1f");
        monster.size.x = std::max(1.0f, monster.size.x);
        monster.size.y = std::max(1.0f, monster.size.y);
        ImGui::TextDisabled("Missing animations use the standard pink fallback.");
        dirty_ |= changed;
        if (ImGui::Button("Delete Monster")) {
            monsters_.erase(monsters_.begin() + selected_);
            if (!monsters_.empty()) selected_ = std::min(selected_, monsters_.size() - 1);
            dirty_ = true;
        }
        ImGui::PopID();
    }
    ImGui::EndGroup();
    ImGui::End();
}
} // namespace teya::editor
