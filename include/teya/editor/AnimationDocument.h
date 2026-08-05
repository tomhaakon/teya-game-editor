#pragma once
#include <teya/animation/AnimationAsset.h>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>
namespace teya::editor {
struct AnimationSelection { std::size_t clip=0,frame=0,item=0; enum class Kind{Frame,Socket,Event,Hitbox,Marker} kind=Kind::Frame; };
class AnimationUndoHistory {
public:
    explicit AnimationUndoHistory(std::size_t capacity=128):capacity_(capacity){}
    void clear(); void record(const teya::animation::AnimationAsset& before); bool undo(teya::animation::AnimationAsset& current); bool redo(teya::animation::AnimationAsset& current); bool canUndo()const; bool canRedo()const;
private: std::size_t capacity_; std::vector<teya::animation::AnimationAsset> undo_,redo_;
};
using AnimationClipboard=std::variant<std::monostate,teya::animation::AnimationClip,teya::animation::AnimationFrame,teya::animation::AnimationSocket,teya::animation::AnimationEvent,teya::animation::AnimationHitbox,teya::animation::AnimationMarker>;
class AnimationDocument {
public:
    void load(teya::animation::AnimationAsset asset,std::uint64_t assetId=0); void markSaved(); bool dirty()const; bool temporaryApplied()const; void markTemporaryApplied();
    teya::animation::AnimationAsset& asset(); const teya::animation::AnimationAsset& asset()const; AnimationSelection& selection(); const AnimationSelection& selection()const;
    void mutate(const teya::animation::AnimationAsset& before); bool undo(); bool redo(); void repairSelection();
    bool addClip(std::string name); bool duplicateClip(); bool deleteClip(); bool addFrame(); bool duplicateFrame(); bool deleteFrame(); bool moveFrame(int direction);
    void copySelection(); bool paste();
    void propagateSocket(std::size_t socketIndex,std::size_t begin,std::size_t end); void propagateHitbox(std::size_t index,std::size_t begin,std::size_t end); void propagateMarker(std::size_t index,std::size_t begin,std::size_t end);
    std::uint64_t assetId()const; std::uint64_t revision()const; AnimationUndoHistory& history();
private: teya::animation::AnimationAsset asset_; AnimationSelection selection_; AnimationUndoHistory history_; AnimationClipboard clipboard_; std::uint64_t assetId_=0,revision_=0; bool dirty_=false,temporary_=false;
};
float snapAnimationValue(float value,bool enabled,float increment) noexcept;
Vector2 animationPreviewScreenToLocal(Vector2 screen,Vector2 previewOrigin,Vector2 pan,float zoom,bool facingLeft,float frameWidth) noexcept;
float animationPreviewZoom(bool pixelArt,bool preferInteger,float requested,float fit) noexcept;
}
