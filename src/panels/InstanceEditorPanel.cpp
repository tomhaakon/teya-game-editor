#include "teya/editor/panels/InstanceEditorPanel.h"
#include "teya/editor/EditorContext.h"
#include <algorithm>
#include <cstring>
#include <imgui.h>
namespace teya::editor {
void InstanceEditorPanel::load(EditorHost &host) {
    auto result = host.loadEditableWorldInstances();
    if (!result) { message_ = result.error; return; }
    auto masters = host.loadEditableMonsters();
    if (!masters) { message_ = masters.error; return; }
    masters_ = std::move(masters.monsters);
    instances_ = std::move(result.instances); selected_ = 0; loaded_ = true; dirty_ = false;
    message_ = "Loaded world instances";
}
void InstanceEditorPanel::draw(EditorHost &host, EditorContext &) {
    if (!open) return;
    if (!ImGui::Begin("Instances", &open)) { ImGui::End(); return; }
    if (!loaded_) load(host);
    const bool hasPlayer = std::any_of(instances_.begin(), instances_.end(), [](const auto &i) {
        return i.kind == EditableInstanceKind::Player;
    });
    if (ImGui::Button("+ Player") && !hasPlayer) {
        std::uint64_t id = 1; for (const auto &i : instances_) id = std::max(id, i.id + 1);
        instances_.push_back({id, EditableInstanceKind::Player, 0, {240, 165}, "Player"});
        selected_ = instances_.size() - 1; dirty_ = true;
    }
    if (hasPlayer && ImGui::IsItemHovered()) ImGui::SetTooltip("Only one Player is allowed");
    ImGui::SameLine();
    if (ImGui::Button("+ Monster") && !masters_.empty()) {
        std::uint64_t id = 1; for (const auto &i : instances_) id = std::max(id, i.id + 1);
        instances_.push_back({id, EditableInstanceKind::Monster, masters_.front().id,
                              {240, 160}, "Monster " + std::to_string(id)});
        selected_ = instances_.size() - 1; dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save & Apply")) {
        auto result = host.saveAndApplyWorldInstances(instances_);
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
    ImGui::BeginChild("instance-list", {240, 0}, true);
    for (std::size_t index = 0; index < instances_.size(); ++index) {
        const auto &instance = instances_[index];
        std::string label = instance.name.empty()
                                ? (instance.kind == EditableInstanceKind::Player ? "Player"
                                                                                : "Monster")
                                : instance.name;
        label += " ##" + std::to_string(instance.id);
        if (ImGui::Selectable(label.c_str(), selected_ == index)) selected_ = index;
    }
    ImGui::EndChild(); ImGui::SameLine(); ImGui::BeginGroup();
    if (instances_.empty()) ImGui::TextDisabled("Add an instance to begin.");
    else {
        selected_ = std::min(selected_, instances_.size() - 1);
        auto &instance = instances_[selected_];
        ImGui::TextUnformatted(instance.kind == EditableInstanceKind::Player ? "Player instance" : "Monster instance");
        bool changed = false;
        if (nameBufferInstanceId_ != instance.id) {
            nameBuffer_.fill('\0');
            std::strncpy(nameBuffer_.data(), instance.name.c_str(), nameBuffer_.size() - 1);
            nameBufferInstanceId_ = instance.id;
        }
        if (ImGui::InputText("Name", nameBuffer_.data(), nameBuffer_.size())) {
            instance.name = nameBuffer_.data();
            changed = true;
        }
        if (instance.kind == EditableInstanceKind::Monster) {
            const auto current = std::find_if(masters_.begin(), masters_.end(),
                [&](const auto &m) { return m.id == instance.masterId; });
            const char *name = current == masters_.end() ? "Missing master" : current->name.c_str();
            if (ImGui::BeginCombo("Master", name)) {
                for (const auto &master : masters_)
                    if (ImGui::Selectable(master.name.c_str(), master.id == instance.masterId)) {
                        instance.masterId = master.id; changed = true;
                    }
                ImGui::EndCombo();
            }
        }
        changed |= ImGui::DragFloat2("Position", &instance.position.x, .5f, -10000, 10000, "%.1f");
        dirty_ |= changed;
        if (ImGui::Button("Delete Instance")) {
            instances_.erase(instances_.begin() + selected_);
            if (!instances_.empty()) selected_ = std::min(selected_, instances_.size() - 1);
            dirty_ = true;
        }
    }
    ImGui::EndGroup(); ImGui::End();
}
}
