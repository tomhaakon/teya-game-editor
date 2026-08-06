#include "teya/editor/panels/AnimationAssetBrowserPanel.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <imgui.h>

namespace teya::editor {
namespace {
bool containsInsensitive(const std::string &text, const char *query) {
    std::string a = text, b = query;
    std::transform(a.begin(), a.end(), a.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(b.begin(), b.end(), b.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return a.find(b) != std::string::npos;
}
} // namespace
void AnimationAssetBrowserPanel::refresh(EditorHost &host) {
    assets_ = host.editableAnimationAssets();
    std::sort(assets_.begin(), assets_.end(),
              [](const auto &a, const auto &b) { return a.assetPath < b.assetPath; });
    if (selected_ && std::none_of(assets_.begin(), assets_.end(),
                                  [&](const auto &a) { return a.id == *selected_; }))
        selected_.reset();
    refreshRequested_ = false;
}
std::optional<std::uint64_t> AnimationAssetBrowserPanel::consumeOpenRequest() {
    auto result = openRequest_;
    openRequest_.reset();
    return result;
}
void AnimationAssetBrowserPanel::draw(EditorHost &host, EditorContext &) {
    if (!open)
        return;
    if (!ImGui::Begin("Assets", &open)) {
        ImGui::End();
        return;
    }
    if (refreshRequested_)
        refresh(host);
    if (ImGui::Button("Refresh"))
        refresh(host);
    ImGui::SameLine();
    if (ImGui::Button("+ Animation")) {
        name_ = {};
        std::strncpy(name_.data(), "new_animation", name_.size() - 1);
        modal_ = Modal::Create;
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##asset-search", "Search animations...", search_.data(),
                             search_.size());
    ImGui::SeparatorText("Animations");
    for (const auto &asset : assets_) {
        if (!containsInsensitive(asset.displayName, search_.data()) &&
            !containsInsensitive(asset.assetPath, search_.data()))
            continue;
        ImGui::PushID(static_cast<int>(asset.id));
        const ImVec4 color = asset.valid ? ImVec4(.45f, 1, .55f, 1) : ImVec4(1, .4f, .35f, 1);
        ImGui::TextColored(color, asset.valid ? "OK" : "ERR");
        ImGui::SameLine();
        const bool selected = selected_ && *selected_ == asset.id;
        if (ImGui::Selectable(asset.displayName.c_str(), selected)) {
            selected_ = asset.id;
            openRequest_ = asset.id;
        }
        if (ImGui::BeginPopupContextItem("asset-actions")) {
            if (ImGui::MenuItem("Open"))
                openRequest_ = asset.id;
            if (ImGui::MenuItem("Duplicate")) {
                selected_ = asset.id;
                name_ = {};
                std::strncpy(name_.data(), (asset.displayName + "_copy").c_str(), name_.size() - 1);
                modal_ = Modal::Duplicate;
            }
            if (ImGui::MenuItem("Rename", nullptr, false, !asset.runtimeAsset)) {
                selected_ = asset.id;
                name_ = {};
                std::strncpy(name_.data(), asset.displayName.c_str(), name_.size() - 1);
                modal_ = Modal::Rename;
            }
            if (ImGui::MenuItem("Delete", nullptr, false, !asset.runtimeAsset)) {
                selected_ = asset.id;
                modal_ = Modal::Delete;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    const char *modalTitle = nullptr;
    switch (modal_) {
    case Modal::Create:
        modalTitle = "Create Animation Asset";
        break;
    case Modal::Duplicate:
        modalTitle = "Duplicate Animation Asset";
        break;
    case Modal::Rename:
        modalTitle = "Rename Animation Asset";
        break;
    case Modal::Delete:
        modalTitle = "Delete Animation Asset";
        break;
    case Modal::None:
        break;
    }
    if (modalTitle != nullptr && !ImGui::IsPopupOpen(modalTitle))
        ImGui::OpenPopup(modalTitle);

    auto operationModal = [&](const char *title, auto operation) {
        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Name", name_.data(), name_.size());
            if (ImGui::Button("Confirm")) {
                auto result = operation();
                message_ = result ? "Asset operation completed" : result.error;
                if (result) {
                    selected_ = result.assetId;
                    refresh(host);
                }
                modal_ = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                modal_ = Modal::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    };
    operationModal("Create Animation Asset",
                   [&] { return host.createAnimationAsset(name_.data()); });
    operationModal("Duplicate Animation Asset", [&] {
        return selected_ ? host.duplicateAnimationAsset(*selected_, name_.data())
                         : AnimationAssetOperationResult{};
    });
    operationModal("Rename Animation Asset", [&] {
        return selected_ ? host.renameAnimationAsset(*selected_, name_.data())
                         : AnimationAssetOperationResult{};
    });
    if (ImGui::BeginPopupModal("Delete Animation Asset", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Delete this animation asset from disk?");
        if (ImGui::Button("Delete")) {
            auto result =
                selected_ ? host.deleteAnimationAsset(*selected_) : AnimationAssetOperationResult{};
            message_ = result ? "Animation deleted" : result.error;
            if (result)
                refresh(host);
            modal_ = Modal::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            modal_ = Modal::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!message_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", message_.c_str());
    }
    ImGui::End();
}
} // namespace teya::editor
