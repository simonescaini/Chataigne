#pragma once
#include "PackageHost.h"
namespace midnightShow {
class PackageInspector final:public ControllableContainer {
public:
 Cue& cue;bool ready=false;std::map<Parameter*,String> fields;std::map<EnumParameter*,String> aliasFields;
 explicit PackageInspector(Cue& c):ControllableContainer("Custom Behaviour package"),cue(c){
  validatePackage(c.package);auto manifest=c.package["manifest"];auto* info=addStringParameter("Package identity","Pinned bytes are embedded in the show",manifest["id"].toString()+" @ "+manifest["version"].toString()+" | SHA256 "+packageHash(c.package));info->setControllableFeedbackOnly(true);info->isSavable=false;
  auto params=packageParameters(c.package,c.parameters);if(auto* values=manifest["parameters"].getArray())for(auto field:*values){auto id=field["id"].toString(),type=field["type"].toString();Parameter* p=nullptr;auto value=params[Identifier(id)];if(type=="number")p=addFloatParameter(id,"Package number parameter",float(double(value)),field.hasProperty("min")?float(double(field["min"])):-std::numeric_limits<float>::max(),field.hasProperty("max")?float(double(field["max"])):std::numeric_limits<float>::max());else if(type=="bool")p=addBoolParameter(id,"Package Boolean parameter",bool(value));else p=addStringParameter(id,"Package text parameter",value.toString());fields[p]=id;}
  std::vector<std::shared_ptr<Cue>> cues;std::function<void(const std::vector<std::shared_ptr<Cue>>&)> visit=[&](const auto& items){for(auto item:items){cues.push_back(item);visit(item->children);}};visit(coordinator()->cues);
  for(auto key:{"inputs","commands"})if(auto* aliases=manifest[key].getArray())for(auto alias:*aliases){auto id=alias["alias"].toString();auto* choice=addEnumParameter(id+" target","Resolved exactly at Start; missing references remain broken");choice->addOption("Unbound","");bool input=String(key)=="inputs";
   if(input)for(auto m:service()->snapshot().document.motors)choice->addOption("Encoder: "+String(m.name),"motor:"+String(m.id));
   else for(auto target:cues)if(target->actions.size()==1)choice->addOption("Cue: "+target->title->stringValue(),"cue:"+String(target->id));
   if(input||alias["operation"].toString()=="setValue")for(auto weak:Engine::mainEngine->getAllControllables(true))if(auto* p=dynamic_cast<Parameter*>(weak.get()))if(input||!p->isControllableFeedbackOnly)choice->addOption("Parameter: "+p->getControlAddress(),"parameter:"+p->getControlAddress());
   auto ref=c.aliases[Identifier(id)];auto kind=ref["kind"].toString();auto value=kind+":"+(kind=="parameter"?ref["address"].toString():ref["id"].toString());if(!choice->setValueWithData(value)&&kind.isNotEmpty())choice->addOption("MISSING: "+value,value);choice->setValueWithData(value);aliasFields[choice]=id;
  }ready=true;
 }
 void onContainerParameterChanged(Parameter* p)override{if(!ready)return;if(fields.count(p)){if(!cue.parameters.isObject())cue.parameters=var(new DynamicObject());cue.parameters.getDynamicObject()->setProperty(Identifier(fields[p]),p->getValue());}
  if(auto* choice=dynamic_cast<EnumParameter*>(p))if(aliasFields.count(choice)){auto value=choice->getValueData().toString();auto kind=value.upToFirstOccurrenceOf(":",false,false),id=value.fromFirstOccurrenceOf(":",false,false);var ref(new DynamicObject());ref.getDynamicObject()->setProperty("kind",kind);ref.getDynamicObject()->setProperty(kind=="parameter"?"address":"id",id);if(!cue.aliases.isObject())cue.aliases=var(new DynamicObject());cue.aliases.getDynamicObject()->setProperty(Identifier(aliasFields[choice]),ref);}
  if(Engine::mainEngine)Engine::mainEngine->changed();
 }
};
}
