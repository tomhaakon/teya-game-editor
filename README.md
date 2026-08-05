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

A future standalone executable can implement `EditorHost` over IPC/shared texture
transport without changing panels. Validated edit commands can later be added to
the host interface; phase 1 is deliberately read-only and does not edit animation.
