#pragma once
#include "teya/editor/panels/Panel.h"
#include <cstdint>
#include <deque>
#include <string>

namespace teya::editor {
class ConsolePanel final : public Panel {
public:
    void draw(EditorHost&, EditorContext&) override;
private:
    void poll(EditorHost&);
    void append(std::string line);
    std::string path_, partial_, error_;
    std::deque<std::string> lines_;
    std::uintmax_t offset_ = 0;
    double nextPoll_ = 0.0;
    bool paused_ = false, autoScroll_ = true, scrollToBottom_ = true;
};
}
