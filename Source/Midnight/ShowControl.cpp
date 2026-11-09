#include "ShowControl.h"
#include "MappingAdapter.h"
#include "PackageHost.h"
#include "SongTransport.h"
namespace midnightShow {
namespace {
var object(){return var(new DynamicObject());}
var copy(const var& v){return JSON::parse(JSON::toString(v));}
std::shared_ptr<Cue> findIn(const std::vector<std::shared_ptr<Cue>>& list,const Id& id){for(auto c:list){if(c->id==id)return c;if(auto found=findIn(c->children,id))return found;}return {};}
std::vector<std::shared_ptr<Cue>>* containing(std::vector<std::shared_ptr<Cue>>& list,const Id& id){for(auto c:list){if(c->id==id)return &list;if(auto* found=containing(c->children,id))return found;}return nullptr;}
}
const char* cueTypeName(CueType t){switch(t){case CueType::Motion:return "Movement / interaction";case CueType::Reaction:return "Reaction routine";case CueType::Step:return "Reaction step";case CueType::Mapping:return "Persistent Mapping";case CueType::Package:return "Custom Behaviour";case CueType::Group:return "Group / music scene";default:return "End cue";}}
const char* outcomeName(Outcome o){switch(o){case Outcome::Pending:return "Pending";case Outcome::Succeeded:return "Succeeded";case Outcome::Failed:return "Failed";case Outcome::Cancelled:return "Ended / cancelled";default:return "Superseded before success";}}
Cue::Cue(CueType t):ControllableContainer("Cue"),type(t){
 title=addStringParameter("Name","Display name; references use stable IDs",cueTypeName(t));
 pre=addFloatParameter("Placement / pre-wait (s)","Timeline: one song placement. Else: delay before native launch.",0,0);
 post=addFloatParameter("Post-wait (s)","Auto-continue: from actual action start. Auto-follow: after successful finite completion.",0,0);
 continuation=addEnumParameter("Continuation","Failure and End never count as success");continuation->addOption("Manual GO",0)->addOption("Auto-continue from action start",1)->addOption("Auto-follow finite success",2);
 repeat=addEnumParameter("Repeated Start","A deliberate restart reaches normal native takeover; Ignore is optional");repeat->addOption("Restart",0)->addOption("Ignore while pending / active",1);
 eligible=addBoolParameter("Eligible","Included in future group starts",true);
 target=addStringParameter("End target ID","Exact cue ID, never rebound by name","");target->hideInEditor=true;
 sourceMapping=addStringParameter("Linked Mapping ID","Blank means cue-owned definition","");sourceMapping->hideInEditor=true;
 cleanup=addBoolParameter("End owned interactions","Mapping: exact motor cleanup. Off retains owned work for later End.",true);cleanup->hideInEditor=t!=CueType::Mapping;
 song=addStringParameter("Song asset","Relative show asset path","");song->hideInEditor=t!=CueType::Group;
 loopStart=addFloatParameter("Loop region start (s)","Primary song region",0,0);loopEnd=addFloatParameter("Loop region end (s)","Zero uses the song end",0,0);loopCount=addIntParameter("Song iterations","1 plays once; 0 repeats until explicit End",1,0,10000);for(auto* p:std::initializer_list<Controllable*>{loopStart,loopEnd,loopCount})p->hideInEditor=t!=CueType::Group;if(t==CueType::Group)markers=std::make_shared<TimeCueManager>();
 groupMode=addEnumParameter("Group mode","Native finite-success Playlist semantics");groupMode->addOption("Timeline",0)->addOption("Start First",1)->addOption("Start First and Enter",2)->addOption("Playlist",3)->addOption("Random",4);groupMode->hideInEditor=t!=CueType::Group;
 wait=addBoolParameter("Wait for reactions","Advance only after all movements have finite successful outcomes",false);
 deactivate=addBoolParameter("Deactivate after press","Otherwise keep Free/Return response active until owning routine End",false);
 live=addBoolParameter("Live while song paused","Explicitly live reaction route",false);
 inputMotor=new MidnightMotorChoice("Input cupboard");addControllable(inputMotor);
 reference=addFloatParameter("Reference angle / degrees","Authored absolute reference",20);
 capture=addBoolParameter("Capture reference on activation","Once per input session, never on navigation",false);
 offset=addFloatParameter("Signed trigger offset / degrees","Native signed crossing and rearm",-5);
 spring=addBoolParameter("Return response","Off is native Free; on uses supported native torque response",false);
 rearm=addBoolParameter("Rearm input","Requires a return to starting side before another crossing",true);
 stiffness=addFloatParameter("Spring strength / Nm per rad","Native button setting",1,0);
 damping=addFloatParameter("Damping / Nm per rad per second","Native button setting",.05f,0);
 effort=addFloatParameter("Maximum torque / Nm","Protocol request, not calibrated physical torque",.5f,0);
 for(auto* c:std::initializer_list<Controllable*>{wait,deactivate,live,inputMotor,reference,capture,offset,spring,rearm,stiffness,damping,effort})c->hideInEditor=t!=CueType::Step;
 if(t==CueType::Motion||t==CueType::Step)addMotion();
 if(t==CueType::Mapping)repeat->setValueWithData(1);
}
std::shared_ptr<MidnightBehaviour> Cue::addMotion(Kind kind){auto b=std::make_shared<MidnightBehaviour>(kind);actions.push_back(b);return b;}
void Cue::onContainerParameterChanged(Parameter*){if(!isCurrentlyLoadingData&&Engine::mainEngine)Engine::mainEngine->changed();}
var Cue::save()const {
 auto v=object();auto* o=v.getDynamicObject();o->setProperty("id",String(id));o->setProperty("type",int(type));o->setProperty("settings",const_cast<Cue*>(this)->getJSONData());o->setProperty("recording",String(recording));
 Array<var> aa;for(auto a:actions){auto v=object();v.getDynamicObject()->setProperty("kind",int(a->kind));v.getDynamicObject()->setProperty("data",a->getJSONData());aa.add(v);}o->setProperty("actions",aa);
 Array<var> cc;for(auto c:children)cc.add(c->save());o->setProperty("children",cc);o->setProperty("media",media);if(markers)o->setProperty("markers",markers->getJSONData());o->setProperty("mapping",mappingData(*this));o->setProperty("package",package);o->setProperty("parameters",parameters);o->setProperty("aliases",aliases);return v;
}
std::shared_ptr<Cue> Cue::load(const var& v){int type=int(v["type"]);require(type>=0&&type<=int(CueType::End),"Unknown cue type");auto c=std::make_shared<Cue>(CueType(type));c->id=v["id"].toString().toStdString();require(!c->id.empty(),"Cue ID missing");c->loadJSONData(v["settings"]);c->recording=v["recording"].toString().toStdString();c->actions.clear();
 if(auto* aa=v["actions"].getArray())for(auto a:*aa){int k=int(a["kind"]);require(k>=0&&k<=int(Kind::Haptic),"Unsupported authored motor behaviour");c->addMotion(Kind(k))->loadJSONData(a["data"]);}
 if(auto* cc=v["children"].getArray())for(auto child:*cc)c->children.push_back(load(child));c->media=copy(v["media"]);if(c->markers)c->markers->loadJSONData(v["markers"]);c->mapping=copy(v["mapping"]);c->package=copy(v["package"]);c->parameters=copy(v["parameters"]);c->aliases=copy(v["aliases"]);return c;
}
std::shared_ptr<const CueSpec> Cue::snapshot()const {
 auto s=std::make_shared<CueSpec>();s->id=id;s->name=title->stringValue();s->type=type;s->mode=GroupMode(int(groupMode->getValueData()));s->pre=pre->doubleValue();s->post=post->doubleValue();s->continuation=int(continuation->getValueData());s->ignore=int(repeat->getValueData())==1;s->eligible=eligible->boolValue();s->wait=wait->boolValue();s->deactivate=deactivate->boolValue();s->live=live->boolValue();s->cleanup=cleanup->boolValue();s->target=target->stringValue().toStdString();s->recording=recording;s->sourceMapping=sourceMapping->stringValue().toStdString();s->song=song->stringValue();s->media=copy(media);s->loopStart=loopStart->doubleValue();s->loopEnd=loopEnd->doubleValue();s->loopCount=loopCount->intValue();if(markers)s->markers=copy(markers->getJSONData());
 for(auto a:actions)s->actions.push_back(a->makeEvent());for(auto c:children)s->children.push_back(c->snapshot());
 s->input.kind=Kind::Button;s->input.outputs={inputMotor->motorId()};s->input.button={reference->doubleValue(),offset->doubleValue(),stiffness->doubleValue(),damping->doubleValue(),effort->doubleValue(),capture->boolValue(),spring->boolValue(),rearm->boolValue()};
 s->mapping=copy(mappingData(*this));
 if(type==CueType::Mapping&&!s->sourceMapping.empty()){Mapping* source=nullptr;for(auto* candidate:nativeMappings())if(candidate->showDefinitionId==String(s->sourceMapping)){if(source!=nullptr)s->bindingError="Duplicate linked Mapping ID; select an unambiguous original";source=candidate;}if(source==nullptr)s->bindingError="Linked Mapping definition missing; no name fallback";else s->mapping=copy(source->getJSONData());}s->package=copy(package);s->parameters=copy(parameters);s->aliases=copy(aliases);return s;
}
Coordinator::Coordinator(bool automatic){songFactory=[](const CueSpec& cue,uint64_t epoch){return std::make_shared<SongTransport>(cue,epoch);};if(automatic)startTimer(10);}
Coordinator::~Coordinator(){stopTimer();clear();}
Coordinator* coordinator(){auto* e=dynamic_cast<ChataigneEngine*>(Engine::mainEngine);return e?e->showControl.get():nullptr;}
void Coordinator::note(const String& s){log.push_back(String(monotonic(),3)+" | "+s);while(log.size()>1000)log.pop_front();++revision;}
std::shared_ptr<Cue> Coordinator::find(const Id& id)const{return findIn(cues,id);}
std::vector<std::shared_ptr<Cue>>* Coordinator::list(const Id& parent){if(parent.empty())return &cues;auto c=find(parent);return c?&c->children:nullptr;}
void Coordinator::remember(){undoStates.push_back(copy(save()));if(undoStates.size()>50)undoStates.erase(undoStates.begin());}
void Coordinator::undo(){if(undoStates.empty())return;auto state=undoStates.back();undoStates.pop_back();load(state,false);note("Undo restored definitions only; existing run snapshots unchanged");}
std::shared_ptr<Cue> Coordinator::add(CueType type,const Id& parent){auto* items=list(parent);require(items!=nullptr,"Missing parent");remember();auto c=std::make_shared<Cue>(type);items->push_back(c);selected=c->id;if(standby[parent].empty())standby[parent]=c->id;++revision;if(Engine::mainEngine)Engine::mainEngine->changed();return c;}
void Coordinator::erase(const Id& id){auto* items=containing(cues,id);if(!items)return;remember();items->erase(std::remove_if(items->begin(),items->end(),[&](auto c){return c->id==id;}),items->end());if(selected==id)selected.clear();++revision;note("Definition deleted; active snapshots and exact End remain available");}
Id Coordinator::duplicate(const Id& id){auto source=find(id);require(bool(source),"Select a cue to duplicate");remember();auto c=Cue::load(copy(source->save()));std::map<Id,Id> ids;
 std::function<void(std::shared_ptr<Cue>)> allocate=[&](auto n){auto old=n->id;n->id=uuid();ids[old]=n->id;for(auto k:n->children)allocate(k);};allocate(c);
 std::function<void(std::shared_ptr<Cue>)> remap=[&](auto n){auto t=n->target->stringValue().toStdString();if(ids.count(t))n->target->setValue(String(ids[t]));if(auto* aliases=n->aliases.getDynamicObject())for(auto pair:aliases->getProperties()){auto ref=pair.value;auto id=ref["id"].toString().toStdString();if(ref["kind"].toString()=="cue"&&ids.count(id))ref.getDynamicObject()->setProperty("id",String(ids[id]));}for(auto k:n->children)remap(k);};remap(c);c->title->setValue(c->title->stringValue()+" copy");auto* items=containing(cues,id);items->push_back(c);selected=c->id;++revision;note("Copied subtree in two passes; external targets remain explicit");return c->id;
}
void Coordinator::reorder(const Id& id,int delta){auto* items=containing(cues,id);if(!items)return;auto i=std::find_if(items->begin(),items->end(),[&](auto c){return c->id==id;});int index=int(i-items->begin()),other=index+delta;if(other<0||other>=int(items->size()))return;remember();std::swap((*items)[index],(*items)[other]);++revision;}
void Coordinator::goTo(const Id& id){require(bool(find(id)),"Missing standby target");auto* l=containing(cues,id);Id parent;std::function<void(const std::vector<std::shared_ptr<Cue>>&)> visit=[&](const auto& items){for(auto c:items){if(&c->children==l)parent=c->id;visit(c->children);}};visit(cues);standby[parent]=id;listFocus=parent;++revision;}
Id Coordinator::go(){
 auto* items=list(listFocus);require(items&&!items->empty(),"No cues in this list");
 if(!standby.count(listFocus))standby[listFocus]=items->front()->id;
 if(standby[listFocus].empty()){
  require(!listFocus.empty(),"End of cue list; select a cue and use GoTo");auto* ownerList=containing(cues,listFocus);Id parent;
  std::function<void(const std::vector<std::shared_ptr<Cue>>&)> visit=[&](const auto& list){for(auto c:list){if(&c->children==ownerList)parent=c->id;visit(c->children);}};visit(cues);listFocus=parent;return go();
 }
 Id id=standby[listFocus];auto i=std::find_if(items->begin(),items->end(),[&](auto c){return c->id==id;});require(i!=items->end(),"Standby target is missing; use GoTo");auto c=*i;Id run;std::shared_ptr<Run> enterOwner;
 for(auto p:runs)if(p.second->admitted&&p.second->cue->id==listFocus&&p.second->cue->type==CueType::Group&&p.second->cue->mode==GroupMode::Enter)if(!enterOwner||p.second->generation>enterOwner->generation)enterOwner=p.second;
 if(enterOwner){std::shared_ptr<const CueSpec> snapshot;for(auto candidate:enterOwner->cue->children)if(candidate->id==id)snapshot=candidate;require(bool(snapshot),"This child was added after the active Group snapshot; Restart to include it");run=start(snapshot,enterOwner->id,enterOwner->clockRoot);tick();}else run=direct(id);
 while(i!=items->end()&&int((*i)->continuation->getValueData())!=0)++i;if(i!=items->end())++i;standby[listFocus]=i==items->end()?Id{}:(*i)->id;
 if(enterOwner){enterOwner->nextChild=size_t(i-items->begin());if(i==items->end())enterOwner->scheduleClosed=true;}
 if(c->type==CueType::Group&&GroupMode(int(c->groupMode->getValueData()))==GroupMode::Enter&&!c->children.empty()){
  listFocus=c->id;size_t next=0;while(next<c->children.size()&&int(c->children[next]->continuation->getValueData())!=0)++next;++next;standby[listFocus]=next<c->children.size()?c->children[next]->id:Id{};if(runs.count(run)){runs[run]->nextChild=next;if(next>=c->children.size())runs[run]->scheduleClosed=true;}
 }
 ++revision;return run;
}
Id Coordinator::direct(const Id& id,bool restart,bool preview){auto cue=find(id);require(bool(cue),"Missing cue definition");if(!restart&&cue->snapshot()->ignore)for(auto& p:runs)if(p.second->cue->id==id&&p.second->admitted&&p.second->outcome==Outcome::Pending){note("Repeat ignored by authored choice: "+cue->title->stringValue());return p.first;}
 // A new movement reaches native takeover; a restart closes only older runs of this definition.
 std::vector<Id> previous;for(auto& p:runs)if(p.second->cue->id==id&&p.second->parent.empty()&&p.second->admitted)previous.push_back(p.first);for(auto old:previous){auto r=runs[old];if(cue->type==CueType::Motion||cue->type==CueType::Step){r->admitted=false;r->continuationSent=true;if(!r->started)finish(r,Outcome::Cancelled,"Pending start replaced");}else end(old);}
 auto run=start(cue->snapshot(),{},{},preview,preview);tick();return run;
}
std::shared_ptr<const CueSpec> Coordinator::freezeBindings(std::shared_ptr<const CueSpec> source){if(source->bindingsFrozen)return source;auto s=std::make_shared<CueSpec>(*source);s->bindingsFrozen=true;s->children.clear();for(auto child:source->children)s->children.push_back(freezeBindings(child));if(s->type==CueType::Mapping){try{MappingInstance binding(s->mapping,0);s->mappingIdentity=binding.identity;}catch(const std::exception& e){s->bindingError=e.what();}}if(s->type==CueType::Package){if(auto* refs=s->aliases.getDynamicObject())for(auto pair:refs->getProperties()){auto ref=pair.value;if(ref["kind"].toString()=="cue"){auto definition=find(ref["id"].toString().toStdString());if(definition&&definition->actions.size()==1)s->packageCommands[pair.name.toString()]=definition->actions[0]->makeEvent();}else if(ref["kind"].toString()=="parameter")s->packageTargets[pair.name.toString()]=dynamic_cast<Parameter*>(Engine::mainEngine->getControllableForAddress(ref["address"].toString()));}}return s;}
Id Coordinator::start(std::shared_ptr<const CueSpec> cue,Id parent,Id clockRoot,bool bypassWait,bool suppressContinuation){
 require(bool(cue),"Missing cue snapshot");cue=freezeBindings(cue);auto r=std::make_shared<Run>();r->cue=cue;r->generation=++serial;r->parent=parent;r->clockRoot=clockRoot;r->acceptedAt=monotonic();r->due=clock(*r,r->acceptedAt)+(bypassWait?0:cue->pre);r->continuationSent=suppressContinuation;
 if(!parent.empty()&&runs.count(parent)){auto p=runs[parent];p->children.push_back(r->id);auto& siblings=p->cue->children;for(size_t i=0;i+1<siblings.size();++i)if(siblings[i]->id==cue->id)r->successor=siblings[i+1];}
 else if(auto* siblings=containing(cues,cue->id)){for(size_t i=0;i+1<siblings->size();++i)if((*siblings)[i]->id==cue->id)r->successor=(*siblings)[i+1]->snapshot();}
 runs[r->id]=r;note("Run accepted "+cue->name+" ["+String(r->id)+"] generation "+String(int64(r->generation))+"; native launch pending");return r->id;
}
bool Coordinator::isFinite(const CueSpec& c)const {if(c.type==CueType::Mapping)return false;if(c.type==CueType::Package)return c.package["manifest"]["lifetime"].toString()=="finite";if(c.type==CueType::Motion||c.type==CueType::Step){for(auto e:c.actions)if(e.kind!=Kind::Move)return false;}return true;}
bool Coordinator::owns(const Run& r)const{for(auto& m:r.members)if(m.held)return true;for(auto& p:r.inputs)if(service()->observe(p.second).held)return true;for(auto id:r.children){auto i=runs.find(id);if(i!=runs.end()&&owns(*i->second))return true;}return (r.mapping&&r.mapping->owns())||(r.package&&r.package->owns());}
double Coordinator::clock(const Run& r,double now)const {if(!r.clockRoot.empty()){auto i=runs.find(r.clockRoot);if(i!=runs.end())return i->second->songPosition;}return now;}
bool Coordinator::schedulingAllowed(const Run& r)const{if(r.clockRoot.empty())return true;auto i=runs.find(r.clockRoot);return i!=runs.end()&&i->second->admitted&&i->second->clockValid&&!i->second->paused;}
void Coordinator::finish(const std::shared_ptr<Run>& r,Outcome outcome,const String& why){if(r->outcome!=Outcome::Pending)return;r->outcome=outcome;r->outcomeAt=clock(*r,monotonic());r->detail=why;note(r->cue->name+": "+outcomeName(outcome)+" | "+why);}
void Coordinator::launchMembers(const std::shared_ptr<Run>& r,bool retryOnly){
 if(!retryOnly){r->members.clear();for(const auto& e:r->cue->actions){Member m;m.authored=e;r->members.push_back(m);}}
 bool failed=false;for(size_t i=0;i<r->members.size();++i){auto& m=r->members[i];if(retryOnly&&m.receipt.accepted&&m.outcome!=Outcome::Failed&&m.outcome!=Outcome::Superseded)continue;auto event=m.authored;event.id="show/"+r->id+"/"+std::to_string(i);event.name=r->cue->name.toStdString();m.receipt=service()->tryLaunch(event);m.outcome=m.receipt.accepted?Outcome::Pending:Outcome::Failed;m.detail=m.receipt.reason;failed|=!m.receipt.accepted;
  note(r->cue->name+" member "+String(int(i+1))+(m.receipt.accepted?": native launch accepted":": rejected "+String(m.receipt.reason)));
 }
 if(failed)finish(r,Outcome::Failed,"Partial batch: accepted movements continue; Retry starts failed members only");
}
void Coordinator::launch(const std::shared_ptr<Run>& r,double now){if(!r->admitted||r->started)return;r->started=true;r->startedAt=clock(*r,now);r->late=jmax(0.,r->startedAt-r->due);r->detail="Action started";note(r->cue->name+": dispatch lateness "+String(r->late,4)+" s");
 try {switch(r->cue->type){
 case CueType::Motion:case CueType::Step:launchMembers(r);break;
 case CueType::Reaction:{
  for(auto step:r->cue->children){require(step->type==CueType::Step,"Reaction routines contain steps");auto e=step->input;require(!e.outputs.empty()&&!e.outputs[0].empty(),"Step input cupboard is missing");auto motor=e.outputs[0];if(r->inputs.count(motor))continue;e.id="show/"+r->id+"/input/"+motor;e.name="Reaction input "+motor;auto receipt=service()->tryLaunch(e);require(receipt.accepted,receipt.reason);r->inputs[motor]=receipt.handle;}
  armStep(r);break;}
 case CueType::End:{require(!r->cue->target.empty(),"End target missing");std::shared_ptr<Run> target;
  // Resolve the newest matching run in this owning subtree; never sweep other instances.
  auto belongs=[&](std::shared_ptr<Run> candidate){if(r->parent.empty())return candidate->parent.empty();Id parent=candidate->parent;while(!parent.empty()&&runs.count(parent)){if(parent==r->parent)return true;parent=runs[parent]->parent;}return false;};
  for(auto p:runs)if(p.first!=r->id&&p.second->cue->id==r->cue->target&&(p.second->admitted||owns(*p.second))&&belongs(p.second))if(!target||p.second->generation>target->generation)target=p.second;
  require(bool(target),"End target has no active run in this ownership scope; use Active Cues for another run");end(target->id,target->generation);finish(r,Outcome::Succeeded,"Exact scoped target run ended");break;}
 case CueType::Group:startGroup(r,now);break;
 case CueType::Mapping:require(r->cue->bindingError.isEmpty(),r->cue->bindingError.toStdString());r->mapping=std::make_shared<MappingInstance>(r->cue->mapping,r->generation);require(r->cue->mappingIdentity&&r->cue->mappingIdentity->matches(*r->mapping->identity),"Mapping target identity changed since acceptance; Restart after fixing the target");r->mapping->activate();r->detail="Persistent private Mapping instance; original enabled state unchanged";break;
 case CueType::Package:r->package=std::make_shared<PackageSession>(*this,r);r->detail="Package worker started; hash "+r->package->hash.substring(0,12);break;
 }}catch(const std::exception& e){finish(r,Outcome::Failed,e.what());}
}
void Coordinator::refresh(const std::shared_ptr<Run>& r,double now){
 if(r->mapping&&r->mapping->processing&&!r->mapping->identity->valid()){r->mapping->cease(true);finish(r,Outcome::Failed,"Mapping target deleted; no name fallback");}
 if(r->package)r->package->update(*this,r,now);
 bool all=!r->members.empty(),failed=false;for(auto& m:r->members){if(!m.receipt.accepted){all=false;failed=true;continue;}auto o=service()->observe(m.receipt.handle);m.held=o.current&&o.held;m.clockEnded=o.current&&o.execution.clockEnded;
  if(m.outcome==Outcome::Pending){if(o.succeeded)m.outcome=Outcome::Succeeded;else if(!o.current||o.execution.status==Status::Superseded)m.outcome=Outcome::Superseded;else if(o.execution.status==Status::Failed)m.outcome=Outcome::Failed;else if(o.execution.status==Status::Stopped)m.outcome=Outcome::Cancelled;}
  all&=m.outcome==Outcome::Succeeded;failed|=m.outcome==Outcome::Failed||m.outcome==Outcome::Superseded||m.outcome==Outcome::Cancelled;
  if(m.clockEnded&&m.outcome==Outcome::Pending)r->detail="Clock ended; awaiting fresh measured arrival";
 }
 if(failed)finish(r,Outcome::Failed,"One or more members failed, ended or were superseded before success");else if(all)finish(r,Outcome::Succeeded,"Finite native success; retained holding is tracked separately");
 if(r->cue->type==CueType::Reaction&&r->started){bool required=true;for(auto id:r->children){auto c=runs[id];if(isFinite(*c->cue))required&=c->outcome==Outcome::Succeeded;}if(r->scheduleClosed&&!required){bool failed=false;for(auto id:r->children)failed|=runs[id]->outcome==Outcome::Failed;if(failed)finish(r,Outcome::Failed,"Routes consumed; an earlier required reaction did not succeed");}if(r->scheduleClosed&&required)finish(r,Outcome::Succeeded,"All reaction steps consumed and finite movements succeeded; inputs remain response-active");}
 if(r->cue->type==CueType::Group&&r->started)groupTick(r,now);
 continueRun(r,now);
}
std::shared_ptr<const CueSpec> Coordinator::nextSpec(const Run& r)const{return r.successor;}
void Coordinator::continueRun(const std::shared_ptr<Run>& r,double now){if(!r->admitted||r->continuationSent||!r->started||!schedulingAllowed(*r)||r->cue->continuation==0)return;if(r->outcome==Outcome::Failed||r->outcome==Outcome::Cancelled||r->outcome==Outcome::Superseded)return;
 bool due=r->cue->continuation==1?clock(*r,now)>=r->startedAt+r->cue->post:r->outcome==Outcome::Succeeded;
 if(!due)return;if(r->cue->continuation==2&&clock(*r,now)<r->outcomeAt+r->cue->post)return;
 r->continuationSent=true;if(auto n=nextSpec(*r))start(n,r->parent,r->clockRoot);}
void Coordinator::armStep(const std::shared_ptr<Run>& r){++r->routeGeneration;r->route=false;r->stepRuns.clear();if(r->step>=r->cue->children.size()){r->scheduleClosed=true;r->detail="All routes consumed; responses remain active";return;}auto step=r->cue->children[r->step];r->routeInput=step->input.outputs[0];auto i=r->inputs.find(r->routeInput);require(i!=r->inputs.end(),"Input session missing");auto snapshot=service()->snapshot();for(auto p:r->inputs){auto e=snapshot.events.find(p.second.id);r->counters[p.first]=e!=snapshot.events.end()&&e->second.started==p.second.started?e->second.presses:0;}r->watermark=r->counters[r->routeInput];r->route=true;r->detail="Step "+String(int(r->step+1))+": eligible; input response active; watermark "+String(int64(r->watermark));note(r->cue->name+": "+r->detail);}
void Coordinator::reactionTick(const std::shared_ptr<Run>& r,const Snapshot& snapshot,double now){if(!r->admitted||r->outcome==Outcome::Failed||r->scheduleClosed)return;
 if(!r->route){if(r->stepRuns.empty())return;auto child=runs[r->stepRuns.back()];if(child->outcome==Outcome::Failed){r->detail="Reaction batch failed; Retry failed members before progression";return;}auto step=r->cue->children[r->step];bool accepted=child->started&&!child->members.empty();for(const auto& m:child->members)accepted&=m.receipt.accepted;
  if(accepted&&(!step->wait||child->outcome==Outcome::Succeeded)){++r->step;armStep(r);}return;}
 auto step=r->cue->children[r->step];if(!schedulingAllowed(*r)&&!step->live)return;auto h=r->inputs[r->routeInput];auto e=snapshot.events.find(h.id);if(e==snapshot.events.end()||e->second.started!=h.started||e->second.status!=Status::Running){r->detail="Input response no longer active; explicit Restart required";return;}
 if(e->second.presses<=r->watermark)return;r->route=false;r->watermark=e->second.presses;note(r->cue->name+": route atomically consumed at generation "+String(int64(r->routeGeneration))+"; response remains active");
 bool sameMotor=false;for(auto action:step->actions)sameMotor|=std::find(action.outputs.begin(),action.outputs.end(),r->routeInput)!=action.outputs.end();if(step->deactivate||sameMotor)deactivateInput(r->id,r->routeInput);
 auto child=start(step,r->id,step->live?Id{}:r->clockRoot,false,true);runs[child]->due=clock(*runs[child],now)+step->pre;r->stepRuns.push_back(child);if(schedulingAllowed(*runs[child])&&clock(*runs[child],now)>=runs[child]->due)launch(runs[child],now);
}
void Coordinator::deactivateInput(const Id& id,const Id& motor){auto r=runs.at(id);auto i=r->inputs.find(motor);if(i!=r->inputs.end()){service()->endOwned(i->second);note(r->cue->name+": explicit scoped input Deactivate "+String(motor));}}
void Coordinator::retry(const Id& id){auto r=runs.at(id);require(r->admitted&&r->started,"This run is ended; use Restart");require(!r->members.empty(),"Select the failed movement batch in Active Cues");r->generation=++serial;r->outcome=Outcome::Pending;r->outcomeAt=-1;r->detail="Retry: prior accepted members preserved";note(r->cue->name+": new attempt generation; previous failure remains in diagnostics");launchMembers(r,true);}
void Coordinator::end(const Id& id,uint64_t expected){auto i=runs.find(id);if(i==runs.end())return;auto r=i->second;if(expected&&expected!=r->generation){note("Rejected stale End generation");return;}
 const bool wasAdmitted=r->admitted;r->admitted=false;r->route=false;r->scheduleClosed=true;++r->routeGeneration;
 if(r->song)r->song->stop();if(r->mapping)r->mapping->cease(!wasAdmitted||r->cue->cleanup);if(r->package)r->package->stop(true);for(auto child:r->children)end(child);for(auto p:r->inputs)service()->endOwned(p.second);for(auto& m:r->members){if(m.receipt.accepted)service()->endOwned(m.receipt.handle);m.held=false;}finish(r,Outcome::Cancelled,"Explicit End; scheduling closed before exact owned cleanup");r->detail=String(outcomeName(r->outcome))+(owns(*r)?"; retained ownership remains; End again to release":"; ownership ended");note(r->cue->name+": exact End complete");}
void Coordinator::tick(double now){if(ticking)return;ScopedValueSetter<bool> guard(ticking,true);std::vector<std::shared_ptr<Run>> clocks;for(auto p:runs)if(p.second->admitted&&p.second->song)clocks.push_back(p.second);for(auto r:clocks)songTick(r,now);auto snapshot=service()->snapshot();std::vector<std::shared_ptr<Run>> batch;for(auto p:runs)batch.push_back(p.second);
 std::stable_sort(batch.begin(),batch.end(),[](auto a,auto b){if(a->clockRoot!=b->clockRoot)return a->clockRoot<b->clockRoot;if(a->due!=b->due)return a->due<b->due;return a->generation<b->generation;});
 for(auto r:batch){if(r->admitted&&!r->started&&schedulingAllowed(*r)&&clock(*r,now)>=r->due)launch(r,now);if(r->started){refresh(r,now);if(r->cue->type==CueType::Reaction)reactionTick(r,snapshot,now);}}
}
var Coordinator::save()const{auto v=object();v.getDynamicObject()->setProperty("version",1);Array<var> cc;for(auto c:cues)cc.add(c->save());v.getDynamicObject()->setProperty("cues",cc);return v;}
void Coordinator::load(const var& v,bool teardown){std::vector<std::shared_ptr<Cue>> loaded;if(!v.isVoid()){require(int(v["version"])==1,"Unsupported Show Control schema");if(auto* cc=v["cues"].getArray())for(auto c:*cc)loaded.push_back(Cue::load(c));}std::set<Id> ids;std::function<void(const std::vector<std::shared_ptr<Cue>>&)> check=[&](const auto& list){for(auto c:list){require(ids.insert(c->id).second,"Duplicate cue ID");check(c->children);}};check(loaded);if(teardown)clear();cues=std::move(loaded);selected.clear();standby.clear();listFocus.clear();if(!cues.empty())standby[{}]=cues.front()->id;++revision;}
void Coordinator::clear(){std::vector<Id> ids;for(auto p:runs)if(p.second->parent.empty())ids.push_back(p.first);for(auto id:ids)end(id);runs.clear();cues.clear();standby.clear();selected.clear();randomHistory.clear();++revision;}
}
