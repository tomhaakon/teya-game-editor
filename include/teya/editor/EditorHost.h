#pragma once
#include <cstdint>
#include <memory>
#include <raylib.h>
#include <string>
#include <teya/animation/AnimationAsset.h>
#include <vector>

namespace teya::editor {
using RuntimeObjectId = std::uint64_t;
struct RuntimeNode {
    RuntimeObjectId id = 0;
    RuntimeObjectId parentId = 0;
    std::string name;
};
struct RuntimeProperty {
    std::string name;
    std::string value;
};
struct EditorFrameMetrics {
    float fps = 0.0f, frameMilliseconds = 0.0f, updateMilliseconds = 0.0f;
    float drawMilliseconds = 0.0f, editorMilliseconds = 0.0f;
    int canvasWidth = 0, canvasHeight = 0, imageWidth = 0, imageHeight = 0;
    float gameViewScale = 0.0f;
    std::string currentState, currentMap;
    int colliderCount = -1;
};
struct EditorDebugDrawSettings {
    bool showPlayerOrigin = false;
    bool showPlayerCollider = false;
    bool showWorldBounds = false;
    bool showAnimationHitboxes = false;
    bool showAnimationSockets = false;
    bool showAnimationMarkers = false;
};
struct EditableAnimationAssetInfo {
    std::uint64_t id = 0;
    std::string displayName;
    std::string assetPath;
    bool valid = false;
    int errorCount = 0, warningCount = 0;
    bool runtimeAsset = false;
};
struct AnimationAssetOperationResult {
    bool success = false;
    std::uint64_t assetId = 0;
    std::string error;
    explicit operator bool() const { return success; }
};
struct AnimationWorkingCopyResult {
    std::shared_ptr<const teya::animation::AnimationAsset> asset;
    Texture2D texture{};
    int textureWidth = 0, textureHeight = 0;
    std::string error;
    explicit operator bool() const { return asset != nullptr; }
};
struct AnimationSaveResult {
    bool saved = false, applied = false;
    std::string error;
    explicit operator bool() const { return saved && applied; }
};
struct AttachmentPreviewInfo {
    std::uint64_t id = 0;
    std::string name;
    std::string socketName;
    std::string texturePath;
    Texture2D texture{};
    Vector2 pivot{};
    Vector2 effectTip{};
    Vector2 positionOffset{};
    float rotationOffsetDegrees = 0.0f;
    Vector2 scale{1.0f, 1.0f};
    teya::animation::AttachmentLayer layer =
        teya::animation::AttachmentLayer::InFrontOfOwner;
    bool visible = true;
    bool smoothRotationFiltering = false;
    bool trailEnabled = true;
    float trailLifetimeSeconds = 0.25f;
    float trailWidth = 9.0f;
    float trailOpacity = 0.45f;
    float trailSmoothing = 0.35f;
    bool ownsTexture = false;
};
struct AttachmentObjectSaveResult {
    bool success = false;
    std::string error;
    explicit operator bool() const { return success; }
};
class EditorHost {
  public:
    virtual ~EditorHost() = default;
    virtual RenderTexture2D gameViewTexture() const = 0;
    virtual int gameCanvasWidth() const = 0;
    virtual int gameCanvasHeight() const = 0;
    virtual std::vector<RuntimeNode> runtimeHierarchy() const = 0;
    virtual std::vector<RuntimeProperty> inspectObject(RuntimeObjectId id) const = 0;
    virtual EditorFrameMetrics frameMetrics() const = 0;
    virtual void setDebugDrawSettings(const EditorDebugDrawSettings &) {}
    virtual std::vector<EditableAnimationAssetInfo> editableAnimationAssets() const { return {}; }
    virtual AnimationAssetOperationResult createAnimationAsset(std::string_view) {
        return {false, 0, "Asset creation is unavailable"};
    }
    virtual AnimationAssetOperationResult duplicateAnimationAsset(std::uint64_t, std::string_view) {
        return {false, 0, "Asset duplication is unavailable"};
    }
    virtual AnimationAssetOperationResult renameAnimationAsset(std::uint64_t, std::string_view) {
        return {false, 0, "Asset renaming is unavailable"};
    }
    virtual AnimationAssetOperationResult deleteAnimationAsset(std::uint64_t) {
        return {false, 0, "Asset deletion is unavailable"};
    }
    virtual AnimationWorkingCopyResult loadAnimationWorkingCopy(std::uint64_t) { return {}; }
    virtual teya::animation::AnimationValidationResult
    validateEditableAnimation(const teya::animation::AnimationAsset &asset, int textureWidth,
                              int textureHeight) const {
        return teya::animation::validateAnimationAsset(asset, textureWidth, textureHeight);
    }
    virtual AnimationSaveResult
    saveAndApplyAnimationAsset(std::uint64_t, const teya::animation::AnimationAsset &) {
        return {false, false, "Animation authoring is unavailable"};
    }
    virtual AnimationSaveResult
    applyAnimationAssetWithoutSaving(std::uint64_t, const teya::animation::AnimationAsset &) {
        return {false, false, "Temporary apply is unavailable"};
    }
    virtual std::vector<AttachmentPreviewInfo> attachmentPreviews(std::uint64_t) const {
        return {};
    }
    virtual AttachmentObjectSaveResult
    saveAttachmentObjects(std::uint64_t, const std::vector<AttachmentPreviewInfo> &) {
        return {false, "Attachment object editing is unavailable"};
    }
    virtual std::vector<std::string> animationEventSuggestions() const {
        return {"attack_started",  "attack_active", "spawn_slash",
                "attack_finished", "play_sound",    "footstep"};
    }
    virtual std::vector<std::string> animationMarkerTypeSuggestions() const {
        return {"effect", "sound", "footstep", "interaction", "camera"};
    }
    virtual std::string editorLogPath() const { return {}; }
    virtual void flushEditorLog() {}
    virtual void requestGameRestart(bool pauseAfterRestart) { (void)pauseAfterRestart; }
    virtual void requestExit() = 0;
};
} // namespace teya::editor
