#pragma once
#include "NativePanel.h"

// A view of the engine-owned registry, never a second motor controller.
class MidnightModule : public Module, private Timer {
public:
    MidnightModule();
    ~MidnightModule() override { stopTimer(); }
    static MidnightModule* create() { return new MidnightModule(); }
    String getDefaultTypeString() const override { return "Midnight"; }
    void refreshValues();
    void afterLoadJSONDataInternal() override;
    InspectableEditor* getEditorInternal(bool, Array<Inspectable*> = {}) override;
private:
    void timerCallback() override { refreshValues(); }
};
