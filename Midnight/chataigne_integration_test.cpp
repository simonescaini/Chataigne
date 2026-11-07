// Actual Chataigne factories, Actions, Mapping outputs, editors, persistence and fake CAN.
// Never opens a physical adapter. No original show is loaded by this executable.
#include "MainIncludes.h"
#include "Module/ModuleIncludes.h"
#include "StateMachine/StateMachineIncludes.h"
#include "Common/Processor/ProcessorIncludes.h"
#include "Midnight/CupboardCondition.h"
#include "Midnight/ChataigneCommand.h"
#include <iostream>
#include <windows.h>
#undef main
using namespace midnightNative;
class IntegrationApp:public OrganicApplication{
public:IntegrationApp():OrganicApplication("Midnight Integration software acceptance",false){}
void initialiseInternal(const String&)override{}
};
static int checks=0;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<std::endl;}
static void pump(int ms){auto end=monotonic()+ms/1000.;while(monotonic()<end){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Thread::sleep(1);}}
static void waitFor(std::function<bool()> f,const char* text,int ms=5000){auto end=monotonic()+ms/1000.;while(!f()&&monotonic()<end)pump(10);if(!f())std::cerr<<"Runtime: "<<service()->snapshot().error<<std::endl;check(f(),text);}
static void position(const Id& id,double angle){service()->injectPosition(id,angle);waitFor([&]{return std::abs(service()->snapshot().motors[id].position-angle)<.1;},"fake encoder stimulus observed");}
static State* state(const String& name){auto* s=new State();s->setNiceName(name);int n=StateManager::getInstance()->items.size();s->viewUIPosition->setPoint(float((n%3-1)*350-150),float((n/3-1)*230-100));StateManager::getInstance()->addItem(s);s->active->setValue(false);s->loadActivationBehavior->setValueWithData(State::NONACTIVE);return s;}
static Action* action(State* s,const String& name){auto* a=new Action();a->setNiceName(name);s->pm->addItem(a);return a;}
static Consequence* consequence(Action* a,MidnightModule* module,const String& command){
    auto* c=new Consequence();c->setNiceName(command);a->csmOn->addItem(c);c->setCommand(module->getCommandDefinitionFor("",command));check(c->command!=nullptr,"native command selection resolves");return c;
}
static MidnightCommand* cmd(Consequence* c){return dynamic_cast<MidnightCommand*>(c->command.get());}
static CupboardButtonCondition* button(Action* a,bool spring=false,bool rearm=true){
    auto* b=new CupboardButtonCondition();a->cdm->addItem(b);b->motorChoice->select("cupboard-a");b->referenceAngle->setValue(20);b->triggerOffset->setValue(-5);b->response->setValueWithData(spring?1:0);b->rearm->setValue(rearm);return b;
}
static void activation(Action* a,bool on){
    var p(new DynamicObject());p.getDynamicObject()->setProperty("type",on?ActivationCondition::ON_ACTIVATE:ActivationCondition::ON_DEACTIVATE);
    a->cdm->addItem(new ActivationCondition(p));
}
static void save(IntegrationApp& app,const File& path){
    bool done=false,ok=false;app.engine->saveAsAsync(path,false,false,false,[&](FileBasedDocument::SaveResult r){done=true;ok=r==FileBasedDocument::savedOk;});
    waitFor([&]{return done;},"native project save callback completes");check(ok,"native project saved");
}
template<class T>static T* findControl(Component& root,const String& name){if(auto* c=dynamic_cast<T*>(&root))if(c->getName()==name)return c;for(auto* child:root.getChildren())if(auto* found=findControl<T>(*child,name))return found;return nullptr;}
static TextButton* findButton(Component& root,const String& name){if(auto* c=dynamic_cast<TextButton*>(&root))if(c->getButtonText()==name)return c;for(auto* child:root.getChildren())if(auto* found=findButton(*child,name))return found;return nullptr;}
static void snapshot(Component& c,const File& path){c.setSize(900,900);c.addToDesktop(0);c.setVisible(true);pump(150);FileOutputStream out(path);out.setPosition(0);out.truncate();PNGImageFormat png;check(png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),out),"actual reusable editor rendered");c.removeFromDesktop();}

