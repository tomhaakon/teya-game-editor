#include "teya/editor/panels/GameViewPanel.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/EditorHost.h"
#include <imgui.h>
#include <rlImGui.h>
namespace teya::editor { void GameViewPanel::draw(EditorHost& h,EditorContext& c){if(!open)return;if(ImGui::Begin("Game View",&open)){focused_=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);auto a=ImGui::GetContentRegionAvail();auto p=ImGui::GetCursorScreenPos();rect_=GameViewport::centered(h.gameCanvasWidth(),h.gameCanvasHeight(),p.x,p.y,a.x,a.y,c.settings.fitGameView);scale_=h.gameCanvasWidth()>0?rect_.width/h.gameCanvasWidth():0;ImGui::SetCursorScreenPos({rect_.x,rect_.y});auto rt=h.gameViewTexture();rlImGuiImageRect(&rt.texture,(int)rect_.width,(int)rect_.height,{0,0,(float)rt.texture.width,-(float)rt.texture.height});hovered_=ImGui::IsItemHovered();}ImGui::End();} const ViewportRect& GameViewPanel::imageRect()const{return rect_;} bool GameViewPanel::hovered()const{return hovered_;} bool GameViewPanel::focused()const{return focused_;} float GameViewPanel::scale()const{return scale_;} }
