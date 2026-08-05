#include "teya/editor/panels/ConsolePanel.h"
#include "teya/editor/EditorHost.h"
#include <imgui.h>
#include <teya/core/Profile.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace teya::editor {
namespace { constexpr std::size_t MaxLines=2000; constexpr std::uintmax_t InitialTailBytes=256*1024; }

void ConsolePanel::append(std::string line){if(!line.empty()&&line.back()=='\r')line.pop_back();if(lines_.size()==MaxLines)lines_.pop_front();lines_.push_back(std::move(line));scrollToBottom_=true;}

void ConsolePanel::poll(EditorHost&host){TEYA_PROFILE_ZONE_NAMED("ConsolePanel::poll");const auto newPath=host.editorLogPath();if(newPath!=path_){path_=newPath;offset_=0;partial_.clear();lines_.clear();}if(path_.empty()){error_="The host did not provide a log file";return;}host.flushEditorLog();std::error_code ec;const auto size=std::filesystem::file_size(path_,ec);if(ec){error_="Waiting for "+path_;return;}if(size<offset_){offset_=0;partial_.clear();lines_.clear();}if(offset_==0&&size>InitialTailBytes)offset_=size-InitialTailBytes;if(size==offset_){error_.clear();return;}std::ifstream input(path_,std::ios::binary);if(!input){error_="Could not open "+path_;return;}input.seekg(static_cast<std::streamoff>(offset_));std::string added((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());offset_+=added.size();if(added.empty())return;partial_+=added;std::size_t begin=0;while(true){const auto newline=partial_.find('\n',begin);if(newline==std::string::npos)break;append(partial_.substr(begin,newline-begin));begin=newline+1;}partial_.erase(0,begin);error_.clear();}

void ConsolePanel::draw(EditorHost&host,EditorContext&){if(!open)return;TEYA_PROFILE_ZONE_NAMED("ConsolePanel::draw");if(!ImGui::Begin("Console",&open)){ImGui::End();return;}if(ImGui::Button(paused_?"Resume":"Pause"))paused_=!paused_;ImGui::SameLine();if(ImGui::Button("Clear view")){lines_.clear();partial_.clear();}ImGui::SameLine();ImGui::Checkbox("Follow",&autoScroll_);ImGui::SameLine();ImGui::TextDisabled("%zu / %zu lines",lines_.size(),MaxLines);if(!paused_&&ImGui::GetTime()>=nextPoll_){poll(host);nextPoll_=ImGui::GetTime()+.20;}if(!error_.empty())ImGui::TextDisabled("%s",error_.c_str());ImGui::Separator();ImGui::BeginChild("console-lines",{0,0},false,ImGuiWindowFlags_HorizontalScrollbar);ImGuiListClipper clipper;clipper.Begin(static_cast<int>(lines_.size()));while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto&line=lines_[static_cast<std::size_t>(i)];ImVec4 color{.82f,.84f,.88f,1};if(line.find("[ERROR]")!=std::string::npos)color={1,.35f,.35f,1};else if(line.find("[WARNING]")!=std::string::npos)color={1,.72f,.25f,1};else if(line.find("[DEBUG]")!=std::string::npos)color={.55f,.72f,1,1};ImGui::PushStyleColor(ImGuiCol_Text,color);ImGui::TextUnformatted(line.c_str());ImGui::PopStyleColor();}if(autoScroll_&&scrollToBottom_){ImGui::SetScrollHereY(1);scrollToBottom_=false;}ImGui::EndChild();ImGui::End();}
}
