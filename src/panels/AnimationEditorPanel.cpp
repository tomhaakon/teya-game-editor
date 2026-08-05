#include "teya/editor/panels/AnimationEditorPanel.h"
#include "teya/editor/EditorContext.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <rlImGui.h>
#include <teya/animation/AnimationTransforms.h>
#include <teya/core/Profile.h>
namespace teya::editor {
namespace {
template <size_t N> std::array<char, N> buffer(const std::string &s) {
    std::array<char, N> b{};
    std::strncpy(b.data(), s.c_str(), N - 1);
    return b;
}
const char *renderName(teya::animation::AnimationRenderMode m) {
    return m == teya::animation::AnimationRenderMode::PixelArt ? "Pixel Art" : "Smooth";
}
const char *filterName(teya::animation::AnimationTextureFilter f) {
    return f == teya::animation::AnimationTextureFilter::Nearest ? "Nearest" : "Linear";
}
const char *directionName(teya::animation::AnimationDirection direction) {
    switch (direction) {
    case teya::animation::AnimationDirection::Any:
        return "Any";
    case teya::animation::AnimationDirection::Down:
        return "Down";
    case teya::animation::AnimationDirection::Up:
        return "Up";
    case teya::animation::AnimationDirection::Right:
        return "Right";
    case teya::animation::AnimationDirection::Left:
        return "Left";
    }
    return "Unknown";
}
float clipDuration(const teya::animation::AnimationClip &c) {
    float t = 0;
    for (auto &f : c.frames)
        t += f.durationSeconds;
    return t;
}
} // namespace
void AnimationEditorPanel::draw(EditorHost &host, EditorContext &) {
    TEYA_PROFILE_ZONE_NAMED("AnimationEditorPanel::draw");
    active_ = false;
    if (!open)
        return;
    const bool mainVisible = ImGui::Begin("Animation Editor", &open);
    active_ = mainVisible;
    if (mainVisible) {
        assets_ = host.editableAnimationAssets();
        if (!loaded_ && !assets_.empty())
            load(host, assets_[0].id);
        assetBar(host);
        if (loaded_) {
            syncPreview();
            shortcuts(host);
            if (!timelineOpen_ && ImGui::Button("Show Animation Timeline"))
                timelineOpen_ = true;
            if (ImGui::BeginTable("animation-layout", 2,
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Clips", ImGuiTableColumnFlags_WidthFixed, 200);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                clips();
                // Structural clip edits happen immediately. Refresh the immutable
                // preview snapshot before it can write its old frame index back.
                syncPreview();
                ImGui::TableNextColumn();
                preview(host);
                ImGui::EndTable();
            }
        }
    }
    ImGui::End();

    if (mainVisible && timelineOpen_) {
        if (ImGui::Begin("Animation Timeline", &timelineOpen_) && loaded_)
            timeline();
        ImGui::End();
    }
}
void AnimationEditorPanel::drawInspectorPanel(EditorHost &host, bool &panelOpen) {
    if (!active_ || !panelOpen)
        return;
    if (ImGui::Begin("Inspector", &panelOpen) && loaded_)
        inspector(host);
    ImGui::End();
}
void AnimationEditorPanel::load(EditorHost &h, std::uint64_t id) {
    auto result = h.loadAnimationWorkingCopy(id);
    if (!result) {
        message_ = result.error;
        return;
    }
    document_.load(*result.asset, id);
    texture_ = result.texture;
    textureWidth_ = result.textureWidth;
    textureHeight_ = result.textureHeight;
    loaded_ = true;
    showGrid_ = document_.asset().authoring.showPixelGridByDefault;
    fit_ = false;
    zoom_ = 4.0f;
    sheetSelection_.clear();
    sheetSelectionAnchor_ = -1;
    attachmentObjects_ = h.attachmentPreviews(id);
    attachmentAssetId_ = id;
    selectedAttachment_ = std::min(selectedAttachment_,
                                   attachmentObjects_.empty() ? std::size_t{0}
                                                              : attachmentObjects_.size() - 1);
    validate(h);
    previewRevision_ = ~std::uint64_t{0};
    message_ = "Loaded working copy";
}

void AnimationEditorPanel::frameSourcePicker() {
    auto &asset = document_.asset();
    const bool grid =
        asset.sourceMode == teya::animation::AnimationFrameSourceMode::SpriteSheetGrid;
    if (ImGui::Button(grid ? "Open Sprite Sheet Picker..." : "Open Atlas Picker...")) {
        if (grid && !asset.clips.empty()) {
            sheetSelection_.clear();
            const auto &clip = asset.clips[document_.selection().clip];
            sheetSelection_.reserve(clip.frames.size());
            for (const auto &frame : clip.frames)
                sheetSelection_.push_back(frame.source.spriteIndex);
            sheetSelectionAnchor_ = sheetSelection_.empty() ? -1 : sheetSelection_.back();
        }
        ImGui::OpenPopup("Frame Source Picker");
    }

    const auto &io = ImGui::GetIO();
    ImGui::SetNextWindowSize(
        {std::max(720.0f, io.DisplaySize.x * .82f), std::max(560.0f, io.DisplaySize.y * .82f)},
        ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({io.DisplaySize.x * .5f, io.DisplaySize.y * .5f}, ImGuiCond_Appearing,
                            {.5f, .5f});
    if (!ImGui::BeginPopupModal("Frame Source Picker", nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    ImGui::Text("%s", grid ? "Sprite Sheet Grid" : "Texture Atlas");
    ImGui::SameLine();
    ImGui::TextDisabled("| Texture: %s | %d x %d", asset.texturePath.c_str(), textureWidth_,
                        textureHeight_);
    if (!asset.clips.empty()) {
        const auto &selection = document_.selection();
        const auto &clip = asset.clips[selection.clip];
        ImGui::Text("Clip: %s | Current frame: %zu of %zu", clip.name.c_str(),
                    clip.frames.empty() ? 0 : selection.frame + 1, clip.frames.size());
    }
    if (grid)
        ImGui::TextDisabled("Cell: %d x %d | Saved columns: %d", asset.frameWidth,
                            asset.frameHeight, asset.sheetColumns);
    else
        ImGui::TextDisabled("Authored regions: %zu", asset.atlasRegions.size());
    ImGui::Separator();

    if (!IsTextureValid(texture_))
        ImGui::TextColored({1, .45f, .3f, 1},
                           "The texture is unavailable. Asset metadata is still editable.");
    else if (grid)
        spriteSheetPicker();
    else
        atlasPicker();

    ImGui::Separator();
    if (ImGui::Button("Close"))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void AnimationEditorPanel::spriteSheetPicker() {
    auto &asset = document_.asset();
    auto &selection = document_.selection();
    if (asset.frameWidth <= 0 || asset.frameHeight <= 0 || textureWidth_ <= 0 ||
        textureHeight_ <= 0) {
        ImGui::TextUnformatted("Enter positive frame dimensions to divide the sheet.");
        return;
    }

    const int actualColumns = textureWidth_ / asset.frameWidth;
    const int rows = textureHeight_ / asset.frameHeight;
    const int frameCount = actualColumns * rows;
    if (actualColumns <= 0 || rows <= 0) {
        ImGui::TextUnformatted("The selected cell size is larger than the texture.");
        return;
    }
    if (asset.sheetColumns != actualColumns) {
        ImGui::TextColored({1, .7f, .2f, 1}, "Saved columns: %d, texture-derived columns: %d",
                           asset.sheetColumns, actualColumns);
        ImGui::SameLine();
        if (ImGui::SmallButton("Use derived columns")) {
            auto before = asset;
            asset.sheetColumns = actualColumns;
            document_.mutate(before);
        }
    }

    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Sheet zoom", &sheetPickerZoom_, .25f, 8.0f, "%.2fx",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat("New frame duration", &importedFrameDuration_, 0, 0, "%.3f");
    ImGui::TextDisabled("Ctrl: toggle  Shift: range  Drag: paint order");

    if (ImGui::Button("Clear selection")) {
        sheetSelection_.clear();
        sheetSelectionAnchor_ = -1;
    }
    const bool canImport = !sheetSelection_.empty() && !asset.clips.empty() &&
                           asset.sheetColumns == actualColumns &&
                           std::isfinite(importedFrameDuration_) && importedFrameDuration_ > 0;
    ImGui::BeginDisabled(!canImport);
    if (ImGui::Button("Set clip frames from selection")) {
        auto before = asset;
        auto &frames = asset.clips[selection.clip].frames;
        auto oldFrames = frames;
        std::vector<bool> reused(oldFrames.size(), false);
        std::vector<teya::animation::AnimationFrame> replacement;
        replacement.reserve(sheetSelection_.size());
        for (int index : sheetSelection_) {
            auto found = oldFrames.end();
            for (std::size_t i = 0; i < oldFrames.size(); ++i) {
                if (!reused[i] && oldFrames[i].source.spriteIndex == index) {
                    found = oldFrames.begin() + static_cast<std::ptrdiff_t>(i);
                    reused[i] = true;
                    break;
                }
            }
            if (found != oldFrames.end()) {
                replacement.push_back(*found);
            } else {
                teya::animation::AnimationFrame frame;
                frame.source.spriteIndex = index;
                frame.durationSeconds = importedFrameDuration_;
                replacement.push_back(std::move(frame));
            }
        }
        frames = std::move(replacement);
        selection.frame = std::min(selection.frame, frames.size() - 1);
        selection.kind = AnimationSelection::Kind::Frame;
        document_.mutate(before);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Makes this ordered selection the clip's complete frame list. Existing "
                          "matching frames keep their timing, sockets, events, hitboxes, and "
                          "markers.");
    if (ImGui::Button("Append selected to clip")) {
        auto before = asset;
        auto &frames = asset.clips[selection.clip].frames;
        for (int index : sheetSelection_) {
            teya::animation::AnimationFrame frame;
            frame.source.spriteIndex = index;
            frame.durationSeconds = importedFrameDuration_;
            frames.push_back(std::move(frame));
        }
        selection.frame = frames.size() - 1;
        selection.kind = AnimationSelection::Kind::Frame;
        document_.mutate(before);
    }
    const bool hasCurrent = !asset.clips.empty() && !asset.clips[selection.clip].frames.empty();
    ImGui::BeginDisabled(!hasCurrent);
    if (ImGui::Button("Replace current frame")) {
        auto before = asset;
        asset.clips[selection.clip].frames[selection.frame].source.spriteIndex =
            sheetSelection_.front();
        document_.mutate(before);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::Text("%zu selected", sheetSelection_.size());

    ImGui::BeginChild("sprite-sheet-scroll", {0, 340}, true, ImGuiWindowFlags_HorizontalScrollbar);
    const float scale = std::max(.05f, sheetPickerZoom_);
    const ImVec2 imageSize{textureWidth_ * scale, textureHeight_ * scale};
    SetTextureFilter(texture_,
                     asset.render.textureFilter == teya::animation::AnimationTextureFilter::Nearest
                         ? TEXTURE_FILTER_POINT
                         : TEXTURE_FILTER_BILINEAR);
    rlImGuiImageRect(
        &texture_, static_cast<int>(imageSize.x), static_cast<int>(imageSize.y),
        Rectangle{0, 0, static_cast<float>(textureWidth_), static_cast<float>(textureHeight_)});
    const ImVec2 imageMin = ImGui::GetItemRectMin();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool overImage = ImGui::IsItemHovered();
    int hoveredIndex = -1;
    if (overImage) {
        const int column = static_cast<int>((mouse.x - imageMin.x) / (asset.frameWidth * scale));
        const int row = static_cast<int>((mouse.y - imageMin.y) / (asset.frameHeight * scale));
        if (column >= 0 && column < actualColumns && row >= 0 && row < rows)
            hoveredIndex = row * actualColumns + column;
    }
    auto addInOrder = [&](int index) {
        if (index < 0 || index >= frameCount)
            return;
        if (std::find(sheetSelection_.begin(), sheetSelection_.end(), index) ==
            sheetSelection_.end())
            sheetSelection_.push_back(index);
    };
    if (hoveredIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (ImGui::GetIO().KeyShift && sheetSelectionAnchor_ >= 0) {
            sheetSelection_.clear();
            const int direction = hoveredIndex >= sheetSelectionAnchor_ ? 1 : -1;
            for (int i = sheetSelectionAnchor_;; i += direction) {
                addInOrder(i);
                if (i == hoveredIndex)
                    break;
            }
        } else if (ImGui::GetIO().KeyCtrl) {
            auto found = std::find(sheetSelection_.begin(), sheetSelection_.end(), hoveredIndex);
            if (found == sheetSelection_.end())
                sheetSelection_.push_back(hoveredIndex);
            else
                sheetSelection_.erase(found);
            sheetSelectionAnchor_ = hoveredIndex;
        } else {
            sheetSelection_.assign(1, hoveredIndex);
            sheetSelectionAnchor_ = hoveredIndex;
        }
    } else if (hoveredIndex >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
               ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        addInOrder(hoveredIndex);
    }
    auto *draw = ImGui::GetWindowDrawList();
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < actualColumns; ++column) {
            const int index = row * actualColumns + column;
            const ImVec2 min{imageMin.x + column * asset.frameWidth * scale,
                             imageMin.y + row * asset.frameHeight * scale};
            const ImVec2 max{min.x + asset.frameWidth * scale, min.y + asset.frameHeight * scale};
            const auto selected = std::find(sheetSelection_.begin(), sheetSelection_.end(), index);
            const bool isSelected = selected != sheetSelection_.end();
            draw->AddRect(min, max,
                          isSelected ? IM_COL32(255, 205, 40, 255) : IM_COL32(255, 255, 255, 75), 0,
                          0, isSelected ? 3.0f : 1.0f);
            if (isSelected) {
                const auto order = static_cast<int>(selected - sheetSelection_.begin()) + 1;
                char label[24];
                std::snprintf(label, sizeof(label), "%d:%d", order, index);
                draw->AddRectFilled(min,
                                    {min.x + ImGui::CalcTextSize(label).x + 6,
                                     min.y + ImGui::GetTextLineHeight() + 2},
                                    IM_COL32(20, 20, 20, 210));
                draw->AddText({min.x + 3, min.y + 1}, IM_COL32(255, 225, 60, 255), label);
            }
        }
    }
    ImGui::EndChild();

    if (ImGui::TreeNode("Automatic cell-size suggestions")) {
        ImGui::TextDisabled("Suggestions that divide the texture exactly:");
        const int common[] = {8, 16, 24, 32, 48, 64, 96, 128, 256, 512};
        for (int size : common) {
            if (size > textureWidth_ || size > textureHeight_ || textureWidth_ % size != 0 ||
                textureHeight_ % size != 0)
                continue;
            ImGui::PushID(size);
            if (ImGui::SmallButton((std::to_string(size) + " x " + std::to_string(size)).c_str())) {
                auto before = asset;
                asset.frameWidth = asset.frameHeight = size;
                asset.sheetColumns = textureWidth_ / size;
                sheetSelection_.clear();
                document_.mutate(before);
            }
            ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::NewLine();
        ImGui::TreePop();
    }
}

void AnimationEditorPanel::atlasPicker() {
    auto &asset = document_.asset();
    auto &selection = document_.selection();
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##atlas-search", "Search atlas regions...", atlasSearch_.data(),
                             atlasSearch_.size());
    ImGui::SameLine();
    if (ImGui::Button("+ Region")) {
        auto before = asset;
        std::string id = "region";
        for (int suffix = 1; asset.findAtlasRegion(id); ++suffix)
            id = "region_" + std::to_string(suffix);
        const float width = std::min(64, textureWidth_);
        const float height = std::min(64, textureHeight_);
        asset.atlasRegions.push_back({id, id, {0, 0, width, height}});
        document_.mutate(before);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Atlas zoom", &sheetPickerZoom_, .25f, 8.0f, "%.2fx",
                       ImGuiSliderFlags_Logarithmic);

    ImGui::BeginChild("atlas-sheet-scroll", {0, 340}, true, ImGuiWindowFlags_HorizontalScrollbar);
    const float scale = std::max(.05f, sheetPickerZoom_);
    const ImVec2 imageSize{textureWidth_ * scale, textureHeight_ * scale};
    SetTextureFilter(texture_,
                     asset.render.textureFilter == teya::animation::AnimationTextureFilter::Nearest
                         ? TEXTURE_FILTER_POINT
                         : TEXTURE_FILTER_BILINEAR);
    rlImGuiImageRect(
        &texture_, static_cast<int>(imageSize.x), static_cast<int>(imageSize.y),
        Rectangle{0, 0, static_cast<float>(textureWidth_), static_cast<float>(textureHeight_)});
    const ImVec2 origin = ImGui::GetItemRectMin();
    auto *draw = ImGui::GetWindowDrawList();
    for (std::size_t i = 0; i < asset.atlasRegions.size(); ++i) {
        const auto &region = asset.atlasRegions[i];
        const ImVec2 min{origin.x + region.bounds.x * scale, origin.y + region.bounds.y * scale};
        const ImVec2 max{min.x + region.bounds.width * scale, min.y + region.bounds.height * scale};
        const bool active =
            !asset.clips.empty() && !asset.clips[selection.clip].frames.empty() &&
            asset.clips[selection.clip].frames[selection.frame].source.atlasRegion == region.id;
        draw->AddRect(min, max, active ? IM_COL32(255, 205, 40, 255) : IM_COL32(60, 210, 255, 210),
                      0, 0, active ? 3 : 1.5f);
        draw->AddText({min.x + 2, min.y + 2}, IM_COL32(255, 255, 255, 255), region.id.c_str());
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::IsMouseHoveringRect(min, max) && !asset.clips.empty() &&
            !asset.clips[selection.clip].frames.empty()) {
            auto before = asset;
            asset.clips[selection.clip].frames[selection.frame].source.atlasRegion = region.id;
            document_.mutate(before);
        }
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Atlas regions");
    for (std::size_t i = 0; i < asset.atlasRegions.size(); ++i) {
        auto &region = asset.atlasRegions[i];
        std::string searchable = region.id + " " + region.name;
        std::string query = atlasSearch_.data();
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        std::transform(searchable.begin(), searchable.end(), searchable.begin(), lower);
        std::transform(query.begin(), query.end(), query.begin(), lower);
        if (!query.empty() && searchable.find(query) == std::string::npos)
            continue;
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::TreeNode(region.name.empty() ? region.id.c_str() : region.name.c_str())) {
            auto before = asset;
            auto id = buffer<96>(region.id);
            auto name = buffer<96>(region.name);
            bool changed = ImGui::InputText("Stable ID", id.data(), id.size());
            changed |= ImGui::InputText("Display name", name.data(), name.size());
            changed |= ImGui::InputFloat4("Rectangle", &region.bounds.x, "%.2f");
            if (changed) {
                region.id = id.data();
                region.name = name.data();
                document_.mutate(before);
            }
            if (ImGui::Button("Assign to current frame") && !asset.clips.empty() &&
                !asset.clips[selection.clip].frames.empty()) {
                before = asset;
                asset.clips[selection.clip].frames[selection.frame].source.atlasRegion = region.id;
                document_.mutate(before);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete region")) {
                before = asset;
                asset.atlasRegions.erase(asset.atlasRegions.begin() + i);
                document_.mutate(before);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::TextDisabled(
        "Regions use exact runtime atlas rectangles and serialize in the animation asset.");
}
bool AnimationEditorPanel::openAsset(EditorHost &host, std::uint64_t id) {
    if (document_.dirty()) {
        message_ = "Save or Reload the dirty animation before opening another asset";
        open = true;
        return false;
    }
    load(host, id);
    open = true;
    return loaded_ && document_.assetId() == id;
}
void AnimationEditorPanel::validate(EditorHost &h) {
    TEYA_PROFILE_ZONE_NAMED("AnimationEditor::validate");
    validation_ = h.validateEditableAnimation(document_.asset(), textureWidth_, textureHeight_);
}
void AnimationEditorPanel::save(EditorHost &h) {
    TEYA_PROFILE_ZONE_NAMED("AnimationEditor::save");
    validate(h);
    if (!validation_.valid()) {
        message_ = "Save blocked by validation errors";
        return;
    }
    auto r = h.saveAndApplyAnimationAsset(document_.assetId(), document_.asset());
    if (r) {
        const auto assetId = document_.assetId();
        const auto oldSelection = document_.selection();
        const auto clipName = document_.asset().clips.empty()
                                  ? std::string{}
                                  : document_.asset().clips[oldSelection.clip].name;
        load(h, assetId);
        auto found = std::find_if(document_.asset().clips.begin(), document_.asset().clips.end(),
                                  [&](const auto &clip) { return clip.name == clipName; });
        if (found != document_.asset().clips.end()) {
            document_.selection().clip =
                static_cast<std::size_t>(found - document_.asset().clips.begin());
            document_.selection().frame = oldSelection.frame;
            document_.repairSelection();
        }
        message_ = "Saved and applied";
    } else
        message_ = r.error;
}
void AnimationEditorPanel::reload(EditorHost &h) {
    if (document_.dirty()) {
        ImGui::OpenPopup("Discard animation changes?");
        return;
    }
    load(h, document_.assetId());
}
void AnimationEditorPanel::applyTemporary(EditorHost &h) {
    validate(h);
    if (!validation_.valid()) {
        message_ = "Temporary apply blocked by validation errors";
        return;
    }
    auto r = h.applyAnimationAssetWithoutSaving(document_.assetId(), document_.asset());
    if (r.applied) {
        document_.markTemporaryApplied();
        message_ = "Applied temporarily (not saved)";
    } else
        message_ = r.error;
}
void AnimationEditorPanel::syncPreview() {
    if (previewRevision_ == document_.revision())
        return;
    auto copy = std::make_shared<const teya::animation::AnimationAsset>(document_.asset());
    std::string clip = document_.asset().clips.empty()
                           ? std::string{}
                           : document_.asset().clips[document_.selection().clip].name;
    previewPlayer_.replaceAsset(copy);
    if (!clip.empty()) {
        previewPlayer_.play(clip, true);
        previewPlayer_.consumeEvents();
        previewPlayer_.setFrameForPreview(document_.selection().frame);
    }
    previewRevision_ = document_.revision();
}
void AnimationEditorPanel::assetBar(EditorHost &h) {
    if (assets_.empty()) {
        ImGui::TextDisabled("The host exposes no editable animation assets");
        return;
    }
    const auto it = std::find_if(assets_.begin(), assets_.end(),
                                 [&](auto &a) { return a.id == document_.assetId(); });
    const char *current = it == assets_.end() ? "Select asset" : it->displayName.c_str();
    ImGui::TextDisabled("Animation asset");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##animation-asset", current)) {
        for (auto &a : assets_) {
            bool selected = a.id == document_.assetId();
            if (ImGui::Selectable(a.displayName.c_str(), selected)) {
                // Selecting the current item only closes the combo. Reloading
                // here replaces the document while this popup is still using
                // its state and is both unnecessary and unsafe.
                if (selected) {
                    ImGui::CloseCurrentPopup();
                } else if (document_.dirty())
                    message_ = "Save or Reload before switching dirty assets";
                else
                    load(h, a.id);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (loaded_)
        ImGui::TextColored(document_.dirty() ? ImVec4(1, .7f, .2f, 1) : ImVec4(.4f, 1, .4f, 1),
                           document_.dirty() ? "Dirty *" : "Saved");
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        save(h);
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
        reload(h);
    ImGui::SameLine();
    if (ImGui::Button("Validate"))
        validate(h);
    if (ImGui::Button("Apply Without Saving"))
        applyTemporary(h);
    ImGui::SameLine();
    frameSourcePicker();
    ImGui::SameLine();
    if (ImGui::Button("Attachment Objects..."))
        ImGui::OpenPopup("Attachment Objects");
    attachmentObjects(h);
    if (document_.temporaryApplied()) {
        ImGui::TextColored({1, .6f, .1f, 1}, "Temporary runtime asset");
    }
    if (ImGui::BeginPopupModal("Discard animation changes?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Discard the dirty working copy and reload from disk?");
        if (ImGui::Button("Discard and Reload")) {
            document_.markSaved();
            load(h, document_.assetId());
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (!message_.empty())
        ImGui::TextWrapped("%s", message_.c_str());
}

void AnimationEditorPanel::attachmentObjects(EditorHost &host) {
    ImGui::SetNextWindowSize({760, 560}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Attachment Objects", nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    ImGui::TextDisabled("Reusable objects follow an animation socket. Changes apply when saved.");
    if (ImGui::Button("+ Object")) {
        AttachmentPreviewInfo object;
        object.id = attachmentObjects_.empty() ? 1 : attachmentObjects_.back().id + 1;
        object.name = "New Object";
        object.socketName = "weapon_hand";
        attachmentObjects_.push_back(std::move(object));
        selectedAttachment_ = attachmentObjects_.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && selectedAttachment_ < attachmentObjects_.size()) {
        auto copy = attachmentObjects_[selectedAttachment_];
        copy.id = attachmentObjects_.empty() ? 1 : attachmentObjects_.back().id + 1;
        copy.name += " Copy";
        copy.ownsTexture = false;
        attachmentObjects_.push_back(std::move(copy));
        selectedAttachment_ = attachmentObjects_.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selectedAttachment_ < attachmentObjects_.size()) {
        attachmentObjects_.erase(attachmentObjects_.begin() + selectedAttachment_);
        selectedAttachment_ = std::min(selectedAttachment_,
                                       attachmentObjects_.empty() ? std::size_t{0}
                                                                  : attachmentObjects_.size() - 1);
    }
    ImGui::Separator();
    if (ImGui::BeginTable("attachment-object-layout", 2, ImGuiTableFlags_Resizable |
                                                           ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthFixed, 190);
        ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        for (std::size_t i = 0; i < attachmentObjects_.size(); ++i)
            if (ImGui::Selectable(attachmentObjects_[i].name.c_str(), i == selectedAttachment_))
                selectedAttachment_ = i;
        ImGui::TableNextColumn();
        if (selectedAttachment_ < attachmentObjects_.size()) {
            auto &object = attachmentObjects_[selectedAttachment_];
            auto name = buffer<96>(object.name);
            if (ImGui::InputText("Name", name.data(), name.size()))
                object.name = name.data();
            auto texturePath = buffer<256>(object.texturePath);
            if (ImGui::InputText("Texture", texturePath.data(), texturePath.size()))
                object.texturePath = texturePath.data();
            auto socketName = buffer<96>(object.socketName);
            if (ImGui::BeginCombo("Socket", object.socketName.c_str())) {
                const auto &asset = document_.asset();
                if (!asset.clips.empty() && !asset.clips[document_.selection().clip].frames.empty()) {
                    const auto &sockets = asset.clips[document_.selection().clip]
                                              .frames[document_.selection().frame]
                                              .sockets;
                    for (const auto &socket : sockets)
                        if (ImGui::Selectable(socket.name.c_str(), socket.name == object.socketName))
                            object.socketName = socket.name;
                }
                ImGui::EndCombo();
            }
            if (ImGui::InputText("Socket name", socketName.data(), socketName.size()))
                object.socketName = socketName.data();
            ImGui::DragFloat2("Pivot", &object.pivot.x, .25f, 0, 0, "%.2f");
            ImGui::DragFloat2("Position offset", &object.positionOffset.x, .25f, -1000, 1000,
                              "%.2f");
            ImGui::DragFloat("Rotation offset", &object.rotationOffsetDegrees, .25f, -360, 360,
                             "%.2f deg");
            ImGui::DragFloat2("Scale", &object.scale.x, .01f, .01f, 100, "%.3f");
            ImGui::Checkbox("Visible", &object.visible);
            ImGui::SeparatorText("Pivot preview");
            if (IsTextureValid(object.texture)) {
                const float maxSide = 220.0f;
                const float scale = std::min(1.0f, maxSide / std::max(object.texture.width,
                                                                      object.texture.height));
                const ImVec2 size{object.texture.width * scale, object.texture.height * scale};
                ImGui::Image((ImTextureID)object.texture.id, size);
                const auto imageMin = ImGui::GetItemRectMin();
                const ImVec2 pivot{imageMin.x + object.pivot.x * scale,
                                   imageMin.y + object.pivot.y * scale};
                auto *draw = ImGui::GetWindowDrawList();
                draw->AddLine({pivot.x - 7, pivot.y}, {pivot.x + 7, pivot.y},
                              IM_COL32(255, 220, 0, 255), 2);
                draw->AddLine({pivot.x, pivot.y - 7}, {pivot.x, pivot.y + 7},
                              IM_COL32(255, 220, 0, 255), 2);
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    const auto mouse = ImGui::GetIO().MousePos;
                    object.pivot = {(mouse.x - imageMin.x) / scale,
                                    (mouse.y - imageMin.y) / scale};
                }
                ImGui::TextDisabled("Click the image at the point held by the hand.");
            } else
                ImGui::TextDisabled("Save a valid texture path to load the pivot preview.");
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    if (ImGui::Button("Save Objects")) {
        auto result = host.saveAttachmentObjects(document_.assetId(), attachmentObjects_);
        attachmentMessage_ = result ? "Attachment objects saved and applied"
                                    : result.error;
        if (result) {
            attachmentObjects_ = host.attachmentPreviews(document_.assetId());
            selectedAttachment_ = std::min(selectedAttachment_,
                                           attachmentObjects_.empty() ? std::size_t{0}
                                                                      : attachmentObjects_.size() - 1);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Objects")) {
        attachmentObjects_ = host.attachmentPreviews(document_.assetId());
        attachmentMessage_ = "Attachment objects reloaded";
    }
    ImGui::SameLine();
    if (ImGui::Button("Close"))
        ImGui::CloseCurrentPopup();
    if (!attachmentMessage_.empty())
        ImGui::TextWrapped("%s", attachmentMessage_.c_str());
    ImGui::EndPopup();
}
void AnimationEditorPanel::clips() {
    auto &a = document_.asset();
    auto &s = document_.selection();
    ImGui::Text("Clips (%d)", (int)a.clips.size());
    if (ImGui::Button("+ Clip")) {
        int n = 1;
        std::string name = "clip";
        while (a.findClip(name))
            name = "clip" + std::to_string(++n);
        document_.addClip(name);
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate"))
        document_.duplicateClip();
    if (ImGui::Button("Delete"))
        document_.deleteClip();
    ImGui::Separator();
    std::vector<std::size_t> sortedClips(a.clips.size());
    for (std::size_t i = 0; i < sortedClips.size(); ++i)
        sortedClips[i] = i;
    std::stable_sort(sortedClips.begin(), sortedClips.end(), [&](std::size_t left,
                                                                 std::size_t right) {
        const auto &aName = a.clips[left].name;
        const auto &bName = a.clips[right].name;
        return std::lexicographical_compare(
            aName.begin(), aName.end(), bName.begin(), bName.end(),
            [](unsigned char aChar, unsigned char bChar) {
                return std::tolower(aChar) < std::tolower(bChar);
            });
    });
    for (const std::size_t i : sortedClips)
        if (ImGui::Selectable(a.clips[i].name.c_str(), s.clip == i)) {
            s.clip = i;
            s.frame = 0;
            previewRevision_ = ~std::uint64_t{0};
        }
}
void AnimationEditorPanel::preview(EditorHost &host) {
    TEYA_PROFILE_ZONE_NAMED("AnimationEditor::preview");
    auto &a = document_.asset();
    auto &s = document_.selection();
    if (a.clips.empty()) {
        ImGui::TextDisabled("Add a clip and frame to preview");
        return;
    }
    s.clip = std::min(s.clip, a.clips.size() - 1);
    if (a.clips[s.clip].frames.empty()) {
        s.frame = 0;
        ImGui::TextDisabled("Add a frame to preview");
        return;
    }
    s.frame = std::min(s.frame, a.clips[s.clip].frames.size() - 1);
    auto &clip = a.clips[s.clip];
    auto &frame = clip.frames[s.frame];
    const bool mirrored = clip.mirrored;
    auto source = teya::animation::animationSourceRectangle(a, frame);
    if (!source || !IsTextureValid(texture_)) {
        ImGui::TextColored({1, .4f, .4f, 1},
                           "Preview texture/source unavailable; metadata remains editable");
        return;
    }
    if (ImGui::Button(playing_ ? "Pause" : "Play")) {
        playing_ = !playing_;
        if (playing_) {
            previewPlayer_.play(clip.name);
            previewPlayer_.resume();
        } else
            previewPlayer_.pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        playing_ = false;
        previewPlayer_.stop();
        previewPlayer_.play(clip.name, true);
        previewPlayer_.pause();
        previewPlayer_.consumeEvents();
    }
    ImGui::Checkbox("Fit", &fit_);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid_);
    ImGui::SameLine();
    ImGui::Checkbox("Show sockets", &showSockets_);
    ImGui::Checkbox("Prev onion", &onionPrevious_);
    ImGui::SameLine();
    ImGui::Checkbox("Next onion", &onionNext_);
    ImGui::SliderFloat("Onion opacity", &opacity_, 0, .8f, "%.2f");
    bool pixel = a.render.mode == teya::animation::AnimationRenderMode::PixelArt;
    if (pixel && a.authoring.preferIntegerPreviewScale) {
        const char *values[] = {"1x", "2x", "4x", "8x", "16x"};
        int opts[] = {1, 2, 4, 8, 16};
        int current = 0;
        for (int i = 0; i < 5; ++i)
            if ((int)zoom_ == opts[i])
                current = i;
        if (ImGui::Combo("Zoom", &current, values, 5)) {
            zoom_ = (float)opts[current];
            fit_ = false;
        }
    } else {
        if (ImGui::SliderFloat("Zoom", &zoom_, .05f, 16, "%.2fx"))
            fit_ = false;
    }
    auto avail = ImGui::GetContentRegionAvail();
    float fitScale =
        std::min(avail.x / source->width, std::max(1.f, avail.y - 50) / source->height);
    float z = animationPreviewZoom(pixel, a.authoring.preferIntegerPreviewScale,
                                   fit_ ? fitScale : zoom_, fitScale);
    ImVec2 size{source->width * z, source->height * z};
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 pos{cursor.x + std::max(0.f, (avail.x - size.x) * .5f) + pan_.x, cursor.y + 20 + pan_.y};
    ImGui::SetCursorScreenPos(pos);
    // rlImGuiImageRect's negative-width convention differs from raylib's
    // DrawTexturePro convention. Use explicit UVs so left-facing preview is a
    // true horizontal mirror of exactly the selected source region.
    const float uLeft = source->x / static_cast<float>(texture_.width);
    const float uRight = (source->x + source->width) / static_cast<float>(texture_.width);
    const float vTop = source->y / static_cast<float>(texture_.height);
    const float vBottom = (source->y + source->height) / static_cast<float>(texture_.height);
    const ImVec2 uv0{mirrored ? uRight : uLeft, vTop};
    const ImVec2 uv1{mirrored ? uLeft : uRight, vBottom};
    ImVec2 min = pos, max{pos.x + size.x, pos.y + size.y};
    auto *dl = ImGui::GetWindowDrawList();
    struct AttachmentHitRegion {
        std::size_t socketIndex = 0;
        std::array<ImVec2, 4> corners{};
    };
    std::vector<AttachmentHitRegion> attachmentHitRegions;
    auto drawAttachmentLayer = [&](teya::animation::AttachmentLayer layer) {
        for (const auto &object : attachmentObjects_) {
            if (!object.visible || !IsTextureValid(object.texture))
                continue;
            auto socketIt = std::find_if(frame.sockets.begin(), frame.sockets.end(),
                                         [&](const auto &socket) {
                                             return socket.name == object.socketName;
                                         });
            if (socketIt == frame.sockets.end() || !socketIt->visible)
                continue;
            if (socketIt->layer != layer)
                continue;
            auto socket = mirrored ? teya::animation::mirrorSocket(*socketIt, source->width)
                                   : *socketIt;
            const float angleDegrees = socket.rotationDegrees +
                                       (mirrored ? -object.rotationOffsetDegrees
                                                 : object.rotationOffsetDegrees);
            const float angle = angleDegrees * DEG2RAD;
            const float cosine = std::cos(angle), sine = std::sin(angle);
            const float sx = socket.scale.x * object.scale.x * z;
            const float sy = socket.scale.y * object.scale.y * z;
            const Vector2 offset{mirrored ? -object.positionOffset.x : object.positionOffset.x,
                                 object.positionOffset.y};
            const ImVec2 anchor{min.x + (socket.position.x + offset.x) * z,
                                min.y + (socket.position.y + offset.y) * z};
            const float pivotX = mirrored ? object.texture.width - object.pivot.x
                                          : object.pivot.x;
            auto corner = [&](float x, float y) {
                x = (x - pivotX) * sx;
                y = (y - object.pivot.y) * sy;
                return ImVec2{anchor.x + x * cosine - y * sine,
                              anchor.y + x * sine + y * cosine};
            };
            const ImVec2 p0 = corner(0, 0);
            const ImVec2 p1 = corner(static_cast<float>(object.texture.width), 0);
            const ImVec2 p2 = corner(static_cast<float>(object.texture.width),
                                     static_cast<float>(object.texture.height));
            const ImVec2 p3 = corner(0, static_cast<float>(object.texture.height));
            attachmentHitRegions.push_back(
                {static_cast<std::size_t>(socketIt - frame.sockets.begin()), {p0, p1, p2, p3}});
            const ImVec2 textureTopLeft = mirrored ? ImVec2{1, 0} : ImVec2{0, 0};
            const ImVec2 textureTopRight = mirrored ? ImVec2{0, 0} : ImVec2{1, 0};
            const ImVec2 textureBottomRight = mirrored ? ImVec2{0, 1} : ImVec2{1, 1};
            const ImVec2 textureBottomLeft = mirrored ? ImVec2{1, 1} : ImVec2{0, 1};
            dl->AddImageQuad((ImTextureID)object.texture.id, p0, p1, p2, p3, textureTopLeft,
                             textureTopRight, textureBottomRight, textureBottomLeft);
        }
    };
    drawAttachmentLayer(teya::animation::AttachmentLayer::BehindOwner);
    ImGui::Image((ImTextureID)texture_.id, size, uv0, uv1);
    min = ImGui::GetItemRectMin();
    max = ImGui::GetItemRectMax();
    drawAttachmentLayer(teya::animation::AttachmentLayer::InFrontOfOwner);
    dl->AddRect(min, max, IM_COL32(255, 255, 255, 180));
    if (showGrid_ && pixel && z >= 4) {
        for (int x = 1; x < (int)source->width; ++x)
            dl->AddLine({min.x + x * z, min.y}, {min.x + x * z, max.y},
                        IM_COL32(255, 255, 255, 25));
        for (int y = 1; y < (int)source->height; ++y)
            dl->AddLine({min.x, min.y + y * z}, {max.x, min.y + y * z},
                        IM_COL32(255, 255, 255, 25));
    }
    if (showSockets_)
        for (size_t i = 0; i < frame.sockets.size(); ++i) {
            auto socket = mirrored ? teya::animation::mirrorSocket(frame.sockets[i], source->width)
                                   : frame.sockets[i];
            ImVec2 p{min.x + socket.position.x * z, min.y + socket.position.y * z};
            ImU32 color = s.kind == AnimationSelection::Kind::Socket && s.item == i
                              ? IM_COL32(255, 255, 0, 255)
                              : IM_COL32(0, 220, 255, 255);
            dl->AddCircleFilled(p, 5, color);
            float r = socket.rotationDegrees * DEG2RAD;
            dl->AddLine(p, {p.x + std::cos(r) * 20, p.y + std::sin(r) * 20}, color, 2);
            dl->AddText({p.x + 6, p.y - 8}, color, socket.name.c_str());
        }
    for (size_t i = 0; i < frame.hitboxes.size(); ++i) {
        auto b = mirrored ? teya::animation::mirrorRectangle(frame.hitboxes[i].localBounds,
                                                             source->width)
                          : frame.hitboxes[i].localBounds;
        ImU32 c = s.kind == AnimationSelection::Kind::Hitbox && s.item == i
                      ? IM_COL32(255, 255, 0, 255)
                      : IM_COL32(255, 60, 60, 220);
        dl->AddRect({min.x + b.x * z, min.y + b.y * z},
                    {min.x + (b.x + b.width) * z, min.y + (b.y + b.height) * z}, c, 0, 0, 2);
    }
    for (size_t i = 0; i < frame.markers.size(); ++i) {
        auto m = mirrored ? teya::animation::mirrorMarker(frame.markers[i], source->width)
                          : frame.markers[i];
        ImVec2 p{min.x + m.position.x * z, min.y + m.position.y * z};
        ImU32 color = s.kind == AnimationSelection::Kind::Marker && s.item == i
                          ? IM_COL32(255, 255, 0, 255)
                          : IM_COL32(100, 255, 100, 255);
        dl->AddLine({p.x - 7, p.y}, {p.x + 7, p.y}, color, 2);
        dl->AddLine({p.x, p.y - 7}, {p.x, p.y + 7}, color, 2);
    }
    const bool previewHovered = ImGui::IsItemHovered();
    auto &io = ImGui::GetIO();
    const Vector2 mouse{io.MousePos.x, io.MousePos.y};
    const Vector2 previewOrigin{min.x, min.y};
    auto mouseLocal = [&] {
        return animationPreviewScreenToLocal(mouse, previewOrigin, {}, z, mirrored, source->width);
    };
    auto pointInTriangle = [](ImVec2 point, ImVec2 aPoint, ImVec2 bPoint, ImVec2 cPoint) {
        auto side = [](ImVec2 p, ImVec2 a, ImVec2 b) {
            return (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
        };
        const bool negative = side(point, aPoint, bPoint) < 0 ||
                              side(point, bPoint, cPoint) < 0 ||
                              side(point, cPoint, aPoint) < 0;
        const bool positive = side(point, aPoint, bPoint) > 0 ||
                              side(point, bPoint, cPoint) > 0 ||
                              side(point, cPoint, aPoint) > 0;
        return !(negative && positive);
    };
    std::optional<std::size_t> attachmentSocket;
    const ImVec2 mousePosition{mouse.x, mouse.y};
    for (auto region = attachmentHitRegions.rbegin(); region != attachmentHitRegions.rend();
         ++region) {
        const auto &p = region->corners;
        if (pointInTriangle(mousePosition, p[0], p[1], p[2]) ||
            pointInTriangle(mousePosition, p[0], p[2], p[3])) {
            attachmentSocket = region->socketIndex;
            break;
        }
    }
    if ((previewHovered || attachmentSocket) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        float closestDistanceSquared = 12.0f * 12.0f;
        std::optional<std::size_t> closest;
        if (showSockets_ && previewHovered)
            for (std::size_t i = 0; i < frame.sockets.size(); ++i) {
                auto displayed = mirrored
                                     ? teya::animation::mirrorSocket(frame.sockets[i], source->width)
                                     : frame.sockets[i];
                const float dx = min.x + displayed.position.x * z - mouse.x;
                const float dy = min.y + displayed.position.y * z - mouse.y;
                const float distanceSquared = dx * dx + dy * dy;
                if (distanceSquared < closestDistanceSquared) {
                    closestDistanceSquared = distanceSquared;
                    closest = i;
                }
            }
        if (!closest)
            closest = attachmentSocket;
        if (closest) {
            playing_ = false;
            previewPlayer_.pause();
            s.kind = AnimationSelection::Kind::Socket;
            s.item = *closest;
            socketDragActive_ = true;
            socketDragChanged_ = false;
            socketDragClip_ = s.clip;
            socketDragFrame_ = s.frame;
            socketDragIndex_ = *closest;
            socketDragBefore_ = a;
            const Vector2 local = mouseLocal();
            socketDragOffset_ = {frame.sockets[*closest].position.x - local.x,
                                 frame.sockets[*closest].position.y - local.y};
        }
    }
    if (socketDragActive_) {
        const bool targetStillValid = socketDragClip_ == s.clip && socketDragFrame_ == s.frame &&
                                      socketDragIndex_ < frame.sockets.size();
        if (!targetStillValid) {
            socketDragActive_ = false;
            socketDragChanged_ = false;
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            Vector2 position = mouseLocal();
            position.x += socketDragOffset_.x;
            position.y += socketDragOffset_.y;
            const bool snap = io.KeyShift || (a.authoring.snapPositions && !io.KeyAlt);
            const float increment = a.authoring.positionSnap > 0 ? a.authoring.positionSnap : 1.0f;
            position.x = snapAnimationValue(position.x, snap, increment);
            position.y = snapAnimationValue(position.y, snap, increment);
            auto &socket = frame.sockets[socketDragIndex_];
            if (socket.position.x != position.x || socket.position.y != position.y) {
                socket.position = position;
                socketDragChanged_ = true;
            }
        } else {
            socketDragActive_ = false;
            if (socketDragChanged_)
                document_.mutate(socketDragBefore_);
            socketDragChanged_ = false;
        }
    }
    if (previewHovered && !socketDragActive_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        float closestDistanceSquared = 12.0f * 12.0f;
        std::optional<std::size_t> closest;
        for (std::size_t i = 0; i < frame.markers.size(); ++i) {
            auto displayed = mirrored
                                 ? teya::animation::mirrorMarker(frame.markers[i], source->width)
                                 : frame.markers[i];
            const float dx = min.x + displayed.position.x * z - mouse.x;
            const float dy = min.y + displayed.position.y * z - mouse.y;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < closestDistanceSquared) {
                closestDistanceSquared = distanceSquared;
                closest = i;
            }
        }
        if (closest) {
            playing_ = false;
            previewPlayer_.pause();
            s.kind = AnimationSelection::Kind::Marker;
            s.item = *closest;
            markerDragActive_ = true;
            markerDragChanged_ = false;
            markerDragClip_ = s.clip;
            markerDragFrame_ = s.frame;
            markerDragIndex_ = *closest;
            markerDragBefore_ = a;
            const Vector2 local = mouseLocal();
            markerDragOffset_ = {frame.markers[*closest].position.x - local.x,
                                 frame.markers[*closest].position.y - local.y};
        }
    }
    if (markerDragActive_) {
        const bool targetStillValid = markerDragClip_ == s.clip && markerDragFrame_ == s.frame &&
                                      markerDragIndex_ < frame.markers.size();
        if (!targetStillValid) {
            markerDragActive_ = false;
            markerDragChanged_ = false;
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            Vector2 position = mouseLocal();
            position.x += markerDragOffset_.x;
            position.y += markerDragOffset_.y;
            const bool snap = io.KeyShift || (a.authoring.snapPositions && !io.KeyAlt);
            const float increment = a.authoring.positionSnap > 0 ? a.authoring.positionSnap : 1.0f;
            position.x = snapAnimationValue(position.x, snap, increment);
            position.y = snapAnimationValue(position.y, snap, increment);
            auto &marker = frame.markers[markerDragIndex_];
            if (marker.position.x != position.x || marker.position.y != position.y) {
                marker.position = position;
                markerDragChanged_ = true;
            }
        } else {
            markerDragActive_ = false;
            if (markerDragChanged_)
                document_.mutate(markerDragBefore_);
            markerDragChanged_ = false;
        }
    }
    if (previewHovered && !socketDragActive_ && !markerDragActive_ &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const Vector2 local = mouseLocal();
        for (std::size_t i = frame.hitboxes.size(); i-- > 0;) {
            const auto &bounds = frame.hitboxes[i].localBounds;
            const float left = std::min(bounds.x, bounds.x + bounds.width);
            const float right = std::max(bounds.x, bounds.x + bounds.width);
            const float top = std::min(bounds.y, bounds.y + bounds.height);
            const float bottom = std::max(bounds.y, bounds.y + bounds.height);
            if (local.x >= left && local.x <= right && local.y >= top && local.y <= bottom) {
                playing_ = false;
                previewPlayer_.pause();
                s.kind = AnimationSelection::Kind::Hitbox;
                s.item = i;
                hitboxDragActive_ = true;
                hitboxDragChanged_ = false;
                hitboxDragClip_ = s.clip;
                hitboxDragFrame_ = s.frame;
                hitboxDragIndex_ = i;
                hitboxDragBefore_ = a;
                hitboxDragOffset_ = {bounds.x - local.x, bounds.y - local.y};
                break;
            }
        }
    }
    if (hitboxDragActive_) {
        const bool targetStillValid = hitboxDragClip_ == s.clip && hitboxDragFrame_ == s.frame &&
                                      hitboxDragIndex_ < frame.hitboxes.size();
        if (!targetStillValid) {
            hitboxDragActive_ = false;
            hitboxDragChanged_ = false;
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            Vector2 position = mouseLocal();
            position.x += hitboxDragOffset_.x;
            position.y += hitboxDragOffset_.y;
            const bool snap = io.KeyShift || (a.authoring.snapPositions && !io.KeyAlt);
            const float increment = a.authoring.positionSnap > 0 ? a.authoring.positionSnap : 1.0f;
            position.x = snapAnimationValue(position.x, snap, increment);
            position.y = snapAnimationValue(position.y, snap, increment);
            auto &bounds = frame.hitboxes[hitboxDragIndex_].localBounds;
            if (bounds.x != position.x || bounds.y != position.y) {
                bounds.x = position.x;
                bounds.y = position.y;
                hitboxDragChanged_ = true;
            }
        } else {
            hitboxDragActive_ = false;
            if (hitboxDragChanged_)
                document_.mutate(hitboxDragBefore_);
            hitboxDragChanged_ = false;
        }
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        auto d = ImGui::GetIO().MouseDelta;
        pan_.x += d.x;
        pan_.y += d.y;
    }
    if (playing_) {
        previewPlayer_.resume();
        previewPlayer_.update(ImGui::GetIO().DeltaTime);
        if (previewPlayer_.currentClipName() == clip.name)
            s.frame = std::min(previewPlayer_.currentFrameIndex(), clip.frames.size() - 1);
        auto events = previewPlayer_.consumeEvents();
        for (auto &e : events) {
            if (eventLog_.size() == 64)
                eventLog_.pop_front();
            eventLog_.push_back({e, previewPlayer_.clipElapsed()});
        }
    }
    ImGui::Text("Frame %zu/%zu  Time %.3f / %.3f", s.frame + 1, clip.frames.size(),
                previewPlayer_.clipElapsed(), clipDuration(clip));
    if (ImGui::CollapsingHeader("Preview Event Log")) {
        if (ImGui::Button("Clear events"))
            eventLog_.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("Scrubbing never dispatches gameplay events");
        for (auto &e : eventLog_)
            ImGui::BulletText("%.3f %s[%zu] %s %s", e.previewTime, e.event.clipName.c_str(),
                              e.event.frameIndex, e.event.name.c_str(), e.event.payload.c_str());
    }
}
void AnimationEditorPanel::inspector(EditorHost &host) {
    auto &a = document_.asset();
    auto &s = document_.selection();
    if (ImGui::CollapsingHeader("Asset")) {
        ImGui::Text("Schema %d", a.schemaVersion);
        auto texturePath = buffer<256>(a.texturePath);
        auto before = a;
        if (ImGui::InputText("Texture path", texturePath.data(), texturePath.size())) {
            a.texturePath = texturePath.data();
            document_.mutate(before);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Save to load the changed texture into the preview and running game");
        before = a;
        int sourceMode =
            a.sourceMode == teya::animation::AnimationFrameSourceMode::SpriteSheetGrid ? 0 : 1;
        if (ImGui::Combo("Frame source", &sourceMode, "Sprite Sheet Grid\0Texture Atlas\0")) {
            a.sourceMode = sourceMode == 0
                               ? teya::animation::AnimationFrameSourceMode::SpriteSheetGrid
                               : teya::animation::AnimationFrameSourceMode::TextureAtlas;
            document_.mutate(before);
            sheetSelection_.clear();
        }
        before = a;
        int mode = a.render.mode == teya::animation::AnimationRenderMode::PixelArt ? 0 : 1;
        if (ImGui::Combo("Render mode", &mode, "Pixel Art\0Smooth\0")) {
            a.render.mode = mode == 0 ? teya::animation::AnimationRenderMode::PixelArt
                                      : teya::animation::AnimationRenderMode::Smooth;
            if (mode == 0) {
                a.render.textureFilter = teya::animation::AnimationTextureFilter::Nearest;
                a.authoring.snapPositions = true;
                a.authoring.positionSnap = 1;
                a.authoring.preferIntegerPreviewScale = true;
            } else {
                a.render.textureFilter = teya::animation::AnimationTextureFilter::Linear;
                a.authoring.snapPositions = false;
                a.authoring.preferIntegerPreviewScale = false;
                a.render.roundOwnerPosition = a.render.roundAttachmentPositions = false;
            }
            document_.mutate(before);
        }
        before = a;
        int filter =
            a.render.textureFilter == teya::animation::AnimationTextureFilter::Nearest ? 0 : 1;
        if (ImGui::Combo("Texture filter", &filter, "Nearest\0Linear\0")) {
            a.render.textureFilter = filter == 0 ? teya::animation::AnimationTextureFilter::Nearest
                                                 : teya::animation::AnimationTextureFilter::Linear;
            document_.mutate(before);
        }
        before = a;
        bool changed = false;
        changed |= ImGui::Checkbox("Snap positions", &a.authoring.snapPositions);
        changed |= ImGui::InputFloat("Position snap", &a.authoring.positionSnap, 0, 0, "%.3f");
        changed |= ImGui::Checkbox("Snap rotation", &a.authoring.snapRotation);
        changed |=
            ImGui::InputFloat("Rotation snap", &a.authoring.rotationSnapDegrees, 0, 0, "%.3f");
        changed |= ImGui::Checkbox("Snap scale", &a.authoring.snapScale);
        changed |= ImGui::InputFloat("Scale snap", &a.authoring.scaleSnap, 0, 0, "%.4f");
        changed |= ImGui::Checkbox("Integer preview scale", &a.authoring.preferIntegerPreviewScale);
        changed |= ImGui::Checkbox("Pixel grid default", &a.authoring.showPixelGridByDefault);
        changed |= ImGui::Checkbox("Round owner", &a.render.roundOwnerPosition);
        changed |= ImGui::Checkbox("Round attachments", &a.render.roundAttachmentPositions);
        if (changed)
            document_.mutate(before);
        if (a.sourceMode == teya::animation::AnimationFrameSourceMode::SpriteSheetGrid) {
            before = a;
            changed = false;
            changed |= ImGui::InputInt("Frame width", &a.frameWidth);
            changed |= ImGui::InputInt("Frame height", &a.frameHeight);
            changed |= ImGui::InputInt("Sheet columns", &a.sheetColumns);
            if (changed)
                document_.mutate(before);
        } else {
            ImGui::Text("Atlas regions: %d", (int)a.atlasRegions.size());
        }
    }
    if (a.clips.empty())
        return;
    actionBindings();
    auto &clip = a.clips[s.clip];
    if (ImGui::CollapsingHeader("Clip")) {
        auto b = buffer<128>(clip.name);
        auto before = a;
        if (ImGui::InputText("Name", b.data(), b.size())) {
            const auto oldName = clip.name;
            clip.name = b.data();
            for (auto &binding : a.controller.bindings)
                if (binding.clipName == oldName)
                    binding.clipName = clip.name;
            document_.mutate(before);
        }
        before = a;
        if (ImGui::Checkbox("Looping", &clip.looping))
            document_.mutate(before);
        before = a;
        if (ImGui::Checkbox("Mirror clip", &clip.mirrored))
            document_.mutate(before);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Always play this clip horizontally mirrored. Duplicate a clip "
                              "and enable this on the copy to reuse the same source frames for "
                              "the opposite direction.");
        ImGui::Text("%zu frames, %.3f seconds", clip.frames.size(), clipDuration(clip));
    }
    if (clip.frames.empty())
        return;
    auto &frame = clip.frames[s.frame];
    if (ImGui::CollapsingHeader("Frame")) {
        auto before = a;
        float duration = frame.durationSeconds;
        if (ImGui::InputFloat("Duration", &duration, 0, 0, "%.4f") && std::isfinite(duration) &&
            duration > 0) {
            frame.durationSeconds = duration;
            document_.mutate(before);
        }
        if (a.sourceMode == teya::animation::AnimationFrameSourceMode::SpriteSheetGrid) {
            before = a;
            int index = frame.source.spriteIndex;
            if (ImGui::InputInt("Sprite index", &index) && index >= 0) {
                frame.source.spriteIndex = index;
                document_.mutate(before);
            }
            auto r = teya::animation::animationSourceRectangle(a, frame);
            if (r)
                ImGui::Text("Row %d, column %d | %.0f,%.0f %.0fx%.0f",
                            a.sheetColumns ? index / a.sheetColumns : 0,
                            a.sheetColumns ? index % a.sheetColumns : 0, r->x, r->y, r->width,
                            r->height);
        } else {
            if (ImGui::BeginCombo("Atlas region", frame.source.atlasRegion.c_str())) {
                for (auto &r : a.atlasRegions)
                    if (ImGui::Selectable(r.name.empty() ? r.id.c_str() : r.name.c_str(),
                                          frame.source.atlasRegion == r.id)) {
                        before = a;
                        frame.source.atlasRegion = r.id;
                        document_.mutate(before);
                    }
                ImGui::EndCombo();
            }
        }
        if (ImGui::Button("Copy frame")) {
            s.kind = AnimationSelection::Kind::Frame;
            document_.copySelection();
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste frame"))
            document_.paste();
    }
    frameCollections(host, frame);
    if (ImGui::CollapsingHeader("Validation")) {
        int errors = 0, warnings = 0;
        for (auto &i : validation_.issues)
            (i.severity == teya::animation::AnimationValidationSeverity::Error ? errors
                                                                               : warnings)++;
        ImGui::Text("%d errors, %d warnings", errors, warnings);
        for (auto &i : validation_.issues) {
            ImVec4 color = i.severity == teya::animation::AnimationValidationSeverity::Error
                               ? ImVec4(1, .35f, .35f, 1)
                               : ImVec4(1, .75f, .2f, 1);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (ImGui::Selectable(i.message.c_str())) {
                if (i.clipIndex)
                    s.clip = std::min(*i.clipIndex, a.clips.size() - 1);
                if (i.frameIndex && !a.clips[s.clip].frames.empty())
                    s.frame = std::min(*i.frameIndex, a.clips[s.clip].frames.size() - 1);
            }
            ImGui::PopStyleColor();
        }
    }
}
void AnimationEditorPanel::actionBindings() {
    auto &asset = document_.asset();
    auto &selection = document_.selection();
    if (!ImGui::CollapsingHeader("Action Bindings"))
        return;

    auto defaultAction = buffer<96>(asset.controller.defaultAction);
    auto before = asset;
    if (ImGui::InputText("Default action", defaultAction.data(), defaultAction.size())) {
        asset.controller.defaultAction = defaultAction.data();
        document_.mutate(before);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The looping action used when the controller first starts");

    if (ImGui::Button("+ Bind selected clip")) {
        before = asset;
        const auto &clip = asset.clips[selection.clip];
        std::string action = clip.name;
        auto direction = teya::animation::AnimationDirection::Any;
        const struct {
            const char *suffix;
            teya::animation::AnimationDirection direction;
        } suffixes[] = {{"_down", teya::animation::AnimationDirection::Down},
                        {"_up", teya::animation::AnimationDirection::Up},
                        {"_right", teya::animation::AnimationDirection::Right},
                        {"_left", teya::animation::AnimationDirection::Left}};
        for (const auto &suffix : suffixes) {
            const std::string_view ending = suffix.suffix;
            if (action.size() > ending.size() &&
                action.compare(action.size() - ending.size(), ending.size(), ending) == 0) {
                action.resize(action.size() - ending.size());
                direction = suffix.direction;
                break;
            }
        }
        asset.controller.bindings.push_back({std::move(action), direction, clip.name,
                                             clip.looping
                                                 ? teya::animation::AnimationActionMode::Looping
                                                 : teya::animation::AnimationActionMode::OneShot,
                                             clip.looping ? 0 : 10});
        if (asset.controller.bindings.size() == 1)
            asset.controller.defaultAction = asset.controller.bindings.front().action;
        document_.mutate(before);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Directional suffixes are detected automatically");

    for (std::size_t i = 0; i < asset.controller.bindings.size(); ++i) {
        auto &binding = asset.controller.bindings[i];
        ImGui::PushID(static_cast<int>(i));
        const std::string label =
            binding.action + " / " + directionName(binding.direction) + " -> " + binding.clipName;
        if (ImGui::TreeNode(label.c_str())) {
            before = asset;
            auto action = buffer<96>(binding.action);
            bool changed = ImGui::InputText("Action", action.data(), action.size());
            int direction = static_cast<int>(binding.direction);
            if (ImGui::Combo("Direction", &direction, "Any\0Down\0Up\0Right\0Left\0")) {
                binding.direction = static_cast<teya::animation::AnimationDirection>(direction);
                changed = true;
            }
            if (ImGui::BeginCombo("Clip", binding.clipName.c_str())) {
                for (const auto &clip : asset.clips)
                    if (ImGui::Selectable(clip.name.c_str(), binding.clipName == clip.name)) {
                        binding.clipName = clip.name;
                        changed = true;
                    }
                ImGui::EndCombo();
            }
            int mode = binding.mode == teya::animation::AnimationActionMode::Looping ? 0 : 1;
            if (ImGui::Combo("Mode", &mode, "Looping\0One Shot\0")) {
                binding.mode = mode == 0 ? teya::animation::AnimationActionMode::Looping
                                         : teya::animation::AnimationActionMode::OneShot;
                changed = true;
            }
            changed |= ImGui::InputInt("Priority", &binding.priority);
            if (changed) {
                binding.action = action.data();
                document_.mutate(before);
            }
            if (ImGui::Button("Remove binding")) {
                before = asset;
                asset.controller.bindings.erase(asset.controller.bindings.begin() + i);
                document_.mutate(before);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (asset.controller.bindings.empty())
        ImGui::TextDisabled("No bindings saved. The game may use its legacy fallback mapping.");
}
void AnimationEditorPanel::frameCollections(EditorHost &host, teya::animation::AnimationFrame &f) {
    auto &a = document_.asset();
    auto &s = document_.selection();
    auto editVec2 = [&](const char *label, Vector2 &v) {
        bool c = ImGui::InputFloat2(label, &v.x, "%.3f");
        if (c) {
            v.x = snapAnimationValue(v.x, a.authoring.snapPositions, a.authoring.positionSnap);
            v.y = snapAnimationValue(v.y, a.authoring.snapPositions, a.authoring.positionSnap);
        }
        return c;
    };
    if (ImGui::CollapsingHeader("Sockets")) {
        if (ImGui::Button("+ Socket")) {
            auto before = a;
            f.sockets.push_back(
                {"socket", {}, 0, {1, 1}, true, teya::animation::AttachmentLayer::InFrontOfOwner});
            document_.mutate(before);
        }
        for (size_t i = 0; i < f.sockets.size(); ++i) {
            ImGui::PushID((int)i);
            auto &x = f.sockets[i];
            if (ImGui::TreeNode(x.name.c_str())) {
                s.kind = AnimationSelection::Kind::Socket;
                s.item = i;
                auto before = a;
                bool c = false;
                auto b = buffer<96>(x.name);
                if (ImGui::InputText("Name", b.data(), b.size())) {
                    x.name = b.data();
                    c = true;
                }
                c |= editVec2("Position", x.position);
                c |= ImGui::InputFloat("Rotation", &x.rotationDegrees, 0, 0, "%.3f deg");
                c |= ImGui::SliderFloat("Rotation slider", &x.rotationDegrees, -180.0f, 180.0f,
                                        "%.1f deg");
                c |= ImGui::InputFloat2("Scale", &x.scale.x, "%.4f");
                c |= ImGui::Checkbox("Visible", &x.visible);
                int layer = x.layer == teya::animation::AttachmentLayer::BehindOwner ? 0 : 1;
                if (ImGui::Combo("Layer", &layer, "Behind Owner\0In Front\0")) {
                    x.layer = layer == 0 ? teya::animation::AttachmentLayer::BehindOwner
                                         : teya::animation::AttachmentLayer::InFrontOfOwner;
                    c = true;
                }
                if (c) {
                    x.rotationDegrees =
                        snapAnimationValue(x.rotationDegrees, a.authoring.snapRotation,
                                           a.authoring.rotationSnapDegrees);
                    x.scale.x =
                        snapAnimationValue(x.scale.x, a.authoring.snapScale, a.authoring.scaleSnap);
                    x.scale.y =
                        snapAnimationValue(x.scale.y, a.authoring.snapScale, a.authoring.scaleSnap);
                    document_.mutate(before);
                }
                if (ImGui::Button("Propagate all"))
                    document_.propagateSocket(i, 0, a.clips[s.clip].frames.size() - 1);
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    before = a;
                    f.sockets.erase(f.sockets.begin() + i);
                    document_.mutate(before);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Events")) {
        if (ImGui::Button("+ Event")) {
            auto before = a;
            f.events.push_back({"event", {}});
            document_.mutate(before);
        }
        for (size_t i = 0; i < f.events.size(); ++i) {
            ImGui::PushID((int)i);
            auto before = a;
            auto nb = buffer<96>(f.events[i].name);
            auto pb = buffer<160>(f.events[i].payload);
            bool c = ImGui::InputText("Name", nb.data(), nb.size());
            c |= ImGui::InputText("Payload", pb.data(), pb.size());
            if (c) {
                f.events[i].name = nb.data();
                f.events[i].payload = pb.data();
                document_.mutate(before);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                before = a;
                f.events.erase(f.events.begin() + i);
                document_.mutate(before);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        ImGui::TextDisabled("Suggestions: attack_started, attack_active, spawn_slash, "
                            "attack_finished, play_sound, footstep");
    }
    if (ImGui::CollapsingHeader("Hitboxes")) {
        if (ImGui::Button("+ Hitbox")) {
            auto before = a;
            f.hitboxes.push_back({"hitbox", {0, 0, 8, 8}, true});
            document_.mutate(before);
        }
        for (size_t i = 0; i < f.hitboxes.size(); ++i) {
            ImGui::PushID((int)i);
            auto &x = f.hitboxes[i];
            if (ImGui::TreeNode(x.name.c_str())) {
                s.kind = AnimationSelection::Kind::Hitbox;
                s.item = i;
                auto before = a;
                bool c = ImGui::InputFloat4("Bounds", &x.localBounds.x, "%.3f");
                c |= ImGui::Checkbox("Active", &x.active);
                if (c)
                    document_.mutate(before);
                if (ImGui::Button("Propagate all"))
                    document_.propagateHitbox(i, 0, a.clips[s.clip].frames.size() - 1);
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    before = a;
                    f.hitboxes.erase(f.hitboxes.begin() + i);
                    document_.mutate(before);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Markers")) {
        if (ImGui::Button("+ Marker")) {
            auto before = a;
            f.markers.push_back({"marker", "effect", {}, 0, {}});
            document_.mutate(before);
        }
        for (size_t i = 0; i < f.markers.size(); ++i) {
            ImGui::PushID((int)i);
            auto &x = f.markers[i];
            if (ImGui::TreeNode(x.name.c_str())) {
                s.kind = AnimationSelection::Kind::Marker;
                s.item = i;
                auto before = a;
                bool c = editVec2("Position", x.position);
                c |= ImGui::InputFloat("Rotation", &x.rotationDegrees, 0, 0, "%.3f");
                auto tb = buffer<96>(x.type);
                auto pb = buffer<160>(x.payload);
                c |= ImGui::InputText("Type", tb.data(), tb.size());
                c |= ImGui::InputText("Payload", pb.data(), pb.size());
                if (c) {
                    x.type = tb.data();
                    x.payload = pb.data();
                    document_.mutate(before);
                }
                if (ImGui::Button("Propagate all"))
                    document_.propagateMarker(i, 0, a.clips[s.clip].frames.size() - 1);
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    before = a;
                    f.markers.erase(f.markers.begin() + i);
                    document_.mutate(before);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}
void AnimationEditorPanel::timeline() {
    TEYA_PROFILE_ZONE_NAMED("AnimationEditor::timeline");
    auto &a = document_.asset();
    auto &s = document_.selection();
    if (a.clips.empty())
        return;
    auto &c = a.clips[s.clip];
    ImGui::SeparatorText("Timeline");
    if (ImGui::Button("Previous") && !c.frames.empty()) {
        s.frame = s.frame ? s.frame - 1 : c.frames.size() - 1;
        previewPlayer_.setFrameForPreview(s.frame);
    }
    ImGui::SameLine();
    if (ImGui::Button("Next") && !c.frames.empty()) {
        s.frame = (s.frame + 1) % c.frames.size();
        previewPlayer_.setFrameForPreview(s.frame);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Frame"))
        document_.addFrame();
    ImGui::SameLine();
    if (ImGui::Button("Duplicate"))
        document_.duplicateFrame();
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
        document_.deleteFrame();
    ImGui::SameLine();
    if (ImGui::Button("Move Left"))
        document_.moveFrame(-1);
    ImGui::SameLine();
    if (ImGui::Button("Move Right"))
        document_.moveFrame(1);
    ImGui::BeginChild("frame-timeline", {0, 105}, true, ImGuiWindowFlags_HorizontalScrollbar);
    float start = 0;
    for (size_t i = 0; i < c.frames.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::BeginGroup();
        bool selected = i == s.frame;
        std::string source =
            a.sourceMode == teya::animation::AnimationFrameSourceMode::SpriteSheetGrid
                ? "#" + std::to_string(c.frames[i].source.spriteIndex)
                : c.frames[i].source.atlasRegion;
        if (ImGui::Selectable((std::to_string(i) + " " + source).c_str(), selected, 0, {95, 35})) {
            s.frame = i;
            previewPlayer_.setFrameForPreview(i);
            previewPlayer_.consumeEvents();
        }
        ImGui::Text("%.3fs", c.frames[i].durationSeconds);
        ImGui::TextDisabled("S%d E%d H%d M%d", (int)c.frames[i].sockets.size(),
                            (int)c.frames[i].events.size(), (int)c.frames[i].hitboxes.size(),
                            (int)c.frames[i].markers.size());
        ImGui::TextDisabled("%.3f-%.3f", start, start + c.frames[i].durationSeconds);
        start += c.frames[i].durationSeconds;
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::EndChild();
}
void AnimationEditorPanel::shortcuts(EditorHost &h) {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
        ImGui::GetIO().WantTextInput || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        return;
    auto &io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl, shift = io.KeyShift;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        save(h);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_R, false))
        reload(h);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (shift)
            document_.redo();
        else
            document_.undo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        document_.redo();
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        playing_ = !playing_;
        if (playing_)
            previewPlayer_.resume();
        else
            previewPlayer_.pause();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) && !document_.asset().clips.empty()) {
        auto &c = document_.asset().clips[document_.selection().clip];
        if (!c.frames.empty())
            document_.selection().frame =
                document_.selection().frame ? document_.selection().frame - 1 : c.frames.size() - 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) && !document_.asset().clips.empty()) {
        auto &c = document_.asset().clips[document_.selection().clip];
        if (!c.frames.empty())
            document_.selection().frame = (document_.selection().frame + 1) % c.frames.size();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
        document_.selection().frame = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_End, false) && !document_.asset().clips.empty()) {
        auto &c = document_.asset().clips[document_.selection().clip];
        if (!c.frames.empty())
            document_.selection().frame = c.frames.size() - 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G, false))
        showGrid_ = !showGrid_;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        if (document_.selection().kind == AnimationSelection::Kind::Frame)
            document_.duplicateFrame();
    }
}
} // namespace teya::editor
