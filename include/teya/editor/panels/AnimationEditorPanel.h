#pragma once
#include "teya/editor/AnimationDocument.h"
#include "teya/editor/EditorHost.h"
#include "teya/editor/panels/Panel.h"
#include <teya/animation/AnimationPlayer.h>
#include <array>
#include <deque>
namespace teya::editor {
struct PreviewEventRecord { teya::animation::TriggeredAnimationEvent event; float previewTime=0; };
class AnimationEditorPanel final:public Panel {
public: void draw(EditorHost&,EditorContext&) override;
private:
 void load(EditorHost&,std::uint64_t); void validate(EditorHost&); void save(EditorHost&); void reload(EditorHost&); void applyTemporary(EditorHost&); void syncPreview(); void shortcuts(EditorHost&);
 void assetBar(EditorHost&); void clips(); void preview(EditorHost&); void inspector(EditorHost&); void timeline(); void frameCollections(EditorHost&,teya::animation::AnimationFrame&);
 bool loaded_=false,facingLeft_=false,playing_=false,fit_=true,showGrid_=true,onionPrevious_=false,onionNext_=false; float zoom_=4,opacity_=.25f; Vector2 pan_{};
 AnimationDocument document_; teya::animation::AnimationPlayer previewPlayer_; std::uint64_t previewRevision_=~std::uint64_t{0}; Texture2D texture_{}; int textureWidth_=0,textureHeight_=0;
 teya::animation::AnimationValidationResult validation_; std::string message_; std::deque<PreviewEventRecord> eventLog_; std::vector<EditableAnimationAssetInfo> assets_;
};
}
