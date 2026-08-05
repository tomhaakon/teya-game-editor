#include "teya/editor/panels/AnimationEditorPanel.h"
#include "teya/editor/EditorContext.h"
#include <algorithm>
#include <array>
#include <cmath>
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
float clipDuration(const teya::animation::AnimationClip &c) {
    float t = 0;
    for (auto &f : c.frames)
        t += f.durationSeconds;
    return t;
}
} // namespace
void AnimationEditorPanel::draw(EditorHost &host, EditorContext &) {
    TEYA_PROFILE_ZONE_NAMED("AnimationEditorPanel::draw");
    if (!open)
        return;
    if (!ImGui::Begin("Animation Editor", &open)) {
        ImGui::End();
        return;
    }
    assets_ = host.editableAnimationAssets();
    if (!loaded_ && !assets_.empty())
        load(host, assets_[0].id);
    assetBar(host);
    if (loaded_) {
        syncPreview();
        shortcuts(host);
        if (ImGui::BeginTable("animation-layout", 3,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Clips", ImGuiTableColumnFlags_WidthFixed, 180);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 330);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            clips();
            // Structural clip edits happen immediately. Refresh the immutable
            // preview snapshot before it can write its old frame index back.
            syncPreview();
            ImGui::TableNextColumn();
            preview(host);
            ImGui::TableNextColumn();
            inspector(host);
            ImGui::EndTable();
        }
        timeline();
    }
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
    fit_ = true;
    validate(h);
    previewRevision_ = ~std::uint64_t{0};
    message_ = "Loaded working copy";
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
        document_.markSaved();
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
    if (ImGui::BeginCombo("Asset", current)) {
        for (auto &a : assets_) {
            bool selected = a.id == document_.assetId();
            if (ImGui::Selectable(a.displayName.c_str(), selected)) {
                if (document_.dirty())
                    message_ = "Save or Reload before switching dirty assets";
                else
                    load(h, a.id);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
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
    ImGui::SameLine();
    if (ImGui::Button("Apply Without Saving"))
        applyTemporary(h);
    if (document_.temporaryApplied()) {
        ImGui::SameLine();
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
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
        document_.deleteClip();
    ImGui::Separator();
    for (size_t i = 0; i < a.clips.size(); ++i)
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
    ImGui::SameLine();
    ImGui::Checkbox("Left-facing", &facingLeft_);
    ImGui::SameLine();
    ImGui::Checkbox("Fit", &fit_);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid_);
    ImGui::SameLine();
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
    Rectangle shown = *source;
    if (facingLeft_) {
        shown.x += shown.width;
        shown.width = -shown.width;
    }
    rlImGuiImageRect(&texture_, (int)size.x, (int)size.y, shown);
    ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    auto *dl = ImGui::GetWindowDrawList();
    dl->AddRect(min, max, IM_COL32(255, 255, 255, 180));
    if (showGrid_ && pixel && z >= 4) {
        for (int x = 1; x < (int)source->width; ++x)
            dl->AddLine({min.x + x * z, min.y}, {min.x + x * z, max.y},
                        IM_COL32(255, 255, 255, 25));
        for (int y = 1; y < (int)source->height; ++y)
            dl->AddLine({min.x, min.y + y * z}, {max.x, min.y + y * z},
                        IM_COL32(255, 255, 255, 25));
    }
    for (size_t i = 0; i < frame.sockets.size(); ++i) {
        auto socket = facingLeft_ ? teya::animation::mirrorSocket(frame.sockets[i], source->width)
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
        auto b = facingLeft_ ? teya::animation::mirrorRectangle(frame.hitboxes[i].localBounds,
                                                                source->width)
                             : frame.hitboxes[i].localBounds;
        ImU32 c = s.kind == AnimationSelection::Kind::Hitbox && s.item == i
                      ? IM_COL32(255, 255, 0, 255)
                      : IM_COL32(255, 60, 60, 220);
        dl->AddRect({min.x + b.x * z, min.y + b.y * z},
                    {min.x + (b.x + b.width) * z, min.y + (b.y + b.height) * z}, c, 0, 0, 2);
    }
    for (size_t i = 0; i < frame.markers.size(); ++i) {
        auto m = facingLeft_ ? teya::animation::mirrorMarker(frame.markers[i], source->width)
                             : frame.markers[i];
        ImVec2 p{min.x + m.position.x * z, min.y + m.position.y * z};
        dl->AddLine({p.x - 6, p.y}, {p.x + 6, p.y}, IM_COL32(100, 255, 100, 255), 2);
        dl->AddLine({p.x, p.y - 6}, {p.x, p.y + 6}, IM_COL32(100, 255, 100, 255), 2);
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
    if (ImGui::CollapsingHeader("Asset", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Schema %d", a.schemaVersion);
        ImGui::TextWrapped("Texture: %s", a.texturePath.c_str());
        auto before = a;
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
    auto &clip = a.clips[s.clip];
    if (ImGui::CollapsingHeader("Clip", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto b = buffer<128>(clip.name);
        auto before = a;
        if (ImGui::InputText("Name", b.data(), b.size())) {
            clip.name = b.data();
            document_.mutate(before);
        }
        before = a;
        if (ImGui::Checkbox("Looping", &clip.looping))
            document_.mutate(before);
        ImGui::Text("%zu frames, %.3f seconds", clip.frames.size(), clipDuration(clip));
    }
    if (clip.frames.empty())
        return;
    auto &frame = clip.frames[s.frame];
    if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if (ImGui::CollapsingHeader("Validation", ImGuiTreeNodeFlags_DefaultOpen)) {
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
                c |= ImGui::InputFloat("Rotation", &x.rotationDegrees, 0, 0, "%.3f");
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
    if (ImGui::IsKeyPressed(ImGuiKey_F, false))
        facingLeft_ = !facingLeft_;
    if (ImGui::IsKeyPressed(ImGuiKey_G, false))
        showGrid_ = !showGrid_;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        if (document_.selection().kind == AnimationSelection::Kind::Frame)
            document_.duplicateFrame();
    }
}
} // namespace teya::editor
