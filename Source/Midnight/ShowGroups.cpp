#include "ShowControl.h"
namespace midnightShow {
void Coordinator::startGroup(const std::shared_ptr<Run>& r,double now){
 require(r->cue->song.isEmpty(),"Song adapter not installed in this development checkpoint");
 std::vector<std::shared_ptr<const CueSpec>> eligible;for(auto c:r->cue->children)if(c->eligible)eligible.push_back(c);
 require(!eligible.empty(),"Group has no eligible child");
 if(r->cue->mode==GroupMode::Timeline){std::stable_sort(eligible.begin(),eligible.end(),[](auto a,auto b){return a->pre<b->pre;});for(auto c:eligible){auto id=start(c,r->id,r->clockRoot,true,true);runs[id]->due=r->startedAt+c->pre;r->dispatched.insert(c->id);}r->scheduleClosed=true;}
 else if(r->cue->mode==GroupMode::Random){std::vector<std::shared_ptr<const CueSpec>> available;for(auto c:eligible){bool busy=false;for(auto p:runs)busy|=p.second->cue->id==c->id&&p.second->admitted&&p.second->outcome==Outcome::Pending;if(!busy)available.push_back(c);}require(!available.empty(),"Random: no eligible non-running children");auto& history=randomHistory[r->cue->id];bool unseen=false;for(auto c:available)unseen|=!history.count(c->id);if(!unseen)history.clear();available.erase(std::remove_if(available.begin(),available.end(),[&](auto c){return history.count(c->id)!=0;}),available.end());auto chosen=available[std::uniform_int_distribution<size_t>(0,available.size()-1)(random)];history.insert(chosen->id);note("Random selected "+String(chosen->id));start(chosen,r->id,r->clockRoot,false,true);r->scheduleClosed=true;}
 else {for(auto c:eligible)r->order.push_back(c->id);auto first=eligible.front();start(first,r->id,r->clockRoot,false,r->cue->mode==GroupMode::Playlist);r->nextChild=1;r->scheduleClosed=r->cue->mode==GroupMode::StartFirst;}
}
void Coordinator::next(const Id& id){auto r=runs.at(id);require(r->cue->type==CueType::Group,"Select a Group run");require(r->admitted,"Group ended");if(r->nextChild>=r->order.size()){r->scheduleClosed=true;return;}auto nextId=r->order[r->nextChild++];for(auto c:r->cue->children)if(c->id==nextId){start(c,r->id,r->clockRoot,false,r->cue->mode==GroupMode::Playlist);break;}if(r->nextChild>=r->order.size()&&r->cue->mode==GroupMode::Enter)r->scheduleClosed=true;}
void Coordinator::groupTick(const std::shared_ptr<Run>& r,double){if(!r->admitted)return;
 if(r->cue->mode==GroupMode::Playlist&&!r->scheduleClosed&&!r->children.empty()&&schedulingAllowed(*r)){auto child=runs[r->children.back()];if(child->outcome==Outcome::Succeeded&&isFinite(*child->cue))next(r->id);else if(child->outcome==Outcome::Failed)r->detail="Playlist branch failed; Retry failed members or explicit Next";else if(!isFinite(*child->cue))r->detail="Persistent child: explicit Next advances without ending it";}
 bool all=true,failed=false;for(auto id:r->children){auto c=runs[id];if(isFinite(*c->cue)){all&=c->outcome==Outcome::Succeeded;failed|=c->outcome==Outcome::Failed;}}if(r->scheduleClosed&&failed)finish(r,Outcome::Failed,"A required finite descendant failed; retained work remains independently endable");if(r->scheduleClosed&&all)finish(r,Outcome::Succeeded,"Group schedule closed; finite descendants succeeded; retained descendants remain owned");
}
void Coordinator::pause(const Id& id,bool pause){auto r=runs.at(id);r->paused=pause;note(pause?"Scene scheduling paused; native ongoing work continues":"Scene scheduling resumed");}
void Coordinator::seek(const Id&,double){throw std::runtime_error("Select a music scene with a loaded primary song");}
void Coordinator::tap(const Id&){throw std::runtime_error("Select a music scene with a loaded primary song");}
}
