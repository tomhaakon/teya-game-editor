#include "teya/editor/panels/HierarchyPanel.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include <imgui.h>
#include <functional>
namespace teya::editor { void HierarchyPanel::draw(EditorHost& h,EditorContext& c){if(!open)return;if(ImGui::Begin("Hierarchy",&open)){auto nodes=h.runtimeHierarchy();c.selection.reconcile(nodes);std::function<void(RuntimeObjectId)> drawChildren=[&](RuntimeObjectId parent){for(const auto& n:nodes)if(n.parentId==parent){bool has=false;for(const auto& x:nodes)has|=x.parentId==n.id;ImGuiTreeNodeFlags f=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth|(c.selection.selected()==n.id?ImGuiTreeNodeFlags_Selected:0)|(has?0:ImGuiTreeNodeFlags_Leaf);bool opened=ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(n.id)),f,"%s",n.name.c_str());if(ImGui::IsItemClicked())c.selection.select(n.id);if(opened){drawChildren(n.id);ImGui::TreePop();}}};drawChildren(0);}ImGui::End();} }
