#include "teya/editor/AnimationDocument.h"
#include "teya/editor/EditorContext.h"
#include "teya/editor/GameViewport.h"
#include <cassert>
#include <cmath>
#include <iostream>

#undef assert
#define assert(condition) do { if (!(condition)) { std::cerr << "check failed at line " << __LINE__ << ": " #condition << '\n'; return 1; } } while (false)

using namespace teya::editor;

static teya::animation::AnimationAsset sampleAsset() {
    teya::animation::AnimationAsset asset;
    asset.texturePath = "x.png";
    asset.frameWidth = asset.frameHeight = 16;
    asset.sheetColumns = 4;
    teya::animation::AnimationFrame idleFrame;
    idleFrame.durationSeconds = .1f;
    teya::animation::AnimationFrame attackFrame;
    attackFrame.source.spriteIndex = 1;
    attackFrame.durationSeconds = .2f;
    attackFrame.sockets.push_back({"hand", {1.25f, 2.5f}, 15, {1, 1}, true,
                                   teya::animation::AttachmentLayer::InFrontOfOwner});
    teya::animation::AnimationClip idle;
    idle.name = "idle";
    idle.frames.push_back(idleFrame);
    teya::animation::AnimationClip attack;
    attack.name = "attack";
    attack.looping = false;
    attack.frames.push_back(attackFrame);
    asset.clips = {idle, attack};
    return asset;
}

int main() {
    assert(GameViewport::scale(320, 180, 1000, 700, false) == 3);
    assert(std::abs(GameViewport::scale(320, 180, 500, 500, true) - 1.5625f) < .001f);
    auto viewport = GameViewport::centered(320, 180, 0, 0, 1000, 700, false);
    assert(viewport.x == 20 && viewport.y == 80 && viewport.width == 960 && viewport.height == 540);
    assert(!GameViewport::screenToCanvas({0, 0}, viewport, 320, 180));

    EditorSelection selection;
    selection.select(2);
    assert(selection.reconcile({{2, 0, "x"}}));
    assert(!selection.reconcile({{3, 0, "y"}}) && selection.selected() == 0);
    EditorContext context;
    context.pause(); context.step();
    assert(context.consumeSimulationStep());
    context.play();
    assert(context.consumeSimulationStep());

    auto asset = sampleAsset();
    AnimationDocument document;
    document.load(asset, 7);
    assert(!document.dirty() && document.assetId() == 7);
    assert(document.duplicateFrame() && document.asset().clips[0].frames.size() == 2);
    assert(document.undo() && document.asset().clips[0].frames.size() == 1);
    assert(document.redo() && document.asset().clips[0].frames.size() == 2);
    document.selection().frame = 1;
    assert(document.moveFrame(-1));
    assert(document.undo());
    document.selection().clip = 1;
    document.selection().frame = 0;
    document.selection().kind = AnimationSelection::Kind::Socket;
    document.copySelection();
    document.selection().clip = 0;
    assert(document.paste());
    document.propagateSocket(0, 0, document.asset().clips[0].frames.size() - 1);
    assert(document.asset().clips[0].frames[1].sockets.size() == 1);
    assert(document.duplicateClip());
    assert(document.deleteClip());
    document.markTemporaryApplied();
    assert(document.temporaryApplied());
    document.markSaved();
    assert(!document.dirty());

    assert(snapAnimationValue(1.24f, true, .5f) == 1.0f);
    assert(std::abs(snapAnimationValue(1.24f, false, .5f) - 1.24f) < .001f);
    auto local = animationPreviewScreenToLocal({120, 80}, {100, 50}, {10, 10}, 2, false, 16);
    assert(local.x == 5 && local.y == 10);
    auto mirrored = animationPreviewScreenToLocal({120, 80}, {100, 50}, {10, 10}, 2, true, 16);
    assert(mirrored.x == 11 && mirrored.y == 10);
    assert(animationPreviewZoom(true, true, 3.7f, 3.7f) == 3);
    assert(std::abs(animationPreviewZoom(false, false, 1.25f, 4) - 1.25f) < .001f);

    AnimationUndoHistory bounded(2);
    auto changed = asset;
    bounded.record(changed);
    changed.texturePath = "1"; bounded.record(changed);
    changed.texturePath = "2";
    assert(bounded.undo(changed) && changed.texturePath == "1");
    assert(bounded.undo(changed) && changed.texturePath == "x.png");
    assert(!bounded.undo(changed));
    bounded.redo(changed);
    bounded.record(changed);
    assert(!bounded.canRedo());
}
