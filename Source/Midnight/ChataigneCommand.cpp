#include "Module/ModuleIncludes.h"
#include "Common/Processor/ProcessorIncludes.h"
#include "ChataigneCommand.h"
using namespace midnightNative;
MidnightCommand::MidnightCommand(Module* m,CommandContext context,var params,Multiplex* multiplex)
    :BaseCommand(m,context,params,multiplex),operation(Operation(int(params.getProperty("operation",0)))){
    runtimeFeedback=addStringParameter("Interaction status","Runtime instance addressed by End interaction","Idle");runtimeFeedback->setControllableFeedbackOnly(true);runtimeFeedback->isSavable=false;startTimerHz(5);
    if(operation<=Haptic){
        behaviour=std::make_unique<MidnightBehaviour>(operation==Move?Kind::Move:operation==Link?Kind::Link:Kind::Haptic);
        addChildControllableContainer(behaviour.get());saveAndLoadRecursiveData=true;
        if(context==CommandContext::MAPPING){
            if(operation==Move){behaviour->playCurve->setValue(false);behaviour->playCurve->setEnabled(false);BaseCommand::onControllableAdded(behaviour->target);linkParamToMappingIndex(behaviour->target,0);}
            else {mappingActive=addBoolParameter("Active","Mapping TRUE starts this interaction; FALSE explicitly ends this instance",false);linkParamToMappingIndex(mappingActive,0);}
        }
    }else{
        target=addTargetParameter(operation==End?"Running command":"Cupboard button","Select the exact command or condition instance");
        target->targetType=TargetParameter::CONTAINER;target->maxDefaultSearchLevel=-1;
        target->defaultContainerTypeCheckFunc=[this](ControllableContainer* cc){
            if(operation==End){auto* handler=dynamic_cast<BaseCommandHandler*>(cc);return handler&&dynamic_cast<MidnightCommand*>(handler->command.get());}
            return dynamic_cast<CupboardButtonCondition*>(cc)!=nullptr;
        };
    }
}
MidnightCommand::~MidnightCommand(){stopTimer();endInteractions();}
void MidnightCommand::timerCallback(){
    if(auto* shared=service()){auto snapshot=shared->snapshot();StringArray states;for(int i:instances){auto found=snapshot.events.find(instanceId+"/"+std::to_string(i));if(found!=snapshot.events.end())states.add(String(statusName(found->second.status)));}runtimeFeedback->setValue(states.isEmpty()?String("Idle"):states.joinIntoString(", ")+(snapshot.error.empty()?"":" | "+String(snapshot.error)));}
}
void MidnightCommand::endInteractions(){
    if(auto* shared=service())for(int i:instances)shared->end(instanceId+"/"+std::to_string(i));
    instances.clear();
}
void MidnightCommand::triggerInternal(int index){
    auto* shared=service();if(!shared)return;
    try{
        if(operation==End){
            auto* handler=target->getTargetContainerAs<BaseCommandHandler>();
            auto* command=handler?dynamic_cast<MidnightCommand*>(handler->command.get()):nullptr;
            require(command&&command!=this,"Select the intended Midnight Start/Move consequence or Mapping output");command->endInteractions();
        }else if(operation>=ActivateButton){
            auto* button=target->getTargetContainerAs<CupboardButtonCondition>();require(button,"Select a Cupboard button condition");
            if(operation==ActivateButton)button->activate();else if(operation==DeactivateButton)button->deactivate();else button->reset();
        }else{
            const Id id=instanceId+"/"+std::to_string(index);
            if(mappingActive){
                bool active=bool(getLinkedValue(mappingActive,index));
                if(!active){shared->end(id);instances.erase(index);return;}
                auto snapshot=shared->snapshot();auto found=snapshot.events.find(id);
                if(found!=snapshot.events.end()&&found->second.status==Status::Running)return;
            }
            Event e=behaviour->makeEvent();e.id=id;e.name=niceName.toStdString();
            if(context==CommandContext::MAPPING&&operation==Move){e.target=double(getLinkedValue(behaviour->target,index));e.curveData.clear();e.curve={};e.mappingStream=true;}
            shared->launch(e);instances.insert(index);
        }
        clearWarning();
    }catch(const std::exception& e){setWarningMessage(e.what());}
}
