#include "MappingAdapter.h"
#include "Common/Processor/ProcessorIncludes.h"
#include "Midnight/ChataigneCommand.h"
namespace midnightShow {
std::vector<Mapping*> nativeMappings(){std::vector<Mapping*> out;for(auto cc:Engine::mainEngine->getAllContainers(true))if(auto* mapping=dynamic_cast<Mapping*>(cc.get()))out.push_back(mapping);return out;}
Mapping& authorMapping(Cue& c){if(!c.mappingEditor){c.mappingEditor=std::make_shared<Mapping>();c.mappingEditor->om.admission=std::make_shared<OutputAdmission>();c.mappingEditor->setForceDisabled(true,true);if(c.mapping.isObject())c.mappingEditor->loadJSONData(c.mapping);}return *c.mappingEditor;}
var mappingData(const Cue& c){return c.mappingEditor?c.mappingEditor->getJSONData():c.mapping;}
MappingInstance::MappingInstance(const var& definition,uint64_t gen):gate(std::make_shared<OutputAdmission>()),generation(gen){
 require(definition.isObject(),"Mapping definition missing");gate->generation=gen;instance=std::make_shared<Mapping>();instance->om.admission=gate;instance->om.admissionGeneration=gen;instance->setForceDisabled(true,true);instance->loadJSONData(definition);
 require(instance->im.items.size()>0&&instance->om.items.size()>0,"Mapping needs an input and output");for(auto* input:instance->im.items)require(input->inputReference!=nullptr,"Mapping input target missing");
 for(auto* output:instance->om.items){output->allowAutomaticTargetRebind=false;require(output->command!=nullptr,"Mapping output command target missing");if(auto* motor=dynamic_cast<MidnightCommand*>(output->command.get()))require(motor->operation<=MidnightCommand::Haptic,"Cue-owned motor Mapping must create its own Move, Link or Haptic interaction");}
 instance->sendAfterLoad->setValue(false);instance->sendOnActivate->setValue(false);instance->enabled->setValue(true);
}
MappingInstance::~MappingInstance(){cease(true);}
void MappingInstance::activate(){require(!processing,"Mapping already active");{std::lock_guard<std::recursive_mutex> lock(gate->mutex);gate->open=true;}processing=true;instance->setForceDisabled(false,true);instance->process(true,-1,true);}
void MappingInstance::cease(bool cleanup){gate->close();if(instance){instance->setForceDisabled(true,true);processing=false;if(cleanup)for(auto* output:instance->om.items)if(auto* motor=dynamic_cast<MidnightCommand*>(output->command.get()))motor->endInteractions();}}
bool MappingInstance::owns()const{if(processing)return true;if(!service()||!instance)return false;auto s=service()->snapshot();for(auto* output:instance->om.items)if(auto* motor=dynamic_cast<MidnightCommand*>(output->command.get()))for(int index:motor->instances){auto id=motor->instanceId+"/"+std::to_string(index);for(auto p:s.motors)if(p.second.owner==id)return true;}return false;}
}
