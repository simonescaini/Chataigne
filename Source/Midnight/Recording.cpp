#include "Recording.h"
namespace midnightNative {
namespace {
var takeObject(){return var(new DynamicObject());}
String takeHash(const var& v){auto text=JSON::toString(v,false);return juce::SHA256(text.toRawUTF8(),text.getNumBytesAsUTF8()).toHexString();}
}
var encodeTake(const MeasuredTake& take){
    auto v=takeObject();auto* o=v.getDynamicObject();o->setProperty("version",1);o->setProperty("id",String(take.id));
    o->setProperty("units","absolute mechanical degrees");o->setProperty("clock","host monotonic seconds at validated encoder reply receipt; not a device sample timestamp");
    o->setProperty("reference","Controller mechanical angle; no software offset or zero command on capture. Hardware zero persistence is unknown.");
    Document d;d.motors={take.motor};d.adapters={take.adapter};o->setProperty("hardwareSnapshot",encodeDocument(d));
    o->setProperty("startedAt",take.startedAt);o->setProperty("stoppedAt",take.stoppedAt);o->setProperty("staleBeforeStart",int64(take.staleBeforeStart));o->setProperty("dropped",int64(take.dropped));o->setProperty("gapsOver150ms",int64(take.gaps));o->setProperty("largestGap",take.largestGap);o->setProperty("referenceChanged",take.referenceChanged);o->setProperty("stoppedReason",String(take.stoppedReason));
    Array<var> samples;for(const auto& s:take.samples)samples.add(var(Array<var>{s.elapsed,s.receivedAt,s.angle,int64(s.sequence)}));o->setProperty("samples",samples);return v;
}
MeasuredTake decodeTake(const var& v){
    require(int(v["version"])==1,"Unsupported take version");MeasuredTake t;t.id=v["id"].toString().toStdString();auto d=decodeDocument(v["hardwareSnapshot"]);require(d.motors.size()==1&&d.adapters.size()==1,"A take requires exactly one motor snapshot");t.motor=d.motors[0];t.adapter=d.adapters[0];
    t.startedAt=double(v["startedAt"]);t.stoppedAt=double(v["stoppedAt"]);finite(t.startedAt);finite(t.stoppedAt);
    t.staleBeforeStart=int64(v["staleBeforeStart"]);t.dropped=int64(v["dropped"]);t.gaps=int64(v["gapsOver150ms"]);t.largestGap=double(v["largestGap"]);t.referenceChanged=bool(v["referenceChanged"]);t.stoppedReason=v["stoppedReason"].toString().toStdString();
    auto* samples=v["samples"].getArray();require(samples&&samples->size()<=MeasuredCapture::capacity,"Invalid take sample count");
    for(const auto& row:*samples){require(row.isArray()&&row.size()==4,"Invalid measured sample");MeasuredSample s{double(row[0]),double(row[1]),double(row[2]),uint64_t(int64(row[3]))};finite(s.elapsed);finite(s.receivedAt);finite(s.angle);t.samples.push_back(s);}return t;
}
RecordedMovement::RecordedMovement(MeasuredTake take):original(std::make_shared<const MeasuredTake>(std::move(take))){movement.motor->select(original->motor.id);}
void RecordedMovement::convertOriginal(){
    const auto& samples=original->samples;require(samples.size()>=2,"Record at least two genuine encoder samples");
    require(!original->referenceChanged,"Mechanical zero changed during this take; select an intact take");
    const double origin=samples.front().elapsed,length=samples.back().elapsed-origin;require(length>=.01,"Measured trajectory must span at least .01 seconds");
    float previous=-1;for(const auto& s:samples){float t=float(s.elapsed-origin);require(t>previous,"Take has duplicate/non-increasing times at curve precision; raw take is retained");previous=t;}
    ScopedValueSetter<bool> importing(movement.isCurrentlyLoadingData,true);
    // Explicit reversible conversion: every genuine sample retained, no normalization or simplification.
    movement.curve->clear();movement.duration->setValue(length);movement.curve->setLength(float(length));
    for(const auto& s:samples)movement.curve->addKey(float(s.elapsed-origin),float(s.angle))->easingType->setValueWithData(Easing::LINEAR);
    movement.startAngle->setValue(samples.front().angle);movement.target->setValue(samples.back().angle);movement.duration->setValue(length);movement.playCurve->setValue(true);movement.nominalSpeed->setValue(std::abs(samples.back().angle-samples.front().angle)/length);editable=true;
}
std::shared_ptr<RecordedMovement> RecordingLibrary::retain(MeasuredTake t,const String& name){
    for(auto item:items)if(item->original->id==t.id)return item;
    require(!t.id.empty(),"No recorded take");auto item=std::make_shared<RecordedMovement>(std::move(t));item->name=name.isEmpty()?"Recorded movement":name;items.push_back(item);if(Engine::mainEngine)Engine::mainEngine->changed();return item;
}
std::shared_ptr<RecordedMovement> RecordingLibrary::find(const Id& id)const {for(auto x:items)if(x->id==id)return x;return {};}
void RecordingLibrary::play(const Id& id){auto item=find(id);require(bool(item)&&item->editable,"Convert the take to an editable movement first");auto e=item->movement.makeEvent();e.id="recording/"+id;e.name=item->name.toStdString();service()->launch(e);playback[id]=e.id;}
void RecordingLibrary::end(const Id& id){auto it=playback.find(id);if(it!=playback.end()){if(service())service()->end(it->second);playback.erase(it);}}
void RecordingLibrary::clear(){for(const auto& p:playback)if(service())service()->end(p.second);playback.clear();items.clear();}
var RecordingLibrary::save()const{Array<var> all;for(const auto& item:items){auto v=takeObject();auto* o=v.getDynamicObject();auto raw=encodeTake(*item->original);o->setProperty("id",String(item->id));o->setProperty("name",item->name);o->setProperty("original",raw);o->setProperty("originalSHA256",takeHash(raw));o->setProperty("editable",item->editable);o->setProperty("movement",item->movement.getJSONData());all.add(v);}auto root=takeObject();root.getDynamicObject()->setProperty("version",1);root.getDynamicObject()->setProperty("takes",all);return root;}
void RecordingLibrary::load(const var& root){
    std::vector<std::shared_ptr<RecordedMovement>> loaded;if(root.isVoid()){clear();return;}require(int(root["version"])==1,"Unsupported recording library version");
    if(auto* all=root["takes"].getArray())for(auto v:*all){require(takeHash(v["original"])==v["originalSHA256"].toString(),"Original take integrity check failed");auto item=std::make_shared<RecordedMovement>(decodeTake(v["original"]));item->id=v["id"].toString().toStdString();item->name=v["name"].toString();item->editable=bool(v["editable"]);item->movement.loadJSONData(v["movement"]);loaded.push_back(item);}clear();items=std::move(loaded);
}
RecordingLibrary* recordings(){auto* engine=dynamic_cast<ChataigneEngine*>(Engine::mainEngine);return engine?engine->recordings.get():nullptr;}
}
