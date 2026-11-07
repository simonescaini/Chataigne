#pragma once
#include "MotorChoice.h"

// Owns only per-use authoring data. No serial connection and no execution lifecycle.
class MidnightBehaviour final : public ControllableContainer, private Timer {
public:
    explicit MidnightBehaviour(midnightNative::Kind);
    midnightNative::Kind kind;
    MidnightMotorChoice *motor,*master=nullptr;
    FloatParameter *startAngle=nullptr,*target=nullptr,*duration=nullptr,*nominalSpeed=nullptr,*driveSpeed=nullptr,*currentLimit=nullptr;
    FloatParameter *stiffness=nullptr,*damping=nullptr,*torqueLimit=nullptr,*deadband=nullptr,*offset=nullptr;
    BoolParameter *playCurve=nullptr,*keepActive=nullptr,*align=nullptr,*reverseMaster=nullptr,*reverseFollower=nullptr;
    std::unique_ptr<Automation> curve;
    ControllableContainer followers{"Followers"};
    std::map<midnightNative::Id,BoolParameter*> followerChoices;
    FloatParameter *requested,*measured;
    StringParameter* explanation;
    midnightNative::Event makeEvent(midnightNative::Event base={}) const;
    void readEvent(const midnightNative::Event&);
    void stretchDuration(double);
    void refresh();
    void afterLoadJSONDataInternal() override { ScopedValueSetter<bool> guard(syncing,true); synchronizeEndpoints(); refresh(); }
    void onContainerParameterChanged(Parameter*) override;
    void onControllableFeedbackUpdate(ControllableContainer*,Controllable*) override;
private:
    bool syncing=true;
    void synchronizeEndpoints();
    void timerCallback() override { refresh(); }
};
