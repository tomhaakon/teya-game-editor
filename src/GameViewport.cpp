#include "teya/editor/GameViewport.h"
#include <algorithm>
#include <cmath>
namespace teya::editor { float GameViewport::scale(int cw,int ch,float aw,float ah,bool fit){if(cw<=0||ch<=0||aw<=0||ah<=0)return 0;float s=std::min(aw/cw,ah/ch);return fit||s<1?s:std::floor(s);} ViewportRect GameViewport::centered(int cw,int ch,float x,float y,float aw,float ah,bool fit){float s=scale(cw,ch,aw,ah,fit),w=cw*s,h=ch*s;return{x+(aw-w)*.5f,y+(ah-h)*.5f,w,h};} std::optional<ViewportPoint> GameViewport::screenToCanvas(ViewportPoint p,const ViewportRect&r,int cw,int ch){if(cw<=0||ch<=0||r.width<=0||r.height<=0||p.x<r.x||p.y<r.y||p.x>=r.x+r.width||p.y>=r.y+r.height)return std::nullopt;return ViewportPoint{(p.x-r.x)*cw/r.width,(p.y-r.y)*ch/r.height};} }
