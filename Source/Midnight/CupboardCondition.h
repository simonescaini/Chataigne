#pragma once
#include "MotorChoice.h"
#include "Common/Processor/Action/Condition/Condition.h"

class CupboardButtonCondition final : public Condition, private Timer {
public:
    CupboardButtonCondition(var params=var(),Multiplex* multiplex=nullptr);
    ~CupboardButtonCondition() override;
    static String getTypeStringStatic(){return "Cupboard button";}
    String getTypeString() const override{return getTypeStringStatic();}
    MidnightMotorChoice* motorChoice;
    EnumParameter* referenceMode;
    FloatParameter *referenceAngle,*triggerOffset,*stiffness,*damping,*maximumEffort;
    EnumParameter* response;
    BoolParameter* rearm;
    Trigger *activateTrigger,*deactivateTrigger,*resetTrigger;
    FloatParameter *liveReference,*measuredAngle;
    BoolParameter *activeFeedback,*pressedFeedback;
    StringParameter* runtimeStatus;
    const midnightNative::Id interactionId=midnightNative::uuid();
    void activate();
    void deactivate();
    void reset();
    void forceCheck() override { updateFeedback(); } // Evaluation never activates or configures the drive.
    void setForceDisabled(bool value,bool force=false) override;
    void onContainerParameterChangedInternal(Parameter*) override;
    void onContainerTriggerTriggered(Trigger*) override;
    void clearItem() override;
    midnightNative::Event definition() const;
    void updateFeedback();
private:
    uint64_t seenPresses=0;
    midnightNative::Id activatedMotor;
    bool constructed=false;
    void timerCallback() override { updateFeedback(); }
};
