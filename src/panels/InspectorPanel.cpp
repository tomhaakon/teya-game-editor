#include "teya/editor/panels/InspectorPanel.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include <imgui.h>
namespace teya::editor { void InspectorPanel::draw(EditorHost& h,EditorContext& c){if(!open)return;if(ImGui::Begin("Inspector",&open)){auto id=c.selection.selected();if(!id)ImGui::TextDisabled("Select a runtime object");else if(ImGui::BeginTable("properties",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV)){for(const auto&p:h.inspectObject(id)){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(p.name.c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(p.value.c_str());}ImGui::EndTable();}}ImGui::End();} }
