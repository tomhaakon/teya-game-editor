#pragma once
namespace teya::editor { class EditorHost; class EditorContext; class Panel { public: virtual ~Panel()=default; virtual void draw(EditorHost&,EditorContext&)=0; bool open=true; }; }
