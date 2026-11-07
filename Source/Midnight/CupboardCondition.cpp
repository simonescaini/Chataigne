#include "Common/Processor/ProcessorIncludes.h"
#include "CupboardCondition.h"
using namespace midnightNative;
CupboardButtonCondition::CupboardButtonCondition(var params,Multiplex* multiplex)
    :Condition(getTypeStringStatic(),params,multiplex) {
    motorChoice=new MidnightMotorChoice("Cupboard");addControllable(motorChoice);
    referenceMode=addEnumParameter("Reference position","Captured only once per activation");
    referenceMode->addOption("Specified absolute angle",0)->addOption("Capture measured angle on activation",1);
    referenceAngle=addFloatParameter("Reference angle / degrees","Authored absolute reference",20);
    triggerOffset=addFloatParameter("Signed trigger offset / degrees","20 degrees with -5 triggers at 15; nonzero", -5);
    response=addEnumParameter("Physical response","Free sends an actual disable; return uses supported torque control");
    response->addOption("Free",0)->addOption("Return toward reference",1);
    stiffness=addFloatParameter("Spring strength / Nm per rad","Supported RobStride torque command",1,0);
    damping=addFloatParameter("Damping / Nm per rad per second","Velocity damping",.05f,0);
    maximumEffort=addFloatParameter("Maximum commanded torque / Nm","Protocol torque request, not calibrated physical torque",.5f,0);
    rearm=addBoolParameter("Rearm","Return to the starting side clears TRUE and prepares another press",true);
    activateTrigger=addTrigger("Activate","Capture reference and start response. Also activated by the owning Action/state entering activity.");
    deactivateTrigger=addTrigger("Deactivate","Explicitly end this button and release its own response");
    resetTrigger=addTrigger("Reset","Clear the latch without recapturing reference or ending response");
    liveReference=addFloatParameter("Live reference / degrees","Reference captured for this activation",0);
    measuredAngle=addFloatParameter("Measured angle / degrees","Encoder feedback",0);
    activeFeedback=addBoolParameter("Button active","Runtime lifecycle state",false);
    pressedFeedback=addBoolParameter("Button state","Latched threshold state",false);
    runtimeStatus=addStringParameter("Runtime status","Execution feedback","");
    for(auto* p:Array<Parameter*>{liveReference,measuredAngle,activeFeedback,pressedFeedback,runtimeStatus}){p->setControllableFeedbackOnly(true);p->isSavable=false;}
    constructed=true;startTimerHz(50);
}
CupboardButtonCondition::~CupboardButtonCondition(){stopTimer();deactivate();}
void CupboardButtonCondition::clearItem(){deactivate();Condition::clearItem();}
Event CupboardButtonCondition::definition() const {
    Event e;e.id=interactionId;e.name=niceName.toStdString();e.kind=Kind::Button;e.outputs={motorChoice->motorId()};
    auto& b=e.button;b.reference=referenceAngle->doubleValue();b.offset=triggerOffset->doubleValue();
    b.capture=int(referenceMode->getValueData())==1;b.spring=int(response->getValueData())==1;b.rearm=rearm->boolValue();
    b.stiffness=stiffness->doubleValue();b.damping=damping->doubleValue();b.effort=maximumEffort->doubleValue();return e;
}
void CupboardButtonCondition::activate(){
    if(!constructed||!enabled->boolValue()||forceDisabled||!service())return;
    for(ControllableContainer* cc=this;cc;cc=cc->parentContainer.get())if(cc->isCurrentlyLoadingData)return;
    if(Engine::mainEngine&&(Engine::mainEngine->isLoadingFile||Engine::mainEngine->isClearing))return;
    try{require(getMultiplexCount()==1,"Cupboard buttons currently require a non-multiplexed Action");auto e=definition();service()->launch(e);activatedMotor=e.outputs[0];seenPresses=0;setValid(0,false);clearWarning();}
    catch(const std::exception& e){setWarningMessage(e.what());}
}
void CupboardButtonCondition::deactivate(){
    if(auto* shared=service())shared->end(interactionId);
    if(constructed){seenPresses=0;activeFeedback->setValue(false);pressedFeedback->setValue(false);for(int i=0;i<getMultiplexCount();++i)setValid(i,false);}
}
void CupboardButtonCondition::reset(){
    if(auto* shared=service())shared->action([id=interactionId](Runtime& r){if(r.execution.count(id))r.resetButton(id);});
    updateFeedback();
}
void CupboardButtonCondition::setForceDisabled(bool value,bool force){
    const bool was=forceDisabled;Condition::setForceDisabled(value,force);
    if(value)deactivate();else if(was||force)activate();
}
void CupboardButtonCondition::onContainerParameterChangedInternal(Parameter* p){
    Condition::onContainerParameterChangedInternal(p);
    if(!constructed)return;
    if(p==enabled){if(enabled->boolValue())activate();else deactivate();}
    // Editing per-use settings is inert. They take effect at the next deliberate activation.
}
void CupboardButtonCondition::onContainerTriggerTriggered(Trigger* t){
    if(t==activateTrigger)activate();else if(t==deactivateTrigger)deactivate();else if(t==resetTrigger)reset();
}
void CupboardButtonCondition::updateFeedback(){
    auto* shared=service();if(!shared||!constructed)return;
    auto s=shared->snapshot();auto motor=s.motors.find(s.events.count(interactionId)?activatedMotor:motorChoice->motorId());
    if(motor!=s.motors.end()&&motor->second.at>=0)measuredAngle->setValue(motor->second.position);
    auto found=s.events.find(interactionId);bool active=found!=s.events.end()&&found->second.status==Status::Running;
    activeFeedback->setValue(active);
    if(found==s.events.end()){runtimeStatus->setValue("Inactive");return;}
    auto state=found->second;liveReference->setValue(state.buttonReference);
    runtimeStatus->setValue(String(statusName(state.status))+(s.error.empty()?"":" | "+String(s.error)));
    if(!active){pressedFeedback->setValue(false);setValid(0,false);return;}
    // Preserve presses observed by the worker even when the UI/message thread was busy.
    WeakReference<ControllableContainer> alive(this);
    while(seenPresses<state.presses){
        ++seenPresses;
        if(getIsValid())setValid(0,false);
        if(alive==nullptr)return;
        setValid(0,true);
        if(alive==nullptr)return;
        // A consequence may explicitly deactivate/delete the condition's runtime.
        if(!shared->snapshot().events.count(interactionId))return;
    }
    pressedFeedback->setValue(state.buttonValid);setValid(0,state.buttonValid);
}
