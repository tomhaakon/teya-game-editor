#include "teya/editor/Editor.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <rlImGui.h>
#include <teya/core/Profile.h>
namespace teya::editor {
Editor::Editor(EditorHost &h) : host_(h) {}
Editor::~Editor() { shutdown(); }
bool Editor::initialize() {
    if (initialized_)
        return true;
    rlImGuiBeginInitImGui();
    ImGui::StyleColorsDark();
    auto &io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig fontConfig;
    fontConfig.SizePixels = 20.0f;
    fontConfig.PixelSnapH = true;
    io.Fonts->AddFontDefault(&fontConfig);
    rlImGuiEndInitImGui();
    ImGui::GetStyle().ScaleAllSizes(1.25f);
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    firstLayout_ = io.IniFilename && GetFileLength(io.IniFilename) <= 0;
    initialized_ = true;
    return true;
}
void Editor::shutdown() {
    if (initialized_) {
        rlImGuiShutdown();
        initialized_ = false;
    }
}
void Editor::menu() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit"))
                host_.requestExit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Game View", nullptr, &gameView_.open);
            ImGui::MenuItem("Hierarchy", nullptr, &hierarchy_.open);
            ImGui::MenuItem("Inspector", nullptr, &inspector_.open);
            ImGui::MenuItem("Performance", nullptr, &performance_.open);
            ImGui::MenuItem("Animation Editor", nullptr, &animationEditor_.open);
            ImGui::MenuItem("Animation Timeline", nullptr, &animationEditor_.timelineOpen());
            ImGui::MenuItem("Assets", nullptr, &assets_.open);
            ImGui::MenuItem("Console", nullptr, &console_.open);
            ImGui::MenuItem("Monsters", nullptr, &monsters_.open);
            ImGui::MenuItem("Instances", nullptr, &instances_.open);
            ImGui::MenuItem("Custom Gameplay", nullptr, &customGameplay_.open);
            ImGui::MenuItem("ImGui Demo Window", nullptr, &showDemo_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Play")) {
                context_.play();
                focusGameView_ = true;
            }
            if (ImGui::MenuItem("Pause"))
                context_.pause();
            if (ImGui::MenuItem("Step One Frame"))
                context_.step();
            if (ImGui::MenuItem("Stop and Reset")) {
                context_.stop();
                host_.requestGameRestart(true);
            }
            if (ImGui::MenuItem("Restart")) {
                context_.play();
                focusGameView_ = true;
                host_.requestGameRestart(false);
            }
            ImGui::Separator();
            ImGui::MenuItem("Capture Game Input", nullptr, &context_.settings.captureGameInput);
            ImGui::MenuItem("Show Player Origin", nullptr, &context_.settings.showPlayerOrigin);
            ImGui::MenuItem("Show Player Collider", nullptr,
                            &context_.settings.showPlayerCollider);
            ImGui::MenuItem("Show World Bounds", nullptr, &context_.settings.showWorldBounds);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}
void Editor::toolbar() {
    const auto mode = context_.simulationMode();
    const char *label = mode == SimulationMode::Stopped   ? "Stopped"
                        : mode == SimulationMode::Playing ? "Playing"
                        : mode == SimulationMode::Paused  ? "Paused"
                                                          : "Step";
    ImGui::Text("Simulation: %s", label);
    ImGui::SameLine();
    if (ImGui::Button("Play")) {
        context_.play();
        focusGameView_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause"))
        context_.pause();
    ImGui::SameLine();
    if (ImGui::Button("Step"))
        context_.step();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        context_.stop();
        host_.requestGameRestart(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart")) {
        context_.play();
        focusGameView_ = true;
        host_.requestGameRestart(false);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Fit", &context_.settings.fitGameView);
}
void Editor::defaultLayout() {
    ImGuiID dock = ImGui::GetID("TeyaDockspace");
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
    ImGuiID left, right, center = dock;
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .2f, &left, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .25f, &right, &center);
    ImGuiID rightBottom, centerBottom;
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, .35f, &rightBottom, &right);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .25f, &centerBottom, &center);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Assets", left);
    ImGui::DockBuilderDockWindow("Monsters", left);
    ImGui::DockBuilderDockWindow("Instances", left);
    ImGui::DockBuilderDockWindow("Custom Gameplay", left);
    ImGui::DockBuilderDockWindow("Animation Editor", center);
    ImGui::DockBuilderDockWindow("Game View", center);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Performance", rightBottom);
    ImGui::DockBuilderDockWindow("Console", centerBottom);
    ImGui::DockBuilderDockWindow("Animation Timeline", centerBottom);
    ImGui::DockBuilderFinish(dock);
    firstLayout_ = false;
}
void Editor::draw() {
    TEYA_PROFILE_ZONE_NAMED("GameEditor::draw");
    rlImGuiBegin();
    auto *v = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(v->WorkPos);
    ImGui::SetNextWindowSize(v->WorkSize);
    ImGui::SetNextWindowViewport(v->ID);
    ImGuiWindowFlags f = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("Teya Editor", nullptr, f);
    ImGui::PopStyleVar(2);
    menu();
    toolbar();
    ImGuiID dock = ImGui::GetID("TeyaDockspace");
    ImGui::DockSpace(dock);
    if (firstLayout_)
        defaultLayout();
    ImGui::End();
    hierarchy_.draw(host_, context_);
    gameView_.draw(host_, context_);
    performance_.draw(host_, context_);
    animationEditor_.draw(host_, context_);
    if (animationEditor_.active())
        animationEditor_.drawInspectorPanel(host_, inspector_.open);
    else
        inspector_.draw(host_, context_);
    assets_.draw(host_, context_);
    if (auto asset = assets_.consumeOpenRequest())
        animationEditor_.openAsset(host_, *asset);
    console_.draw(host_, context_);
    monsters_.draw(host_, context_);
    instances_.draw(host_, context_);
    customGameplay_.draw(host_, context_);
    if (focusGameView_) {
        ImGui::SetWindowFocus("Game View");
        focusGameView_ = false;
    }
    if (showDemo_)
        ImGui::ShowDemoWindow(&showDemo_);
    rlImGuiEnd();
}
bool Editor::shouldUpdateGame() { return context_.consumeSimulationStep(); }
bool Editor::gameInputEnabled() const {
    auto &io = ImGui::GetIO();
    return EditorContext::allowsGameInput({gameView_.focused(), context_.settings.captureGameInput,
                                           io.WantCaptureKeyboard, io.WantCaptureMouse,
                                           ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId),
                                           true, false, context_.simulationMode()});
}
std::optional<Vector2> Editor::gamePointerCanvasPosition() const {
    const auto &mouse = ImGui::GetIO().MousePos;
    auto point = GameViewport::screenToCanvas({mouse.x, mouse.y}, gameView_.imageRect(),
                                              host_.gameCanvasWidth(), host_.gameCanvasHeight());
    if (!point)
        return std::nullopt;
    return Vector2{point->x, point->y};
}
EditorContext &Editor::context() { return context_; }
} // namespace teya::editor
