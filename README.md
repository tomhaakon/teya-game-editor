# Teya Game Editor

Experimental C++17 in-process development editor for Teya games. It provides the
`Teya::GameEditor` CMake target and depends upward on `Teya::Core`, `Teya::2D`,
raylib, pinned Dear ImGui docking, and rlImGui. Core and 2D never depend on it.

The game owns its window, simulation, fixed-resolution `PixelCanvas`, and runtime
objects. An `EditorHost` supplies immutable hierarchy/property/metrics snapshots,
stable IDs, a render-texture handle, and an exit callback. No game headers or
mutable game pointers cross the boundary.

Initialize after the window, canvas, and game resources. Each frame, let editor
simulation state decide whether one game update runs, draw the game into the
canvas, then call `Editor::draw()` inside the raylib drawing pass. Destroy the
editor before canvas/GPU resources and the window.

Panels derive from `Panel`, own only presentation state, and communicate through
`EditorContext` and `EditorHost`. Add a header/source pair, store the panel in
`Editor`, expose it in View, and dock it in `defaultLayout()`.

`GameViewport` calculates integer or fit scaling, centers the image, and converts
screen positions only when inside the image rectangle; letterbox positions are
rejected. This pure logic is covered by tests.

## Animation authoring

Open **View > Animation Editor** to edit the immutable `teya-2d` animation model.
The host lists assets and returns an independent deep working copy plus a borrowed
preview texture. Gameplay keeps its last validated immutable asset until Save or
the explicitly temporary Apply Without Saving action succeeds.

The panel provides an asset bar, clip list, playback preview, metadata inspector,
validation navigation, and a horizontally scrolling frame timeline. Clips and
frames can be added, duplicated, deleted, and reordered. Frame source, duration,
sockets, attachment layers, events, hitboxes, and markers use the runtime types;
the internal typed clipboard always makes deep copies. Undo/redo stores bounded
asset snapshots, is cleared on asset load, and never retains vector references.

Pixel Art mode defaults to nearest filtering, whole-pixel position snapping,
integer preview zoom, optional pixel grid, and configurable owner/attachment
rounding. Smooth mode defaults to linear filtering, arbitrary zoom and unrounded
floating-point transforms. Changing mode retains authored decimal values. The
same snapping helpers handle position, rotation and scale; left-facing display
uses `teya-2d` mirroring and saved coordinates remain canonical right-facing.

Grid sources expose dimensions, columns, sprite index, row/column and calculated
source bounds. Atlas assets expose the runtime atlas-region list and deterministic
region IDs. A missing texture disables only the visual preview, not metadata work.
Preview playback uses a private `AnimationPlayer`; its fixed-capacity event log is
editor-local and manual scrubbing emits no gameplay events.

Save validates through the host, serializes through `teya-2d`, writes the normal
asset path, then atomically replaces the runtime asset. Validation or write failure
preserves both working and live data. Reload first asks before discarding dirty
work and only installs a successfully loaded copy. Keyboard shortcuts are active
only while the panel owns focus and no text field or modal is active: Space,
arrows, Home/End, Ctrl+S, Ctrl+R, Ctrl+Z, Ctrl+Y/Ctrl+Shift+Z, Ctrl+D, F and G.

Hosts implement the animation methods on `EditorHost`: enumerate assets, load a
working copy and texture metadata, validate, save/apply, optionally temporary
apply, supply attachment previews, and provide event/marker suggestions. Texture
handles are borrowed unless explicitly marked owned and must remain valid until
the next load or editor shutdown.

`Attachment Objects...` manages reusable game-provided objects such as swords,
axes, fishing rods, and lanterns. Each object has a texture, hand pivot, default
socket, transform offsets, and visibility. Click the texture in
the pivot preview at the point that should sit on the socket. The animation
preview renders the object through the authored socket transform and clip
mirroring. The socket's per-frame layer decides whether it is behind or in front
of the owner. Persistence and live application remain host-owned, so the editor
does not hard-code game item paths or formats.
