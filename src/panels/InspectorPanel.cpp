#include "teya/editor/panels/InspectorPanel.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include <algorithm>
#include <imgui.h>

namespace teya::editor {
void InspectorPanel::draw(EditorHost &host, EditorContext &context) {
    if (!open)
        return;
    if (!ImGui::Begin("Inspector", &open)) {
        ImGui::End();
        return;
    }
    const auto id = context.selection.selected();
    if (!id) {
        ImGui::TextDisabled("Select a runtime object");
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("properties", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        for (const auto &property : host.inspectObject(id)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(property.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(property.value.c_str());
        }
        ImGui::EndTable();
    }
    if (auto collider = host.editableCollider(id)) {
        if (ImGui::CollapsingHeader("Physics / Collider")) {
            ImGui::TextColored(collider->saved ? ImVec4(.4f, 1, .4f, 1)
                                               : ImVec4(1, .7f, .2f, 1),
                               collider->saved ? "Saved" : "Unsaved *");
            Vector2 offset = collider->offset;
            Vector2 size = collider->size;
            bool changed = ImGui::DragFloat2("Offset", &offset.x, .25f, -1000, 1000, "%.2f");
            changed |= ImGui::DragFloat2("Size", &size.x, .25f, .25f, 1000, "%.2f");
            size.x = std::max(.25f, size.x);
            size.y = std::max(.25f, size.y);
            if (changed) {
                const auto result = host.applyEditableCollider(id, offset, size);
                if (!result)
                    ImGui::SetTooltip("%s", result.error.c_str());
            }
            if (ImGui::Button("Save Collider"))
                (void)host.saveEditableCollider(id);
            ImGui::SameLine();
            if (ImGui::Button("Reload"))
                (void)host.reloadEditableCollider(id);
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
                (void)host.applyEditableCollider(id, {-5, -10}, {10, 10});
            ImGui::TextDisabled("Offset is measured from the yellow player origin at the feet.");
        }
    }
    ImGui::End();
}
} // namespace teya::editor
