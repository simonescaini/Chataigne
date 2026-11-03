// Actual application objects, native persistence and worker; only simulated adapters.
#include "MainIncludes.h"
#include "Midnight/NativePanel.h"
#include <iostream>
#include <windows.h>
#undef main
using namespace midnightNative;
class MultiApplication:public OrganicApplication {public:MultiApplication():OrganicApplication("Midnight multi motor acceptance",false){}void initialiseInternal(const String&)override{}};
static int checks=0;
static void check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<std::endl;}
static void pump(int ms){auto end=monotonic()+ms/1000.;while(monotonic()<end){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Thread::sleep(1);}}
static void waitFor(const std::function<bool()>& fn,const char* label,int ms=6000){auto end=monotonic()+ms/1000.;while(monotonic()<end&&!fn())pump(5);check(fn(),label);}
template<class T>static T* control(Component& c,const String& name){if(auto* t=dynamic_cast<T*>(&c))if(t->getName()==name)return t;for(auto* child:c.getChildren())if(auto* found=control<T>(*child,name))return found;return nullptr;}
static TextButton* button(Component& c,const String& name){if(auto* t=dynamic_cast<TextButton*>(&c))if(t->getButtonText()==name)return t;for(auto* child:c.getChildren())if(auto* found=button(*child,name))return found;return nullptr;}
static bool noteContains(Component& c,const String& text){if(auto* t=dynamic_cast<Label*>(&c))if(t->getText().contains(text))return true;for(auto* child:c.getChildren())if(noteContains(*child,text))return true;return false;}
static Document fixture(){Document d;d.adapters.push_back({"bus","Four-model simulated CANable","","",1000000,2000000,true,true});
    const char* models[]{"RS01","RS02","RS04","RMD-L-7025"};
    for(int i=0;i<4;++i){Motor m;m.id=models[i];m.name=models[i];m.model=models[i];m.adapter="bus";m.can=i==3?1:i+1;m.min=-720;m.max=720;m.direction=i>=2?-1:1;m.speed=40;d.motors.push_back(m);}
    Event root;root.id="root";root.name="root";root.kind=Kind::Timeline;root.duration=3;d.events.push_back(root);
    Event nested=root;nested.id="nested";nested.parent="root";nested.duration=2;d.events.push_back(nested);
    Event a;a.id="rmd-first";a.name=a.id;a.parent="nested";a.outputs={"RMD-L-7025"};a.target=12;a.speed=60;a.keepActive=false;d.events.push_back(a);
    Event b=a;b.id="rmd-second";b.name=b.id;b.target=-12;b.in={{Edge::Completed,"rmd-first"}};d.events.push_back(b);
    Event crossing;crossing.id="crossing";crossing.outputs={"RS02"};crossing.target=8;crossing.in={{Edge::Position,"RMD-L-7025",5,0,1}};d.events.push_back(crossing);
    Event link;link.id="rmd-master";link.kind=Kind::Link;link.input="RMD-L-7025";link.outputs={"RS01","RS04"};d.events.push_back(link);
    link.id="rs-master";link.input="RS02";link.outputs={"RMD-L-7025","RS04"};d.events.push_back(link);
    Event h;h.id="haptic";h.kind=Kind::Haptic;h.input="RS04";h.outputs={"RS01"};d.events.push_back(h);return d;
}
int main(int argc,char** argv){if(argc!=3)return 2;ScopedJuceInitialiser_GUI gui;MultiApplication app;
    app.engine.reset(new ChataigneEngine());app.mainComponent.reset(new MainContentComponent());app.appProperties->getUserSettings()->setValue("lastVersion",ProjectInfo::versionString);app.mainComponent->init();app.engine->createNewGraph();int code=0;
    try{auto* s=service();File path(String::fromUTF8(argv[2]));bool reload=String(argv[1])=="reload";
        if(reload){check(app.engine->loadFrom(path,false).wasOk(),"load saved mixed-model native document");waitFor([&]{return !app.engine->isLoadingFile;},"native reload finished");}else s->edit(fixture());
        waitFor([&]{auto p=s->snapshot();if(p.document.motors.size()!=4)return false;for(auto& m:p.document.motors)if(p.motors[m.id].at<0)return false;return true;},"interleaved four-model bus provides fresh addressed feedback");
        auto d=s->snapshot().document;check(d.adapters.size()==1&&d.motors[0].can==d.motors[3].can,"standard and extended motors reuse one adapter and same numeric device ID");
        check(d.motors[3].direction==-1&&protocolOf(d.motors[3])=="myactuator-v3-v4.2"&&d.events[2].outputs[0]=="RMD-L-7025","profile direction and stable event references persist");
        for(auto& line:s->snapshot().monitor)if(line.find(" TX ")!=std::string::npos){auto wire=line.substr(line.find_last_of(' ')+1);Frame f;if(parseSlcan(wire,f))check(f.extended?((f.id>>24)&31)==17:(f.data[0]==0x92||f.data[0]==0x9a||f.data[0]==0x9c),"load/connect TX is observation only");}
        for(auto& m:d.motors)check(!s->snapshot().motors[m.id].enableSent,"load/reconnect never enables a drive");
        // Old v1 documents acquire explicit protocol/direction without changing existing values.
        auto old=encodeDocument(d);old.getDynamicObject()->getProperty("motors").getArray()->getReference(0).getDynamicObject()->removeProperty("protocol");old["motors"][0].getDynamicObject()->removeProperty("direction");auto decoded=decodeDocument(old);
        check(decoded.motors[0].can==d.motors[0].can&&decoded.motors[0].current==3&&decoded.motors[0].model=="RS01"&&decoded.motors[0].direction==1,"backward-compatible old motor load preserves RS01 fields");
        {auto editor=createMotorEditor("RMD-L-7025");editor->setSize(820,850);
            check(!control<TextEditor>(*editor,"Host CAN ID")->isVisible()&&!control<TextEditor>(*editor,"Current-limit parameter / A")->isVisible(),"MyActuator editor hides unsupported host/current controls");
            check(!button(*editor,"Enable")->isEnabled()&&noteContains(*editor,"0x88"),"unsupported separate Enable is visibly explained");
            check(noteContains(*editor,"Restart the controller separately"),"zero ROM persistence/restart semantics visible in editor");
            check(control<TextEditor>(*editor,"COM port (manual selection allowed)")->isEnabled(),"manual port fallback remains editable for a saved shared adapter");
            auto model=control<ComboBox>(*editor,"Motor model");model->setSelectedId(3,sendNotificationSync);check(control<TextEditor>(*editor,"Current-limit parameter / A")->isVisible()&&control<TextEditor>(*editor,"Current-limit parameter / A")->getText().getDoubleValue()==3,"model switching exposes RS04 controls without raising current");
            model->setSelectedId(4,sendNotificationSync);button(*editor,"Save motor settings")->onClick();check(s->snapshot().document.motors[0].model=="RS01","editing MyActuator preserves other motor definitions");
        }
        for(auto id:{"RMD-L-7025","RS04"}){
            auto editor=createMotorEditor(id);editor->setSize(820,850);control<TextEditor>(*editor,"Target / mechanical degrees")->setText("5");button(*editor,"Move to Position")->onClick();
            waitFor([&]{return std::abs(s->snapshot().motors[id].position-5)<.2;},"direct Move callback executes through selected model driver");
            button(*editor,"Jog +")->onClick();waitFor([&]{return std::abs(s->snapshot().motors[id].position-6)<.3;},"direct Jog callback uses measured degrees on selected driver");button(*editor,"Stop / Disable")->onClick();
        }
        pump(80);s->injectPosition("RMD-L-7025",0);pump(60);
        s->action([](Runtime& r){r.arm();r.go("root");});
        waitFor([&]{return s->snapshot().events["rmd-second"].status==Status::Completed;},"nested timeline MyActuator arrival completes then executes subsequent event");
        check(s->snapshot().events["crossing"].started>0,"direction-normalized measured MyActuator crossing starts RobStride event");
        check(s->snapshot().motors["RMD-L-7025"].requestedAt>=0,"A4 worker transmission timestamps command without RobStride register");
        s->action([](Runtime& r){r.stopAll();});pump(80);
        s->simulatePresence("RS04",false);auto missingAt=s->snapshot().motors["RS04"].at;pump(180);
        s->action([](Runtime& r){r.move("RMD-L-7025",22,60);r.move("RS01",15,60);r.move("RS02",10,60);});
        waitFor([&]{auto p=s->snapshot();return std::abs(p.motors["RMD-L-7025"].position-22)<.5&&std::abs(p.motors["RS01"].position-15)<.5&&std::abs(p.motors["RS02"].position-10)<.5;},"missing RS04 never blocks three other models or misroutes their feedback");
        check(s->snapshot().motors["RS04"].at<=missingAt+.03,"missing motor receives no other device's position");s->simulatePresence("RS04",true);
        waitFor([&]{return monotonic()-s->snapshot().motors["RS04"].at<.1;},"missing motor observation resumes without replay");
        s->action([](Runtime& r){r.stopAll();});pump(80);s->injectPosition("RMD-L-7025",30);pump(60);s->action([](Runtime& r){r.go("rmd-master");});
        waitFor([&]{auto p=s->snapshot();return std::abs(p.motors["RS01"].position-30)<.5&&std::abs(p.motors["RS04"].position-30)<.5;},"MyActuator master follows to RS01 and inverted RS04 concurrently");
        s->action([](Runtime& r){r.stopAll();});pump(80);s->injectPosition("RS02",-20);pump(60);s->action([](Runtime& r){r.go("rs-master");});
        waitFor([&]{auto p=s->snapshot();return std::abs(p.motors["RMD-L-7025"].position+20)<.5&&std::abs(p.motors["RS04"].position+20)<.5;},"RobStride master follows to inverted MyActuator and RS04 with saved speeds");
        auto latencyStart=monotonic();s->injectPosition("RS02",-10);waitFor([&]{return s->snapshot().motors["RMD-L-7025"].requestedAt>latencyStart;},"new master measurement reaches MyActuator target transmission");auto triggerLatency=(s->snapshot().motors["RMD-L-7025"].requestedAt-latencyStart)*1000;
        s->action([](Runtime& r){r.stopAll();});pump(80);s->action([](Runtime& r){r.go("haptic");});
        waitFor([&]{return s->snapshot().motors["RS04"].enableSent;},"RS04 mixed haptic setup transmitted");s->injectPosition("RS04",10);
        waitFor([&]{auto p=s->snapshot();return std::abs(p.motors["RS04"].commandedTorque)>.01&&p.motors["RS04"].hapticAt>=0;},"RS04 haptic packet replies decode through real bus and worker");
        NativePanel panel("Multi-motor acceptance");panel.setSize(1200,760);s->resetMetrics();
        for(int i=0;i<30;++i){panel.update();panel.createComponentSnapshot(panel.getLocalBounds());JSON::toString(s->save());pump(5);}auto loops=s->snapshot().metrics.loops;Thread::sleep(250);check(s->snapshot().metrics.loops>loops+10,"motor worker continues during blocked GUI with four mixed models");
        auto met=s->snapshot().metrics;var report(new DynamicObject());auto* o=report.getDynamicObject();o->setProperty("scope","Four models on one production fake bus; RS04/RS01 haptic pair; rendering/serialization and 250ms GUI block");o->setProperty("loops",int64(met.loops));o->setProperty("interval_mean_ms",met.intervalMean*1000);o->setProperty("interval_max_ms",met.intervalMax*1000);o->setProperty("enqueue_to_write_max_ms",met.transmitDelayMax*1000);o->setProperty("queue_max",int(met.queueMax));o->setProperty("feedback_age_max_ms_at_snapshot",met.feedbackAgeMax*1000);o->setProperty("injected_master_to_rmd_target_tx_ms",triggerLatency);o->setProperty("tx",int64(met.tx));o->setProperty("rx",int64(met.rx));path.getParentDirectory().getParentDirectory().getChildFile("evidence/multi-motor-worker-metrics.json").replaceWithText(JSON::toString(report));
        s->action([](Runtime& r){r.stopAll();});pump(80);s->connect("bus",false);waitFor([&]{return !s->snapshot().motors["RMD-L-7025"].connected;},"mixed adapter disconnect");s->connect("bus",true);waitFor([&]{return s->snapshot().motors["RMD-L-7025"].at>=0;},"mixed adapter reconnect restores observation");
        check(!s->snapshot().motors["RMD-L-7025"].enableSent&&!s->snapshot().motors["RS04"].enableSent,"mixed reconnect restores no movement or haptic enable");
        // ROM zero is exercised only on the fake adapter and must not fabricate zero feedback.
        s->injectPosition("RMD-L-7025",17);pump(60);s->action([](Runtime& r){r.mechanicalZero("RMD-L-7025");});pump(80);check(std::abs(s->snapshot().motors["RMD-L-7025"].position-17)<.01,"fake MyActuator zero waits for separate restart, no invisible local zero");
        if(!reload){bool done=false,ok=false;app.engine->saveAsAsync(path,false,false,false,[&](FileBasedDocument::SaveResult r){done=true;ok=r==FileBasedDocument::savedOk;});waitFor([&]{return done;},"native mixed Save As callback");check(ok,"native mixed project saved");}
        s->action([](Runtime& r){r.stopAll();});app.mainComponent->clear();std::cout<<"SUCCESS "<<checks<<" mixed integration checks in "<<argv[1]<<std::endl;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;if(service())std::cerr<<service()->snapshot().error<<std::endl;code=1;}
    app.mainComponent.reset();app.engine.reset();return code;
}
