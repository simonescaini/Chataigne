#pragma once
#include "BehaviourDefinition.h"
class MidnightCommand final : public BaseCommand, private Timer {
public:
    enum Operation {Move,Link,Haptic,End,ActivateButton,DeactivateButton,ResetButton};
    MidnightCommand(Module*,CommandContext,var,Multiplex*);
    ~MidnightCommand() override;
    static BaseCommand* create(ControllableContainer* m,CommandContext c,var p,Multiplex* x){return new MidnightCommand(static_cast<Module*>(m),c,p,x);}
    Operation operation;
    std::unique_ptr<MidnightBehaviour> behaviour;
    TargetParameter* target=nullptr;
    BoolParameter* mappingActive=nullptr;
    void triggerInternal(int) override;
    void endInteractions();
    const midnightNative::Id instanceId=midnightNative::uuid();
    std::set<int> instances;
    StringParameter* runtimeFeedback;
    void timerCallback() override;
};
