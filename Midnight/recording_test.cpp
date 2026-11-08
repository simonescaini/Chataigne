#include "MainIncludes.h"
#include "Midnight/Recording.h"
#include <iostream>
#include <windows.h>
#undef main
using namespace midnightNative;
class RecorderApp:public OrganicApplication {public:RecorderApp():OrganicApplication("Midnight recorder software acceptance",false){}void initialiseInternal(const String&)override{}};
static int checks=0;
static void check(bool ok,const char* s){if(!ok)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;}
static void pump(int ms){auto until=monotonic()+ms*.001;while(monotonic()<until){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Thread::sleep(1);}}
static void wait(std::function<bool()> f,const char* s){auto until=monotonic()+4;while(!f()&&monotonic()<until)pump(10);check(f(),s);}
int main(){ScopedJuceInitialiser_GUI gui;RecorderApp app;app.engine.reset(new ChataigneEngine());app.engine->createNewGraph();try{
 Document d;Adapter a;a.id="fake";a.simulated=true;d.adapters={a};Motor m;m.id="one";m.adapter=a.id;m.can=1;m.min=-100;m.max=100;Motor two=m;two.id="two";two.can=2;d.motors={m,two};
 MeasuredCapture capture;capture.start("original",m,a,10);capture.sample(two.id,22,10.1);capture.sample(m.id,20,9.99);
 check(capture.take.samples.empty()&&capture.take.staleBeforeStart==1,"wrong-motor and pre-start feedback are not invented samples");
 Runtime r;r.document=d;r.measuredSample=[&](const Id& id,double angle,double at){capture.sample(id,angle,at);};
 auto feed=[&](double angle,double at,bool pending=true){auto f=write(m.host,m.can,0x7016,radians(angle),m.model);f.id=(0x11u<<24)|(m.can<<8)|m.host;f.data[0]=0x19;f.data[1]=0x70;r.feedback[m.id].positionReadAt=pending?at-.001:-1;r.receive(a.id,f,at);};
 feed(-12,10.1);feed(-12,10.2);feed(5,10.55);feed(-12,10.7);feed(42,10.8,false);
 check(capture.take.samples.size()==4,"only accepted real encoder replies feed capture; unsolicited duplicate rejected");
 auto take=capture.stop(11);check(take.samples[1].angle==take.samples[0].angle,"dwell samples preserved separately");check(std::abs(take.samples[2].elapsed-.55)<1e-9,"actual monotonic receipt timing preserved");check(take.gaps>=1&&std::abs(take.largestGap-.35)<1e-9,"missing interval reported honestly");
 auto raw=JSON::toString(encodeTake(take));check(JSON::toString(encodeTake(decodeTake(JSON::parse(raw))))==raw,"original sample values timestamp provenance and hardware metadata roundtrip exactly");
 service()->edit(d);wait([&]{return service()->snapshot().motors[m.id].at>=0;},"fake service connected for measured capture");
 auto* library=recordings();auto item=library->retain(take,"Throw and return");check(!item->editable,"retaining raw take does not silently convert");check(service()->snapshot().events.empty(),"capture retention and construction emit no commands");
 item->convertOriginal();check(item->movement.curve->items.size()==4,"explicit conversion retains every measured sample");check(std::abs(item->movement.duration->doubleValue()-.6)<1e-5,"conversion keeps recorded trajectory duration");
 for(int i=0;i<4;++i){auto* key=item->movement.curve->items[i];check(std::abs(key->getPosAndValue().x-(take.samples[i].elapsed-take.samples[0].elapsed))<1e-6,"editable point retains exact relative timing at float precision");check(std::abs(key->getPosAndValue().y-take.samples[i].angle)<1e-5,"editable point retains measured absolute angle");}
 item->movement.target->setValue(-10);check(JSON::toString(encodeTake(*item->original))==raw,"editing a movement cannot rewrite immutable original");item->convertOriginal();check(std::abs(item->movement.target->doubleValue()+12)<1e-4,"explicit restore reverses edits from retained original");
 auto saved=library->save();const auto id=item->id;library->load(JSON::parse(JSON::toString(saved)));item=library->find(id);check(item&&JSON::toString(encodeTake(*item->original))==raw,"saved library reopens stable identity and immutable raw take");check(service()->snapshot().events.empty(),"reopening saved takes remains inert");
 auto tampered=JSON::parse(JSON::toString(saved));tampered["takes"][0]["original"].getDynamicObject()->setProperty("startedAt",999);bool rejected=false;try{library->load(tampered);}catch(...){rejected=true;}check(rejected&&library->find(id),"altered raw take hash rejected without discarding valid library");
 service()->startCapture(m.id);pump(130);auto live=service()->captureProgress();check(live.active&&live.samples>0,"service feedback worker feeds recorder without UI sample polling");check(service()->snapshot().motors[m.id].owner.empty(),"starting recording does not claim or move motor");auto actual=service()->stopCapture();check(actual.samples.size()>0&&!service()->captureProgress().active,"Stop recording ends capture only");
 Event unrelated;unrelated.id="independent";unrelated.outputs={two.id};unrelated.duration=5;unrelated.target=10;service()->launch(unrelated);
 library->play(id);auto eventId="recording/"+id;check(service()->snapshot().motors[m.id].owner==eventId,"recorded playback uses shared native Move ownership");
 {auto editor=std::unique_ptr<InspectableEditor>(item->movement.getEditor(false));editor.reset();}check(service()->snapshot().motors[m.id].owner==eventId,"closing recording editor leaves active playback owned");
 wait([&]{return service()->snapshot().events[eventId].status==Status::Completed;},"recorded curve reaches finite clock-end and fresh measured-arrival completion");check(service()->snapshot().motors[m.id].owner==eventId,"finite completion retains final holding ownership");
 library->end(id);auto end=service()->snapshot();check(end.motors[m.id].owner.empty()&&end.motors[two.id].owner==unrelated.id,"exact recorded End preserves independent motor work");
 check(JSON::toString(encodeTake(*item->original))==raw,"playback and End preserve raw take");service()->end(unrelated.id);
 std::cout<<"SUCCESS "<<checks<<" recorder checks"<<std::endl;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;app.engine.reset();return 1;}app.engine.reset();return 0;}
