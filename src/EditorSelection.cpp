#include "teya/editor/EditorSelection.h"
#include <algorithm>
namespace teya::editor { void EditorSelection::select(RuntimeObjectId id){selected_=id;} void EditorSelection::clear(){selected_=0;} RuntimeObjectId EditorSelection::selected()const{return selected_;} bool EditorSelection::reconcile(const std::vector<RuntimeNode>& n){if(selected_==0)return true;if(std::any_of(n.begin(),n.end(),[&](const auto& x){return x.id==selected_;}))return true;clear();return false;} }
