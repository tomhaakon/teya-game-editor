#pragma once
#include "teya/editor/EditorSelection.h"
#include "teya/editor/EditorSettings.h"
namespace teya::editor {
enum class SimulationMode { Stopped, Playing, Paused, StepRequested };
struct InputGateState { bool gameViewFocused=false, captureEnabled=false, wantKeyboard=false, wantMouse=false, modalOpen=false, keyboardRequired=true, mouseRequired=true; SimulationMode simulation=SimulationMode::Stopped; };
class EditorContext {
public:
    EditorSelection selection; EditorSettings settings;
    SimulationMode simulationMode() const; void stop(); void play(); void pause(); void step();
    bool consumeSimulationStep();
    static bool allowsGameInput(const InputGateState& state);
private: SimulationMode mode_ = SimulationMode::Stopped;
};
}
