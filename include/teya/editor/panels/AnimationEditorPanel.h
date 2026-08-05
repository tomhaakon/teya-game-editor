#pragma once
#include "teya/editor/AnimationDocument.h"
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/Panel.h"
#include <array>
#include <deque>
#include <teya/animation/AnimationPlayer.h>
namespace teya::editor {
struct PreviewEventRecord {
    teya::animation::TriggeredAnimationEvent event;
    float previewTime = 0;
};
class AnimationEditorPanel final : public Panel {
  public:
    void draw(EditorHost &, EditorContext &) override;
    bool openAsset(EditorHost &, std::uint64_t);
    bool &timelineOpen() { return timelineOpen_; }
    [[nodiscard]] bool active() const { return active_; }
    void drawInspectorPanel(EditorHost &, bool &open);

  private:
    void load(EditorHost &, std::uint64_t);
    void validate(EditorHost &);
    void save(EditorHost &);
    void reload(EditorHost &);
    void applyTemporary(EditorHost &);
    void syncPreview();
    void shortcuts(EditorHost &);
    void assetBar(EditorHost &);
    void clips();
    void preview(EditorHost &);
    void inspector(EditorHost &);
    void actionBindings();
    void frameSourcePicker();
    void attachmentObjects(EditorHost &);
    void spriteSheetPicker();
    void atlasPicker();
    void timeline();
    void frameCollections(EditorHost &, teya::animation::AnimationFrame &);
    bool loaded_ = false, active_ = false, timelineOpen_ = true, playing_ = false, fit_ = false,
         showGrid_ = true, showSockets_ = true, onionPrevious_ = false, onionNext_ = false;
    float zoom_ = 4, opacity_ = .25f;
    Vector2 pan_{};
    AnimationDocument document_;
    teya::animation::AnimationPlayer previewPlayer_;
    std::uint64_t previewRevision_ = ~std::uint64_t{0};
    Texture2D texture_{};
    int textureWidth_ = 0, textureHeight_ = 0;
    teya::animation::AnimationValidationResult validation_;
    std::string message_;
    std::deque<PreviewEventRecord> eventLog_;
    std::vector<EditableAnimationAssetInfo> assets_;
    std::vector<int> sheetSelection_;
    int sheetSelectionAnchor_ = -1;
    float sheetPickerZoom_ = 1.0f;
    float importedFrameDuration_ = 0.1f;
    std::array<char, 96> atlasSearch_{};
    std::vector<AttachmentPreviewInfo> attachmentObjects_;
    std::uint64_t attachmentAssetId_ = 0;
    std::size_t selectedAttachment_ = 0;
    bool editAttachmentEffectTip_ = false;
    std::string attachmentMessage_;
    bool socketDragActive_ = false, socketDragChanged_ = false;
    std::size_t socketDragClip_ = 0, socketDragFrame_ = 0, socketDragIndex_ = 0;
    Vector2 socketDragOffset_{};
    teya::animation::AnimationAsset socketDragBefore_;
    bool markerDragActive_ = false, markerDragChanged_ = false;
    std::size_t markerDragClip_ = 0, markerDragFrame_ = 0, markerDragIndex_ = 0;
    Vector2 markerDragOffset_{};
    teya::animation::AnimationAsset markerDragBefore_;
    bool hitboxDragActive_ = false, hitboxDragChanged_ = false;
    bool hitboxResizeX_ = false, hitboxResizeY_ = false;
    std::size_t hitboxDragClip_ = 0, hitboxDragFrame_ = 0, hitboxDragIndex_ = 0;
    Vector2 hitboxDragOffset_{};
    teya::animation::AnimationAsset hitboxDragBefore_;
};
} // namespace teya::editor