int main(int argc,char** argv){
    if(argc!=3)return 2;ScopedJuceInitialiser_GUI gui;IntegrationApp app;
    app.engine.reset(new ChataigneEngine());app.mainComponent.reset(new MainContentComponent());app.appProperties->getUserSettings()->setValue("lastVersion",ProjectInfo::versionString);app.mainComponent->init();app.engine->createNewGraph();int code=0;
    try{
        File path(String::fromUTF8(argv[2]));bool reload=String(argv[1])=="reload";auto* shared=service();
        if(String(argv[1])=="compatibility"){
            auto original=JSON::parse(path);check(original.isObject(),"compatibility copy parses");
            auto expected=decodeDocument(original["midnightNative"]);
            for(auto& adapter:expected.adapters)check(adapter.simulated&&adapter.port.empty(),"compatibility copy is fake CAN only");
            check(app.engine->loadFrom(path,false).wasOk(),"compatibility copy accepted by actual application");
            waitFor([&]{return !app.engine->isLoadingFile;},"compatibility copy load completed");
            auto loaded=shared->snapshot().document;
            check(JSON::toString(encodeDocument(expected))==JSON::toString(encodeDocument(loaded)),"existing hardware identities settings and interactions retain normalized data");
            save(app,path.getSiblingFile(path.getFileNameWithoutExtension()+"_roundtrip.noisette"));
            auto persisted=JSON::parse(path.getSiblingFile(path.getFileNameWithoutExtension()+"_roundtrip.noisette"));
            check(JSON::toString(encodeDocument(expected))==JSON::toString(encodeDocument(decodeDocument(persisted["midnightNative"]))),"existing project survives load/save without hardware or interaction loss");
        }else if(reload){
            check(app.engine->loadFrom(path,false).wasOk(),"saved project accepted");
            waitFor([&]{return !app.engine->isLoadingFile;},"fresh-process native project load finished");
            waitFor([&]{return shared->snapshot().motors["cupboard-a"].at>=0;},"fake communication reconnects after reopen");
            auto s=shared->snapshot();check(s.document.motors.size()==5,"hardware registry survives reopen");
            check(s.document.motors[0].name=="Renamed cupboard","shared hardware rename survives reopen");
            for(auto& m:s.document.motors)check(!s.motors[m.id].enableSent&&!s.motors[m.id].torqueMode,"reopen does not activate motor behaviour");
            auto* module=dynamic_cast<MidnightModule*>(ModuleManager::getInstance()->items[0]);check(module!=nullptr,"native Midnight module survives reopen");
            check(StateManager::getInstance()->items.size()>=9,"all example scenes survive reopen");
            auto* st=StateManager::getInstance()->items[0];auto* a=dynamic_cast<Action*>(st->pm->items[0]);auto* b=dynamic_cast<CupboardButtonCondition*>(a->cdm->items[0]);
            check(b&&b->motorChoice->motorId()=="cupboard-a","per-use condition retains stable renamed motor identity");
            check(!b->activeFeedback->boolValue(),"condition reconstruction remains inert");
            auto* deactivate=dynamic_cast<Consequence*>(a->csmOn->items[0]);
            check(cmd(deactivate)->target->getTargetContainerAs<CupboardButtonCondition>()==b,"explicit button-end target resolves after reopen");
            auto* move=cmd(dynamic_cast<Consequence*>(a->csmOn->items[1]));
            check(move->behaviour->curve->items.size()>=2&&move->behaviour->motor->motorId()=="cupboard-a","command curve and motor reference survive reopen");
            st->active->setValue(true);position("cupboard-a",20);b->activate();position("cupboard-a",14);
            waitFor([&]{return !b->activeFeedback->boolValue()&&shared->snapshot().motors["cupboard-a"].requested==45;},"reopened Action deactivates button then launches Move");
            st->active->setValue(false);move->endInteractions();
            auto* mappingState=StateManager::getInstance()->items[7];auto* mapping=dynamic_cast<Mapping*>(mappingState->pm->items[0]);
            check(mapping&&mapping->im.items.size()==1&&mapping->im.items[0]->inputReference!=nullptr,"encoder Mapping input reference survives reopening");
            mappingState->active->setValue(true);position("cupboard-a",31);
            waitFor([&]{return std::abs(shared->snapshot().motors["cupboard-b"].requested-31)<.1;},"reopened encoder Mapping sends unchanged absolute angle");
            mappingState->active->setValue(false);dynamic_cast<MidnightCommand*>(mapping->om.items[0]->command.get())->endInteractions();
        }else{
            Document d;Adapter bus;bus.id="fake-can";bus.name="Software CAN only";bus.simulated=true;d.adapters={bus};
            const char* ids[]={"cupboard-a","cupboard-b","follower-c","rmd","rs04"};
            const char* models[]={"RS02","RS01","RS02","RMD-L-7025","RS04"};
            for(int i=0;i<5;++i){Motor m;m.id=ids[i];m.name=ids[i];m.adapter=bus.id;m.can=i+1;m.model=models[i];m.min=-360;m.max=360;d.motors.push_back(m);}
            shared->edit(d);waitFor([&]{auto s=shared->snapshot();return s.motors["cupboard-a"].at>=0&&s.motors["rs04"].at>=0;},"five fake motor profiles communicate through shared service");
            auto* module=new MidnightModule();ModuleManager::getInstance()->addItem(module);
            check(module->getCommands().size()>=7,"Midnight commands registered in native module");
            auto* st=state("01 Button triggers Move");auto* a=action(st,"Press then deactivate then Move");auto* b=button(a);
            check(!shared->snapshot().events.count(b->interactionId),"constructing a Cupboard condition is inert");
            auto editor=std::unique_ptr<InspectableEditor>(b->getEditor(false));pump(50);
            check(!shared->snapshot().events.count(b->interactionId),"opening a Cupboard editor is inert");editor.reset();
            position("cupboard-a",20);st->active->setValue(true);b->activate();
            waitFor([&]{return b->activeFeedback->boolValue();},"explicit activation starts button");
            check(!shared->snapshot().motors["cupboard-a"].enableSent,"Free sends actual output-disable");
            auto generation=shared->snapshot().motors["cupboard-a"].generation;
            for(int i=0;i<20;++i)b->forceCheck();pump(60);
            check(generation==shared->snapshot().motors["cupboard-a"].generation,"condition evaluation never reinitializes response");
            position("cupboard-a",14);waitFor([&]{return b->getIsValid();},"negative signed crossing produces TRUE");
            auto pressed=shared->snapshot().events[b->interactionId].presses;pump(100);
            check(shared->snapshot().events[b->interactionId].presses==pressed,"holding past threshold does not refire");
            check(b->activeFeedback->boolValue(),"press does not deactivate button");
            position("cupboard-a",20);waitFor([&]{return !b->getIsValid();},"return rearms and clears TRUE");
            check(b->activeFeedback->boolValue(),"FALSE does not deactivate physical response");
            b->deactivate();position("cupboard-a",14);b->activate();pump(80);
            check(!b->getIsValid(),"already beyond threshold does not synthesize a press");
            position("cupboard-a",20);position("cupboard-a",14);waitFor([&]{return b->getIsValid();},"return then cross presses after beyond-threshold activation");
            b->deactivate();b->rearm->setValue(false);position("cupboard-a",20);b->activate();position("cupboard-a",14);position("cupboard-a",20);
            check(b->getIsValid(),"non-rearming button remains latched after return");b->reset();check(!b->getIsValid(),"explicit reset clears latch without ending activation");
            check(shared->snapshot().events[b->interactionId].status==Status::Running,"reset retains active response");
            b->deactivate();b->rearm->setValue(true);b->referenceMode->setValueWithData(1);position("cupboard-a",37);b->activate();position("cupboard-a",35);pump(30);
            check(std::abs(b->liveReference->doubleValue()-37)<.1,"capture reference is fixed once per activation");
            b->deactivate();b->referenceMode->setValueWithData(0);

            // Actual Action consequences, ordered explicit deactivation and Move on same motor.
            auto* endButton=consequence(a,module,"Deactivate Cupboard button");cmd(endButton)->target->setValueFromTarget(b);
            auto* moveHandler=consequence(a,module,"Move to position");auto* move=cmd(moveHandler);
            move->behaviour->motor->select("cupboard-a");
            position("cupboard-a",20);b->activate();position("cupboard-a",14);
            waitFor([&]{return !shared->snapshot().events.count(b->interactionId)&&shared->snapshot().motors["cupboard-a"].owner==move->instanceId+"/0";},"native Action deactivates button before same-motor Move");
            move->endInteractions();st->active->setValue(false);
            auto* copied=new Action();copied->loadJSONData(a->getJSONData());st->pm->addItem(copied);
            auto* copiedButton=dynamic_cast<CupboardButtonCondition*>(copied->cdm->items[0]);
            check(copiedButton&&copiedButton->interactionId!=b->interactionId,"copied Action has independent per-use runtime identity");
            copiedButton->triggerOffset->setValue(-8);
            check(b->triggerOffset->doubleValue()==-5&&copiedButton->motorChoice->motorId()==b->motorChoice->motorId(),"copied settings are independent and retain motor reference");
            check(!shared->snapshot().events.count(copiedButton->interactionId),"copy/load does not activate button");
            pump(250);auto* copiedEnd=cmd(dynamic_cast<Consequence*>(copied->csmOn->items[0]));
            check(copiedEnd->target->getTargetContainerAs<CupboardButtonCondition>()==copiedButton,"copied Action deactivation targets its own copied button");
            st->pm->removeItem(copied);

            auto* springState=state("02 Spring assisted button");auto* springAction=action(springState,"Spring press");auto* spring=button(springAction,true);
            position("cupboard-a",20);springState->active->setValue(true);spring->activate();
            waitFor([&]{return shared->snapshot().motors["cupboard-a"].enableSent&&shared->snapshot().motors["cupboard-a"].torqueMode;},"spring uses existing RobStride torque setup");
            position("cupboard-a",19);waitFor([&]{return std::abs(shared->snapshot().motors["cupboard-a"].commandedTorque)>.001;},"spring response runs in native worker");
            springState->active->setValue(false);waitFor([&]{return !shared->snapshot().events.count(spring->interactionId);},"state exit deactivates Cupboard response");
            auto* freeState=state("03 Free rearming button");button(action(freeState,"Free button"));
            auto* latchState=state("04 Latched button and explicit reset");auto* latch=button(action(latchState,"Latched button"),false,false);
            auto* resetHandler=consequence(action(latchState,"Reset latch"),module,"Reset Cupboard button");cmd(resetHandler)->target->setValueFromTarget(latch);

            auto* curveState=state("05 Absolute curve from measured 30 degrees");auto* curveAction=action(curveState,"Play authored curve");
            auto* curveHandler=consequence(curveAction,module,"Move to position");auto* movement=cmd(curveHandler);auto& def=*movement->behaviour;
            def.motor->select("cupboard-a");def.target->setValue(60);def.nominalSpeed->setValue(30);
            check(std::abs(def.duration->doubleValue()-2)<.001,"nominal speed synchronizes duration from authored endpoints");
            auto* custom=def.curve->addKey(1,15);custom->easingType->setValueWithData(Easing::BEZIER);
            auto* bezier=dynamic_cast<CubicEasing*>(custom->easing.get());bezier->anchor1->setPoint(.2f,7);bezier->anchor2->setPoint(-.2f,-3);
            def.duration->setValue(4);
            check(def.curve->items.size()==3&&std::abs(custom->position->doubleValue()-2)<.001,"duration stretches timing without discarding interior keys");
            check(std::abs(bezier->anchor1->y-7)<.001&&std::abs(bezier->anchor1->x-.4)<.001,"duration preserves Bezier angular handles");
            def.startAngle->setValue(10);check(def.curve->items[0]->value->doubleValue()==10,"numeric authored start updates graphical key");
            custom->value->setValue(12);check(def.duration->doubleValue()==4,"shape edit retains duration");
            def.curve->items.getLast()->value->setValue(70);check(def.target->doubleValue()==70,"graphical endpoint updates numeric target");
            def.target->setValue(10);check(def.duration->doubleValue()==4&&!def.nominalSpeed->enabled,"closed curve keeps explicit duration and marks nominal speed unavailable");
            def.target->setValue(400);
            {bool rejected=false;try{auto invalid=def.makeEvent();invalid.id="invalid-curve";shared->launch(invalid);}catch(const std::exception&){rejected=true;}
            check(rejected&&def.target->doubleValue()==400&&!shared->snapshot().events.count("invalid-curve"),"out-of-range curve rejects without reshaping or launching");}
            def.startAngle->setValue(0);def.target->setValue(45);def.duration->setValue(.4);def.driveSpeed->setValue(1);
            position("cupboard-a",30);curveState->active->setValue(true);curveHandler->triggerCommand();
            auto initial=shared->snapshot();check(initial.motors["cupboard-a"].requested==0,"playback first request is authored zero from measured 30");
            pump(520);auto playback=shared->snapshot();
            check(playback.events[movement->instanceId+"/0"].clockEnded,"curve clock completes while movement is physically resisted in simulation");
            check(playback.motors["cupboard-a"].position>20&&std::abs(playback.motors["cupboard-a"].requested-45)<.1,"requested and measured remain distinct and curve endpoint is unchanged");
            movement->endInteractions();curveState->active->setValue(false);def.driveSpeed->setValue(90);def.duration->setValue(3);
            {auto ui=std::unique_ptr<InspectableEditor>(def.getEditor(false));snapshot(*ui,path.getParentDirectory().getParentDirectory().getChildFile("evidence/integration-curve.png"));}

            auto* linkState=state("06 One master with two followers");auto* linkAction=action(linkState,"Start Link on state entry");activation(linkAction,true);
            auto* linkHandler=consequence(linkAction,module,"Link");auto* link=cmd(linkHandler);
            link->behaviour->master->select("cupboard-a");link->behaviour->followerChoices.at("cupboard-b")->setValue(true);link->behaviour->followerChoices.at("follower-c")->setValue(true);
            auto* endLink=consequence(action(linkState,"Explicit End Link"),module,"End interaction");cmd(endLink)->target->setValueFromTarget(linkHandler);
            position("cupboard-a",20);linkState->active->setValue(true);
            waitFor([&]{auto s=shared->snapshot();return std::abs(s.motors["cupboard-b"].position-20)<.6&&std::abs(s.motors["follower-c"].position-20)<.6;},"native state entry starts one-master two-follower Link");
            linkState->active->setValue(false);pump(60);
            check(shared->snapshot().events[link->instanceId+"/0"].status==Status::Running,"leaving a state does not implicitly undo persistent consequence");
            cmd(endLink)->triggerInternal(0);check(!shared->snapshot().events.count(link->instanceId+"/0"),"End interaction addresses exact running command");

            auto* hapticState=state("07 Two-motor haptics with explicit state ending");auto* hapticAction=action(hapticState,"Start Haptic");activation(hapticAction,true);
            auto* hapticHandler=consequence(hapticAction,module,"Haptic");auto* haptic=cmd(hapticHandler);haptic->behaviour->master->select("cupboard-a");haptic->behaviour->motor->select("cupboard-b");
            auto* endHapticAction=action(hapticState,"End Haptic on state exit");activation(endHapticAction,false);auto* endHaptic=consequence(endHapticAction,module,"End interaction");cmd(endHaptic)->target->setValueFromTarget(hapticHandler);
            hapticState->active->setValue(true);waitFor([&]{auto s=shared->snapshot();return s.motors["cupboard-a"].torqueMode&&s.motors["cupboard-b"].enableSent;},"native consequence starts supported haptic pair");
            auto before=shared->snapshot().metrics.loops;Thread::sleep(120);check(shared->snapshot().metrics.loops>before+10,"communication and haptics continue without GUI activity");
            hapticState->active->setValue(false);waitFor([&]{return !shared->snapshot().events.count(haptic->instanceId+"/0");},"authored On Deactivate consequence ends haptics");

            // Boolean Mapping interaction outputs share command lifetime and do not restart on held TRUE.
            for(auto type:{String("Link"),String("Haptic")}){
                MappingOutput temporary;temporary.setCommand(module->getCommandDefinitionFor("",type));temporary.forceDisabled=false;
                auto* c=dynamic_cast<MidnightCommand*>(temporary.command.get());c->behaviour->master->select("cupboard-a");
                if(type=="Link")c->behaviour->followerChoices.at("cupboard-b")->setValue(true);else c->behaviour->motor->select("cupboard-b");
                temporary.setValue(var(Array<var>{true}),0);
                waitFor([&]{return shared->snapshot().events[c->instanceId+"/0"].status==Status::Running;},"boolean Mapping output starts native interaction");
                auto started=shared->snapshot().events[c->instanceId+"/0"].started;
                for(int i=0;i<10;++i)temporary.setValue(var(Array<var>{true}),0);
                check(shared->snapshot().events[c->instanceId+"/0"].started==started,"held TRUE Mapping does not reinitialize interaction");
                temporary.setValue(var(Array<var>{false}),0);
                check(!shared->snapshot().events.count(c->instanceId+"/0"),"FALSE Mapping ends the intended interaction");
            }

            auto* mappingState=state("08 Encoder Mapping to absolute target");auto* mapping=new Mapping();mappingState->pm->addItem(mapping);
            auto* output=new MappingOutput();mapping->om.addItem(output);output->setCommand(module->getCommandDefinitionFor("","Move to position"));
            auto* mapCommand=dynamic_cast<MidnightCommand*>(output->command.get());mapCommand->behaviour->motor->select("cupboard-b");
            mapping->enabled->setValue(false);output->forceDisabled=false;output->setValue(var(Array<var>{23.0}),0);
            waitFor([&]{return shared->snapshot().motors["cupboard-b"].requested==23;},"native Mapping output sends absolute angle through shared service");
            for(int i=0;i<40;++i)output->setValue(var(Array<var>{double(i)}),0);
            waitFor([&]{return std::abs(shared->snapshot().motors["cupboard-b"].requested-39)<.01;},"rapid Mapping replacements retain initial setup and latest target");
            mapCommand->endInteractions();
            auto* input=new StandardMappingInput();mapping->im.addItem(input);module->refreshValues();
            auto* encoderValues=module->valuesCC.getControllableContainerByName("motorcupboarda");
            input->inputTarget->setValueFromTarget(encoderValues->getControllableByName("Measured angle",true));
            mapping->enabled->setValue(true);mappingState->active->setValue(true);
            position("cupboard-a",27);
            waitFor([&]{return std::abs(shared->snapshot().motors["cupboard-b"].requested-27)<.1;},"measured encoder drives real Mapping input/filter/output chain");
            mappingState->active->setValue(false);mapCommand->endInteractions();
            auto* mappingEnd=consequence(action(mappingState,"End mapping move"),module,"End interaction");cmd(mappingEnd)->target->setValueFromTarget(output);
            auto* groupState=state("09 Group activation and shared configuration");
            auto* groupAction=action(groupState,"Grouped Cupboard button");auto* group=new ConditionGroup();groupAction->cdm->addItem(group);
            auto* grouped=new CupboardButtonCondition();grouped->motorChoice->select("cupboard-a");group->manager.addItem(grouped);
            position("cupboard-a",20);groupState->active->setValue(true);grouped->activate();group->enabled->setValue(false);
            check(!shared->snapshot().events.count(grouped->interactionId),"disabling a group ends nested button response");
            group->enabled->setValue(true);waitFor([&]{return shared->snapshot().events.count(grouped->interactionId)!=0;},"reenabling group deliberately reactivates nested button");
            group->manager.removeItem(grouped);check(!shared->snapshot().motors["cupboard-a"].torqueMode,"deletion ends nested interaction");groupState->active->setValue(false);
            auto* exampleGrouped=new CupboardButtonCondition();exampleGrouped->motorChoice->select("cupboard-a");exampleGrouped->referenceAngle->setValue(20);exampleGrouped->triggerOffset->setValue(-5);group->manager.addItem(exampleGrouped);
            auto* controls=state("10 End persistent interactions");
            auto* persistentEnd=consequence(action(controls,"End persistent Link"),module,"End interaction");cmd(persistentEnd)->target->setValueFromTarget(linkHandler);
            auto* persistentMappingEnd=consequence(action(controls,"End persistent Mapping Move"),module,"End interaction");cmd(persistentMappingEnd)->target->setValueFromTarget(output);
            linkState->active->setValue(true);linkState->active->setValue(false);controls->active->setValue(true);
            check(shared->snapshot().events[link->instanceId+"/0"].status==Status::Running,"separate control state does not implicitly end persistent Link");
            persistentEnd->triggerCommand();
            check(!shared->snapshot().events.count(link->instanceId+"/0"),"enabled control state can end a command from an inactive state");
            controls->active->setValue(false);

            {
                auto first=createMotorEditor("cupboard-a"),second=createMotorEditor("cupboard-a");
                first->setSize(800,900);second->setSize(800,900);
                auto* name=findControl<TextEditor>(*first,"Motor name");auto* mirrored=findControl<TextEditor>(*second,"Motor name");
                check(name&&mirrored,"both shared hardware editor instances expose motor name");
                name->setText("Renamed cupboard");findButton(*first,"Save motor settings")->onClick();
                waitFor([&]{return mirrored->getText()=="Renamed cupboard";},"hardware editor save appears in the other open editor");
                auto changed=shared->snapshot().document;changed.motors[0].speed=44;shared->edit(changed);
                auto* speed=findControl<TextEditor>(*first,"Speed limit / degrees per second");
                // Source field label verified below; absence is a test error.
                check(speed!=nullptr,"shared speed control exists");
                waitFor([&]{return std::abs(speed->getText().getDoubleValue()-44)<.001;},"shared hardware speed change updates the open editor");
            }
            module->refreshValues();b->motorChoice->refresh();
            check(b->motorChoice->motorId()=="cupboard-a","hardware rename preserves condition identity");
            auto* values=module->valuesCC.getControllableContainerByName("motorcupboarda");
            check(values&&values->niceName=="Renamed cupboard","shared names update native module values");
            {auto ui=std::unique_ptr<InspectableEditor>(module->getEditor(false));snapshot(*ui,path.getParentDirectory().getParentDirectory().getChildFile("evidence/integration-hardware.png"));}
            shared->action([](Runtime& r){r.stopAll();});
            for(auto* s:StateManager::getInstance()->items)s->active->setValue(false);
            save(app,path);auto saved=JSON::parse(path);
            check(saved["midnightNative"]["events"].size()==0,"per-use running instances are not duplicated in hardware persistence");
            saved["states"].getDynamicObject()->setProperty("viewZoom",.4);
            saved.getDynamicObject()->setProperty("layout",JSON::parse(String::fromUTF8(BinaryData::default_chalayout,BinaryData::default_chalayoutSize)));
            auto examples=path.getParentDirectory().getParentDirectory().getChildFile("examples/Midnight_Chataigne_Integration.noisette");examples.replaceWithText(JSON::toString(saved));
        }
        shared->action([](Runtime& r){r.stopAll();});app.mainComponent->clear();std::cout<<"SUCCESS "<<checks<<" integration checks in "<<argv[1]<<std::endl;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;code=1;}
    app.mainComponent.reset();app.engine.reset();return code;
}
