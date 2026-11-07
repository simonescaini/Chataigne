#include "MainIncludes.h"
#include "BehaviourDefinition.h"
using namespace midnightNative;
MidnightBehaviour::MidnightBehaviour(Kind type):ControllableContainer("Behaviour"),kind(type) {
    saveAndLoadRecursiveData=true;editorIsCollapsed=false;
    motor=new MidnightMotorChoice(type==Kind::Haptic?"Motor B":"Motor");addControllable(motor);
    if(type==Kind::Move){
        startAngle=addFloatParameter("Authored start / degrees","Absolute first requested angle; never replaced by measured position",0);
        target=addFloatParameter("Absolute target / degrees","Authored endpoint or Mapping input",45);
        nominalSpeed=addFloatParameter("Nominal speed / degrees per second","Endpoint distance divided by duration; not instantaneous curve speed or a drive cap",30,0);
        duration=addFloatParameter("Duration / seconds","Stretch time while retaining angular shape and custom points",1.5f,.01f);
        driveSpeed=addFloatParameter("Drive speed limit / degrees per second","Independent hardware position-control cap; steep curve segments may be physically limited",30,0);
        currentLimit=addFloatParameter("Maximum current / A","RobStride current-limit parameter, not calibrated maximum torque",3,0);
        playCurve=addBoolParameter("Play absolute angle curve","Clock starts immediately; first target is the authored start, even if measured position differs",true);
        keepActive=addBoolParameter("Keep active after arrival","Retain final position control; otherwise disable after clock end and measured arrival",true);
        curve=std::make_unique<Automation>("Absolute position over time");
        curve->valueRange->setEnabled(false);curve->setLength(1.5f);
        curve->addKey(0,0)->easingType->setValueWithData(Easing::LINEAR);curve->addKey(1.5f,45)->easingType->setValueWithData(Easing::LINEAR);
        curve->viewValueRange->setPoint(0,50);addChildControllableContainer(curve.get());
    }else{
        master=new MidnightMotorChoice(type==Kind::Haptic?"Motor A":"Master");addControllable(master);
        if(type==Kind::Link){
            motor->hideInEditor=true;addChildControllableContainer(&followers);followers.saveAndLoadRecursiveData=true;
        }
        deadband=addFloatParameter("Deadband / degrees","Existing native interaction deadband",.2f,0);
        if(type==Kind::Haptic){
            stiffness=addFloatParameter("Stiffness / Nm per rad","Existing two-motor spring law",1,0);
            damping=addFloatParameter("Damping / Nm per rad per second","Existing two-motor velocity damping",.05f,0);
            torqueLimit=addFloatParameter("Maximum commanded torque / Nm","Model-specific protocol torque request",.5f,0);
            align=addBoolParameter("Align on activation","Capture relative geometry; does not change mechanical zero",true);
            offset=addFloatParameter("Explicit offset / degrees","Used when alignment capture is disabled",0);
            reverseMaster=addBoolParameter("Reverse A","Invert coupling axis A",false);
            reverseFollower=addBoolParameter("Reverse B","Invert coupling axis B",false);
        }
    }
    requested=addFloatParameter("Requested angle / degrees","Requested position remains distinct from encoder feedback",0);
    measured=addFloatParameter("Measured angle / degrees","Shared encoder feedback",0);
    explanation=addStringParameter("Playback details","Current model and timing semantics","");
    for(auto* p:Array<Parameter*>{requested,measured,explanation}){p->setControllableFeedbackOnly(true);p->isSavable=false;}
    syncing=false;refresh();startTimerHz(5);
}
void MidnightBehaviour::stretchDuration(double seconds){
    if(!curve)return;
    const double old=curve->length->doubleValue();require(seconds>=.01,"Duration must be at least .01 seconds");
    struct Handle {CubicEasing* easing;Point<float> a,b;};
    std::vector<Handle> handles;
    for(auto* k:curve->items)if(auto* cubic=dynamic_cast<CubicEasing*>(k->easing.get()))handles.push_back({cubic,cubic->anchor1->getPoint(),cubic->anchor2->getPoint()});
    curve->setLength(float(seconds),true);
    for(auto& h:handles){h.easing->anchor1->setPoint(float(h.a.x*seconds/old),h.a.y);h.easing->anchor2->setPoint(float(h.b.x*seconds/old),h.b.y);}
    duration->setValue(seconds);
}
void MidnightBehaviour::synchronizeEndpoints(){
    if(!curve||curve->items.size()<2)return;
    curve->items[0]->position->setValue(0);
    curve->items.getLast()->position->setValue(duration->doubleValue());
    startAngle->setValue(curve->items[0]->value->doubleValue());
    target->setValue(curve->items.getLast()->value->doubleValue());
    const auto distance=std::abs(target->doubleValue()-startAngle->doubleValue());
    nominalSpeed->setValue(distance/duration->doubleValue());
    nominalSpeed->setEnabled(distance>1.e-8);
}
void MidnightBehaviour::onContainerParameterChanged(Parameter* p){
    if(syncing||isCurrentlyLoadingData||!curve)return;
    ScopedValueSetter<bool> guard(syncing,true);
    if(p==duration)stretchDuration(duration->doubleValue());
    if(p==nominalSpeed&&nominalSpeed->doubleValue()>0){
        const auto distance=std::abs(target->doubleValue()-startAngle->doubleValue());
        if(distance>1.e-8)stretchDuration(distance/nominalSpeed->doubleValue());
    }
    if(p==startAngle||p==target){
        if(curve->items.size()>=2){
            curve->items[0]->value->setValue(startAngle->doubleValue());
            curve->items.getLast()->value->setValue(target->doubleValue());
        }
    }
    synchronizeEndpoints();
}
void MidnightBehaviour::onControllableFeedbackUpdate(ControllableContainer* cc,Controllable*){
    if(syncing||isCurrentlyLoadingData||!curve)return;
    if(cc==curve.get()||ControllableUtil::findParentAs<Automation>(cc)==curve.get()){
        ScopedValueSetter<bool> guard(syncing,true);synchronizeEndpoints();
    }
}
void MidnightBehaviour::refresh(){
    auto* shared=service();if(!shared)return;
    const auto snapshot=shared->snapshot();
    if(kind==Kind::Link)for(const auto& m:snapshot.document.motors){
        auto found=followerChoices.find(m.id);
        if(found==followerChoices.end()){
            auto* p=followers.addBoolParameter(String(m.name),"Follower motor: "+String(m.id),false);
            p->setCustomShortName("motor"+String(m.id).removeCharacters("-"));followerChoices[m.id]=p;
        }else if(found->second->niceName!=String(m.name))found->second->setNiceName(String(m.name));
    }
    auto f=snapshot.motors.find(motor->motorId());
    if(f!=snapshot.motors.end()){requested->setValue(f->second.requested);if(f->second.at>=0)measured->setValue(f->second.position);}
    if(kind==Kind::Move){
        bool rmd=false;for(const auto& m:snapshot.document.motors)if(m.id==motor->motorId())rmd=Driver{m}.rmd();
        currentLimit->setEnabled(!rmd);
        String note=rmd?"MyActuator: maximum effort control is unavailable. Speed cap is whole degrees/s; zero means uncapped. ":"RobStride: effort is current limit in A. ";
        note+="Nominal speed is endpoint distance / duration, including for curves with reversals. The independent drive speed cap can limit physical following. ";
        if(std::abs(target->doubleValue()-startAngle->doubleValue())<1.e-8)note+="Nominal speed is N/A (displayed 0); explicit duration is retained. ";
        note+="Linear, Bezier and Hold segments; endpoint times are 0 and Duration. Clock end precedes measured arrival if resisted.";
        explanation->setValue(note);
    }else explanation->setValue(kind==Kind::Link?"One master / one or more followers. Uses each follower's saved speed/current. Move ranges do not limit Link. End explicitly through show logic.":"Two supported RobStride motors only. Move ranges do not limit Haptic. End explicitly through show logic.");
}
Event MidnightBehaviour::makeEvent(Event e) const {
    e.kind=kind;e.input.clear();e.outputs={motor->motorId()};
    if(kind==Kind::Move){
        e.target=target->doubleValue();e.speed=driveSpeed->doubleValue();e.nominalSpeed=nominalSpeed->doubleValue();e.currentLimit=currentLimit->enabled?currentLimit->doubleValue():-1;
        e.keepActive=keepActive->boolValue();e.curveData=playCurve->boolValue()?JSON::toString(curve->getJSONData()).toStdString():"";
        if(playCurve->boolValue())e.duration=duration->doubleValue();
    }else{
        e.input=master->motorId();e.deadband=deadband->doubleValue();
        if(kind==Kind::Link){e.outputs.clear();for(const auto& f:followerChoices)if(f.second->boolValue())e.outputs.push_back(f.first);}
        else {e.stiffness=stiffness->doubleValue();e.damping=damping->doubleValue();e.torqueLimit=torqueLimit->doubleValue();e.align=align->boolValue();e.offset=offset->doubleValue();e.signA=reverseMaster->boolValue()?-1:1;e.signB=reverseFollower->boolValue()?-1:1;}
    }
    return e;
}
void MidnightBehaviour::readEvent(const Event& e){
    ScopedValueSetter<bool> guard(syncing,true);refresh();
    motor->select(e.outputs.empty()?Id{}:e.outputs[0]);if(master)master->select(e.input);
    if(kind==Kind::Move){
        target->setValue(e.target);driveSpeed->setValue(e.speed);nominalSpeed->setValue(e.nominalSpeed);currentLimit->setValue(e.currentLimit<0?3:e.currentLimit);keepActive->setValue(e.keepActive);playCurve->setValue(!e.curveData.empty());
        if(!e.curveData.empty()){curve->loadJSONData(JSON::fromString(String(e.curveData)));duration->setValue(e.duration);synchronizeEndpoints();}
        else {const double length=std::max(.01,std::abs(e.target)/std::max(.01,e.nominalSpeed));curve->setLength(float(length),true);curve->items.getLast()->value->setValue(e.target);duration->setValue(length);synchronizeEndpoints();}
    }else{
        deadband->setValue(e.deadband);
        if(kind==Kind::Link)for(auto& f:followerChoices)f.second->setValue(std::find(e.outputs.begin(),e.outputs.end(),f.first)!=e.outputs.end());
        else {stiffness->setValue(e.stiffness);damping->setValue(e.damping);torqueLimit->setValue(e.torqueLimit);align->setValue(e.align);offset->setValue(e.offset);reverseMaster->setValue(e.signA<0);reverseFollower->setValue(e.signB<0);}
    }
}
