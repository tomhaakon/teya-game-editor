#pragma once
namespace teya::editor {
struct EditorSettings {
    bool fitGameView = true;
    bool captureGameInput = false;
    bool showPlayerOrigin = false;
    bool showPlayerCollider = false;
    bool showWorldBounds = false;
    bool showAnimationHitboxes = false;
    bool showAnimationSockets = false;
    bool showAnimationMarkers = false;
};
} // namespace teya::editor
