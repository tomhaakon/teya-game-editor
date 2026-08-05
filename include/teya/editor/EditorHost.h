#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <raylib.h>

namespace teya::editor {
using RuntimeObjectId = std::uint64_t;
struct RuntimeNode { RuntimeObjectId id = 0; RuntimeObjectId parentId = 0; std::string name; };
struct RuntimeProperty { std::string name; std::string value; };
struct EditorFrameMetrics {
    float fps = 0.0f, frameMilliseconds = 0.0f, updateMilliseconds = 0.0f;
    float drawMilliseconds = 0.0f, editorMilliseconds = 0.0f;
    int canvasWidth = 0, canvasHeight = 0, imageWidth = 0, imageHeight = 0;
    float gameViewScale = 0.0f;
    std::string currentState, currentMap;
    int colliderCount = -1;
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
    virtual void requestExit() = 0;
};
}
