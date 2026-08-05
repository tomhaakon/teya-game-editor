#pragma once
#include <optional>
namespace teya::editor {
struct ViewportPoint { float x=0, y=0; };
struct ViewportRect { float x=0, y=0, width=0, height=0; };
class GameViewport {
public:
 static float scale(int canvasWidth,int canvasHeight,float availableWidth,float availableHeight,bool fit);
 static ViewportRect centered(int canvasWidth,int canvasHeight,float availableX,float availableY,float availableWidth,float availableHeight,bool fit);
 static std::optional<ViewportPoint> screenToCanvas(ViewportPoint screen,const ViewportRect& image,int canvasWidth,int canvasHeight);
};
}
