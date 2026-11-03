#include "MainIncludes.h"
#include "NativePanel.h"
#include <sstream>

namespace midnightNative {
namespace {
const Colour background(0xff111a24),box(0xff1c2b39),inner(0xff273d4a),accent(0xff65dfc4),muted(0xffa8bdc9);
String labelFor(const Document& d,const Id& id){if(id.empty())return "None";for(auto& m:d.motors)if(m.id==id)return String(m.name);for(auto& e:d.events)if(e.id==id)return String(e.name);return "Missing: "+String(id);}
String kindName(Kind k){switch(k){case Kind::Move:return "Movement";case Kind::Link:return "Master / followers";case Kind::Haptic:return "Haptic pair";case Kind::DMX:return "DMX / Art-Net";default:return "Timeline";}}
String ruleText(const Document& d,const Rule& r){if(r.edge==Edge::Manual)return "Manual GO";auto name=labelFor(d,r.source);String text;
    if(r.edge==Edge::Position)text=name+" crosses "+String(r.threshold,1)+" deg "+(r.direction>0?"up":r.direction<0?"down":"either way");
    else text=name+(r.edge==Edge::Started?" starts":r.edge==Edge::Stopped?" stops":r.edge==Edge::Completed?" completes":" is superseded");
    if(r.delay>0)text+=" + "+String(r.delay,2)+" s";return text;
}
String rulesText(const Document& d,const Event& e){if(e.in.empty())return e.parent.empty()?"Manual GO":"At timeline offset";StringArray a;for(auto& r:e.in)a.add(ruleText(d,r));return a.joinIntoString(e.all?" AND ":" OR ");}
double numeric(const TextEditor& edit){auto s=edit.getText().trim().toStdString();size_t n=0;double v=std::stod(s,&n);require(n==s.size(),"Use a finite number with a decimal point");finite(v);return v;}
int integer(const TextEditor& edit){double v=numeric(edit);require(v==std::floor(v)&&v>=std::numeric_limits<int>::min()&&v<=std::numeric_limits<int>::max(),"Use a whole number for an address, channel or bitrate");return int(v);}
void showWindow(const String& title,Component* content,int width,int height){content->setSize(width,height);DialogWindow::LaunchOptions o;o.dialogTitle=title;o.dialogBackgroundColour=background;o.content.setOwned(content);o.useNativeTitleBar=true;o.resizable=true;o.launchAsync();}
class Form : public Component {
public:
    struct Row {String name;Component* component;int height;};
    OwnedArray<Component> owned;std::vector<Row> rows;Colour fillColour=box;
    TextEditor& field(const String& title,const String& text){auto* c=new TextEditor();c->setText(text);c->setName(title);c->setColour(TextEditor::backgroundColourId,background);c->setColour(TextEditor::textColourId,Colours::white);c->applyColourToAllText(Colours::white);own(title,c,30);return *c;}
    TextEditor& number(const String& title,double value){return field(title,String(value,4).trimCharactersAtEnd("0").trimCharactersAtEnd("."));}
    ComboBox& choice(const String& title,const StringArray& items,int selected=1){auto* c=new ComboBox();c->addItemList(items,1);c->setSelectedId(selected,dontSendNotification);c->setName(title);own(title,c,30);return *c;}
    ToggleButton& toggle(const String& title,bool value){auto* c=new ToggleButton(title);c->setToggleState(value,dontSendNotification);own({},c,28);return *c;}
    TextButton& button(const String& title,std::function<void()> fn){auto* c=new TextButton(title);c->onClick=std::move(fn);own({},c,30);return *c;}
    Label& note(const String& text,int height=35){auto* c=new Label();c->setText(text,dontSendNotification);c->setColour(Label::textColourId,muted);c->setMinimumHorizontalScale(.8f);own({},c,height);return *c;}
    void heading(const String& title){auto& l=note(title,36);l.setFont(Font(17,Font::bold));l.setColour(Label::textColourId,accent);}
    void own(const String& title,Component* c,int height){owned.add(c);rows.push_back({title,c,height});addAndMakeVisible(c);}
    int preferredHeight() const{int n=20;for(auto& r:rows)n+=r.height+6;return n;}
    void resized() override {auto area=getLocalBounds().reduced(10);for(auto& r:rows){auto line=area.removeFromTop(r.height);area.removeFromTop(6);if(r.name.isNotEmpty())line.removeFromLeft(jmin(240,getWidth()/3));r.component->setBounds(line);}}
    void paint(Graphics& g) override {g.fillAll(fillColour);g.setColour(muted);g.setFont(13);auto area=getLocalBounds().reduced(10);for(auto& r:rows){auto line=area.removeFromTop(r.height);area.removeFromTop(6);if(r.component->isVisible()&&r.name.isNotEmpty())g.drawText(r.name,line.removeFromLeft(jmin(236,getWidth()/3)),Justification::centredLeft);}}
};
class ScrollForm : public Component {
public: Viewport view;Form form;ScrollForm(){addAndMakeVisible(view);view.setViewedComponent(&form,false);view.setScrollBarsShown(true,false);}void resized() override{view.setBounds(getLocalBounds());form.setSize(jmax(200,view.getWidth()-16),form.preferredHeight());}
};
class MotorEditor final : public ScrollForm,private Timer {
    Document doc;Motor motor;Adapter adapter;bool saved=false;std::vector<Port> ports;
    ComboBox *shared,*model,*portChoice,*direction;TextButton *enableButton,*haltButton;Label *profileNote,*zeroNote;TextEditor *name,*port,*baud,*bitrate,*can,*host,*minimum,*maximum,*speed,*current,*target,*jog;
    ToggleButton *remember;Label *status,*identity,*error;Id selectedAdapter;
    void safe(const std::function<void()>& f){try{error->setText({},dontSendNotification);f();}catch(const std::exception& e){error->setText(e.what(),dontSendNotification);}}
    void loadAdapter(){int index=shared->getSelectedId();selectedAdapter=index>=3?doc.adapters[size_t(index-3)].id:Id{};
        adapter=index>=3?doc.adapters[size_t(index-3)]:Adapter{};if(adapter.id.empty())adapter.id=uuid();if(index<3)adapter.simulated=index==2;
        port->setText(String(adapter.port));baud->setText(String(adapter.baud));bitrate->setText(String(adapter.bitrate));remember->setToggleState(adapter.reconnect,dontSendNotification);identity->setText("Remembered identity: "+String(adapter.identity.empty()?"manual port assignment":adapter.identity),dontSendNotification);
    }
    void save(){auto fresh=service()->snapshot().document;motor.name=name->getText().toStdString();motor.model=model->getText().toStdString();motor.protocol=defaultProtocol(motor.model);motor.can=integer(*can);if(motor.model!="RMD-L-7025")motor.host=integer(*host);motor.direction=direction->getSelectedId()==2?-1:1;motor.min=numeric(*minimum);motor.max=numeric(*maximum);motor.speed=numeric(*speed);if(motor.model!="RMD-L-7025")motor.current=numeric(*current);
        adapter.port=port->getText().trim().toStdString();adapter.baud=integer(*baud);adapter.bitrate=integer(*bitrate);adapter.reconnect=remember->getToggleState();adapter.name=adapter.simulated?"Simulated CANable":adapter.port+" CANable";
        if(adapter.port!=loadedPort) {adapter.identity.clear();for(auto& p:ports)if(p.port==adapter.port)adapter.identity=p.identity;}
        motor.adapter=adapter.id;bool found=false;for(auto& a:fresh.adapters)if(a.id==adapter.id){a=adapter;found=true;}if(!found)fresh.adapters.push_back(adapter);found=false;for(auto& m:fresh.motors)if(m.id==motor.id){m=motor;found=true;}if(!found)fresh.motors.push_back(motor);
        service()->edit(fresh);saved=true;loadedPort=adapter.port;
    }
    void capabilities(){bool rmd=model->getText()=="RMD-L-7025";host->setVisible(!rmd);current->setVisible(!rmd);enableButton->setEnabled(!rmd);haltButton->setVisible(rmd);
        profileNote->setText(rmd?"Profile: MyActuator V3 controller / protocol V4.2 (250208). Standard TX 0x140+ID, RX 0x240+ID; device ID 1..32; 1 Mbps bus. Installed firmware unverified. No host ID/current-limit control/haptics. A deliberate Move starts control; separate Enable/0x88 unsupported. Speed: whole deg/s; 0 removes the drive speed limit.":"Profile: "+String(defaultProtocol(model->getText().toStdString()))+". Extended private CAN. RS01 parameters: 44 rad/s, 23 A; RS02: 33 rad/s, 16 A; RS04: 20 rad/s, 90 A. Ranges are not motor ratings. Saved current/speed are never raised by model selection.",dontSendNotification);
        zeroNote->setText(rmd?"Explicit zero sends 0x64: writes current encoder position to ROM. Restart the controller separately to apply. No automatic restart or local position reset. Persistence requires physical verification.":"Explicit zero writes controller Type 6 while stopped. Power-cycle persistence and installed firmware behavior remain unverified. No zero is sent on connect/save/load.",dontSendNotification);form.repaint();
    }
    std::string loadedPort;
    void timerCallback() override {if(!service())return;auto s=service()->snapshot();auto it=s.motors.find(motor.id);if(it==s.motors.end())return;auto& f=it->second;if(!s.error.empty())error->setText("Runtime: "+String(s.error),dontSendNotification);
        status->setText((adapter.simulated?"SIMULATED | ":"PHYSICAL | ")+String(s.adapters[motor.adapter])+"\nMeasured mechanical: "+(f.at<0?String("unavailable"):String(f.position,2)+" deg | age "+String((monotonic()-f.at)*1000,0)+" ms")+" | Requested: "+(f.requestedAt<0?String("not sent"):String(f.requested,2)+" deg")+"\n"+String(f.message)+" | Drive mode: "+(f.observedMode<0?String("unavailable"):String(f.observedMode))+" | Fault report: "+(f.statusAt<0?String("unavailable"):String(int(f.faults|f.reportFaults))),dontSendNotification);
    }
public:
    MotorEditor(Id id){doc=service()->snapshot().document;motor.id=uuid();if(!id.empty())for(auto& m:doc.motors)if(m.id==id){motor=m;saved=true;}
        form.heading(saved?"Motor settings + direct test":"Add Motor");form.note("CANable detection finds adapters, not motor models or motor CAN IDs. Assign the verified model and address.",42);
        name=&form.field("Motor name",String(motor.name));model=&form.choice("Motor model",{"RS01","RS02","RS04","RMD-L-7025"},motor.model=="RS01"?1:motor.model=="RS04"?3:motor.model=="RMD-L-7025"?4:2);
        direction=&form.choice("Coordinate direction",{"+1","-1 (inverted)"},motor.direction==1?1:2);
        StringArray options{"New CANable / physical","New simulated adapter"};int selected=1;for(size_t i=0;i<doc.adapters.size();++i){options.add("Reuse "+String(doc.adapters[i].name));if(doc.adapters[i].id==motor.adapter)selected=int(i)+3;}
        shared=&form.choice("Shared adapter",options,selected);portChoice=&form.choice("Detected serial adapters",{"Scan to discover"});
        form.button("Scan serial adapters",[this]{safe([&]{ports=discoverPorts();portChoice->clear();for(size_t i=0;i<ports.size();++i)portChoice->addItem(String(ports[i].port)+" | "+String(ports[i].description)+" | "+String(ports[i].hardware),int(i)+1);if(ports.empty())portChoice->setTextWhenNothingSelected("No serial adapters found");});});
        port=&form.field("COM port (manual selection allowed)",{});baud=&form.number("Serial baud",2000000);bitrate=&form.number("CAN bitrate",1000000);identity=&form.note({},40);remember=&form.toggle("Reconnect this adapter for feedback on project load",true);
        can=&form.number("Motor CAN ID (assignment only)",motor.can);host=&form.number("Host CAN ID",motor.host);
        minimum=&form.number("MIN target / mechanical degrees",motor.min);form.button("MIN: Capture Current Position",[this]{safe([&]{auto f=service()->snapshot().motors.at(motor.id);require(f.at>=0&&monotonic()-f.at<.15,"Fresh feedback unavailable");minimum->setText(String(f.position,3));});});
        maximum=&form.number("MAX target / mechanical degrees",motor.max);form.button("MAX: Capture Current Position",[this]{safe([&]{auto f=service()->snapshot().motors.at(motor.id);require(f.at>=0&&monotonic()-f.at<.15,"Fresh feedback unavailable");maximum->setText(String(f.position,3));});});
        speed=&form.number("Speed limit / degrees per second",motor.speed);current=&form.number("Current-limit parameter / A",motor.current);
        profileNote=&form.note({},100);form.note("MIN/MAX and events use the displayed coordinate direction. Shared adapter edits affect all its motors; model selection does not change the bus settings. No hardware IDs are changed.",45);
        form.button("Save motor settings",[this]{safe([&]{save();});});form.button("Save + Connect for feedback",[this]{safe([&]{save();service()->connect(adapter.id);});});
        form.button("Disconnect adapter",[this]{safe([&]{service()->connect(motor.adapter,false);});});
        form.heading("Direct testing - independent of events");status=&form.note("Save and connect to observe the configured motor",80);
        target=&form.number("Target / mechanical degrees",45);jog=&form.number("Jog step / degrees",1);
        form.button("Move to Position",[this]{safe([&]{save();auto t=numeric(*target),v=numeric(*speed);auto id=motor.id;service()->action([=](Runtime& r){r.move(id,t,v);});});});
        for(int sign:{-1,1})form.button(sign<0?"Jog -":"Jog +",[this,sign]{safe([&]{save();auto f=service()->snapshot().motors.at(motor.id);require(f.at>=0&&monotonic()-f.at<.15,"Fresh measured position unavailable");auto t=f.position+sign*numeric(*jog),v=numeric(*speed);auto id=motor.id;service()->action([=](Runtime& r){r.move(id,t,v);});});});
        enableButton=&form.button("Enable",[this]{safe([&]{save();auto id=motor.id;service()->action([=](Runtime& r){r.enable(id);});});});
        haltButton=&form.button("MyActuator Stop (retain closed-loop state)",[this]{auto id=motor.id;service()->action([=](Runtime& r){r.haltMotor(id);});});
        form.button("Stop / Disable",[this]{auto id=motor.id;service()->action([=](Runtime& r){r.stopMotor(id);});});
        form.button("Set Mechanical 0 degrees at Current Position",[this]{safe([&]{require(saved,"Save motor first");save();auto id=motor.id;service()->action([=](Runtime& r){r.mechanicalZero(id);});});});
        zeroNote=&form.note({},65);
        form.button("Simulation only: set measured position to Target",[this]{safe([&]{service()->injectPosition(motor.id,numeric(*target));});});
        error=&form.note({},45);error->setName("Motor action status");error->setColour(Label::textColourId,Colours::orange);
        model->onChange=[this]{capabilities();};capabilities();loadAdapter();loadedPort=adapter.port;
        shared->onChange=[this]{loadAdapter();loadedPort=adapter.port;};
        portChoice->onChange=[this]{int i=portChoice->getSelectedId()-1;if(i>=0&&i<int(ports.size())){port->setText(String(ports[size_t(i)].port));adapter.identity=ports[size_t(i)].identity;identity->setText("Device instance: "+String(adapter.identity),dontSendNotification);}};
        startTimerHz(10);
    }
};
class HardwareSetup final : public ScrollForm,private Timer {
    ComboBox* motors;Label* feedback;std::vector<Id> ids;
    void timerCallback() override{auto s=service()->snapshot();String text;for(auto& m:s.document.motors){auto f=s.motors[m.id];text+=String(m.name)+" | "+String(m.model)+" ID "+String(m.can)+" | "+(f.at>=0?String(f.position,2)+" deg":"no mechanical feedback")+"\n";}feedback->setText(text,dontSendNotification);}
public:
    HardwareSetup(){form.heading("Hardware Setup");form.note("Configure motors and test them here. Every motor on one CANable shares its transport and receiver.",45);
        auto s=service()->snapshot();StringArray names;for(auto& m:s.document.motors){names.add(String(m.name));ids.push_back(m.id);}motors=&form.choice("Saved motor",names);
        form.button("Add Motor",[]{showWindow("Add Motor",new MotorEditor({}),820,850);});form.button("Edit selected motor + test",[this]{int i=motors->getSelectedId()-1;if(i>=0&&i<int(ids.size()))showWindow("Motor settings",new MotorEditor(ids[size_t(i)]),820,850);});
        form.button("Refresh saved motor list",[this]{auto s=service()->snapshot();ids.clear();motors->clear();for(auto& m:s.document.motors){ids.push_back(m.id);motors->addItem(String(m.name),int(ids.size()));}motors->setSelectedId(1);});
        form.heading("DMX output - existing Chataigne Art-Net module");auto* host=&form.field("Destination IP",String(s.document.dmxHost));auto* port=&form.number("UDP port",s.document.dmxPort);auto* universe=&form.number("Art-Net port-address / universe (0-based)",s.document.dmxUniverse);
        auto* err=&form.note({},35);
        form.button("Save DMX output configuration",[host,port,universe,err]{try{auto d=service()->snapshot().document;d.dmxHost=host->getText().toStdString();d.dmxPort=integer(*port);d.dmxUniverse=integer(*universe);service()->edit(d);err->setText("Saved. Output begins only when a DMX event executes.",dontSendNotification);}catch(const std::exception& e){err->setText(e.what(),dontSendNotification);}});
        form.note("V0.1 exposes Art-Net over UDP. An Art-Net node can provide physical DMX. Compatibility with unspecified USB-DMX hardware is not claimed.",45);
        feedback=&form.note({},180);startTimerHz(5);
    }
};
class RuleRow : public Form {
public:
    ComboBox *edge,*source,*direction;TextEditor *threshold,*delay;std::vector<Id> sources;Rule initial;
    RuleRow(const Document& d,Rule r):initial(r){edge=&choice("When",{"Manual GO","Another event starts","Another event stops","Another event completes","Another event is superseded","Motor position crosses"},int(r.edge)+1);source=&choice("Source",{});threshold=&number("Crossing / mechanical degrees",r.threshold);direction=&choice("Crossing direction",{"Increasing","Decreasing","Either"},r.direction==1?1:r.direction==-1?2:3);delay=&number("Delay after condition / seconds",r.delay);
        auto refresh=[this,d]{source->clear();sources.clear();bool pos=edge->getSelectedId()==6;source->setEnabled(edge->getSelectedId()!=1);threshold->setVisible(pos);direction->setVisible(pos);int selected=0;if(pos){for(auto& m:d.motors){sources.push_back(m.id);source->addItem(String(m.name),int(sources.size()));if(m.id==initial.source)selected=int(sources.size());}}else for(auto& e:d.events){sources.push_back(e.id);source->addItem(String(e.name),int(sources.size()));if(e.id==initial.source)selected=int(sources.size());}source->setSelectedId(selected?selected:1,dontSendNotification);repaint();};edge->onChange=refresh;refresh();
    }
    Rule value(){Rule r;r.edge=Edge(edge->getSelectedId()-1);int i=source->getSelectedId()-1;if(i>=0&&i<int(sources.size()))r.source=sources[size_t(i)];r.threshold=numeric(*threshold);r.delay=numeric(*delay);r.direction=direction->getSelectedId()==1?1:direction->getSelectedId()==2?-1:0;if(r.edge==Edge::Manual)r.source.clear();return r;}
};
class EventEditor final : public ScrollForm {
    Event working;Document doc;Id id;Form content;Form incoming;Form outgoing;
    TextEditor *name,*start,*duration;ComboBox *kind,*input,*output;ToggleButton *all,*stopAll,*repeat;
    std::vector<Id> motorIds;std::map<String,TextEditor*> numbers;ToggleButton *keep=nullptr,*align=nullptr;ComboBox *signA=nullptr,*signB=nullptr;std::vector<std::pair<Id,ToggleButton*>> followers;
    OwnedArray<RuleRow> inRows,outRows;Label* error;
    void safe(const std::function<void()>& fn){try{error->setText({},dontSendNotification);fn();}catch(const std::exception& e){error->setText(e.what(),dontSendNotification);}}
    void buildContent(){content.owned.clear();content.rows.clear();numbers.clear();followers.clear();keep=align=nullptr;input=output=nullptr;signA=signB=nullptr;
        content.heading("EVENT WINDOW / WHAT happens - "+kindName(working.kind));
        StringArray names{"None"};motorIds.clear();motorIds.push_back({});for(auto& m:doc.motors){names.add(String(m.name));motorIds.push_back(m.id);}
        auto index=[&](const Id& key){for(size_t i=0;i<motorIds.size();++i)if(motorIds[i]==key)return int(i)+1;return 1;};
        if(working.kind==Kind::Move||working.kind==Kind::Link||working.kind==Kind::Haptic){input=&content.choice("Device INPUT",names,index(working.input));if(working.kind==Kind::Move)input->setSelectedId(1,dontSendNotification);
            if(working.kind!=Kind::Link)output=&content.choice("Device OUTPUT",names,index(working.outputs.empty()?Id{}:working.outputs[0]));
            if(output)output->onChange=[this]{int i=output->getSelectedId()-1;working.outputs=i>0?std::vector<Id>{motorIds[size_t(i)]}:std::vector<Id>{};};
            input->onChange=[this]{working.input=motorIds[size_t(input->getSelectedId()-1)];if(working.kind==Kind::Move&&!working.input.empty()){working.kind=Kind::Link;kind->setSelectedId(2,dontSendNotification);buildContent();resizeForm();}else if(working.kind==Kind::Link&&working.input.empty()){working.kind=Kind::Move;if(working.outputs.size()>1)working.outputs.resize(1);kind->setSelectedId(1,dontSendNotification);buildContent();resizeForm();}};
        }
        auto num=[&](const String& label,double v){numbers[label]=&content.number(label,v);};
        if(working.kind==Kind::Move){num("Target / mechanical degrees",working.target);num("Speed limit / degrees per second",working.speed);keep=&content.toggle("Keep motor active after completion (unchecked: Disable afterward)",working.keepActive);content.note("Arrival uses fresh measured feedback and the motor's saved tolerance. Position/speed has no guaranteed duration. Curve execution is unavailable in V0.1.",48);}
        if(working.kind==Kind::Link){content.note("One master -> one or more followers, copying mechanical degrees 1:1. Each follower uses its saved speed/current.",40);for(auto& m:doc.motors)followers.push_back({m.id,&content.toggle(String(m.name),std::find(working.outputs.begin(),working.outputs.end(),m.id)!=working.outputs.end())});num("Target-change deadband / degrees",working.deadband);}
        if(working.kind==Kind::Haptic){content.note("Exactly two distinct RobStride motors (RS01/RS02/RS04). MyActuator and cross-brand haptics unsupported. Bidirectional reference spring/damper coupling; no multi-follower haptics.",40);num("Stiffness K / Nm per rad",working.stiffness);num("Damping D / Nm per rad/s",working.damping);num("Maximum torque / Nm",working.torqueLimit);num("Spring deadband / degrees",working.deadband);signA=&content.choice("Input axis sign",{"+1","-1"},working.signA==1?1:2);signB=&content.choice("Output axis sign",{"+1","-1"},working.signB==1?1:2);align=&content.toggle("Align relative positions on activation (does not write mechanical zero)",working.align);num("Explicit relative offset / degrees",working.offset);}
        if(working.kind==Kind::DMX){content.note("Output: configured Chataigne Art-Net device. Configure its destination in Hardware Setup.",40);num("DMX channel / 1..512",working.channel);num("DMX value / 0..255",working.value);num("Fade / seconds",working.fade);}
        if(working.kind==Kind::Timeline)content.note("This box contains a graphical timeline. Close the editor and double-click the timeline box to enter it; add events or another timeline there.",70);
    }
    void resizeForm(){for(auto& r:form.rows){if(r.component==&content)r.height=content.preferredHeight();if(r.component==&incoming)r.height=incoming.preferredHeight();if(r.component==&outgoing)r.height=outgoing.preferredHeight();}resized();}
    void addRule(bool stop,Rule r={}){
        auto* row=new RuleRow(doc,r);(stop?outRows:inRows).add(row);auto& dest=stop?outgoing:incoming;
        dest.rows.push_back({{},row,row->preferredHeight()});dest.addAndMakeVisible(row);
        auto* button=&dest.button("Remove this condition",{});
        Component::SafePointer<EventEditor> editor(this);
        button->onClick=[editor,row,stop,button]{
            // Remove both rows after this button's callback has returned.
            MessageManager::callAsync([editor,row,stop,button]{
                if(!editor)return;auto& list=stop?editor->outRows:editor->inRows;if(!list.contains(row))return;
                auto& f=stop?editor->outgoing:editor->incoming;
                f.rows.erase(std::remove_if(f.rows.begin(),f.rows.end(),[&](const Form::Row& x){return x.component==row||x.component==button;}),f.rows.end());
                list.removeObject(row);f.owned.removeObject(button);editor->resizeForm();
            });
        };resizeForm();
    }
    Event collect(){auto e=working;e.name=name->getText().toStdString();e.start=numeric(*start);e.duration=numeric(*duration);e.in.clear();e.out.clear();for(auto* row:inRows)e.in.push_back(row->value());for(auto* row:outRows)e.out.push_back(row->value());e.all=all->getToggleState();e.stopAll=stopAll?stopAll->getToggleState():false;e.repeat=repeat->getToggleState();
        if(input)e.input=motorIds[size_t(input->getSelectedId()-1)];if(output){int n=output->getSelectedId()-1;e.outputs=n>0?std::vector<Id>{motorIds[size_t(n)]}:std::vector<Id>{};}
        if(e.kind==Kind::Move){e.input.clear();e.target=numeric(*numbers.at("Target / mechanical degrees"));e.speed=numeric(*numbers.at("Speed limit / degrees per second"));e.keepActive=keep->getToggleState();}
        if(e.kind==Kind::Link){e.outputs.clear();for(auto& f:followers)if(f.second->getToggleState())e.outputs.push_back(f.first);e.deadband=numeric(*numbers.at("Target-change deadband / degrees"));}
        if(e.kind==Kind::Haptic){e.stiffness=numeric(*numbers.at("Stiffness K / Nm per rad"));e.damping=numeric(*numbers.at("Damping D / Nm per rad/s"));e.torqueLimit=numeric(*numbers.at("Maximum torque / Nm"));e.deadband=numeric(*numbers.at("Spring deadband / degrees"));e.offset=numeric(*numbers.at("Explicit relative offset / degrees"));e.signA=signA->getSelectedId()==1?1:-1;e.signB=signB->getSelectedId()==1?1:-1;e.align=align->getToggleState();}
        if(e.kind==Kind::DMX){e.input.clear();e.outputs.clear();e.channel=integer(*numbers.at("DMX channel / 1..512"));e.value=integer(*numbers.at("DMX value / 0..255"));e.fade=numeric(*numbers.at("Fade / seconds"));}
        if(e.kind==Kind::Timeline){e.input.clear();e.outputs.clear();}return e;
    }
public:
    explicit EventEditor(const Id& key):id(key){content.fillColour=inner;doc=service()->snapshot().document;for(auto& e:doc.events)if(e.id==key)working=e;
        form.heading("RELATIONSHIP BOX / WHEN this event activates");name=&form.field("Event name",String(working.name));kind=&form.choice("Event content",{"Movement","Master / followers","Haptic pair","DMX / Art-Net","Timeline"},int(working.kind)+1);
        start=&form.number("Timeline offset / earliest eligibility (s)",working.start);duration=&form.number(working.kind==Kind::Timeline?"Timeline duration / seconds":"Planning width / seconds (not arrival)",working.duration);
        form.note("Timeline time makes a child eligible. Explicit Manual GO participates in ALL/ANY and delays. Without a Manual GO rule, GO starts the selected box directly.",45);
        if(!working.copiedExternalReferences.empty()){StringArray names;for(auto& ref:working.copiedExternalReferences)names.add(labelFor(doc,ref));form.note("Copied event retains these external condition references: "+names.joinIntoString(", "),42);}
        incoming.heading(working.kind==Kind::Haptic||working.kind==Kind::Link?"IN / Activate when...":"IN / Start when...");all=&incoming.toggle("ALL conditions (unchecked: ANY)",working.all);incoming.button("Add IN condition",[this]{addRule(false);});
        form.rows.push_back({{},&incoming,100});form.addAndMakeVisible(incoming);
        for(auto& r:working.in)addRule(false,r);
        form.rows.push_back({{},&content,100});form.addAndMakeVisible(content);buildContent();
        outgoing.heading(working.kind==Kind::Haptic||working.kind==Kind::Link?"OUT / Deactivate when...":"OUT / Continue when...");
        stopAll=&outgoing.toggle("ALL deactivation conditions (unchecked: ANY)",working.stopAll);
        outgoing.button("Add condition that stops this event",[this]{addRule(true);});for(auto& r:working.out)addRule(true,r);
        outgoing.note("Outgoing relationships are stored in the destination IN box, once. Edit the destination to change starts / stops / completion and ALL / ANY.",45);
        for(auto& destination:doc.events)for(auto& r:destination.in)if(r.source==id&&r.edge!=Edge::Position)outgoing.note("-> "+String(destination.name)+": "+ruleText(doc,r),30);
        StringArray names;std::vector<Id> destinations;for(auto& e:doc.events)if(e.id!=id){destinations.push_back(e.id);names.add(String(e.name));}
        auto* dest=&outgoing.choice("Continue to event",names);auto* edge=&outgoing.choice("Continue when this event...",{"Completes normally","Starts","Stops","Is superseded"});
        outgoing.button("Add outgoing relationship",[this,dest,edge,destinations]{safe([&]{int i=dest->getSelectedId()-1;require(i>=0&&i<int(destinations.size()),"Select a destination event");auto d=service()->snapshot().document;for(auto& e:d.events)if(e.id==destinations[size_t(i)]){Edge edges[]{Edge::Completed,Edge::Started,Edge::Stopped,Edge::Superseded};e.in.push_back({edges[edge->getSelectedId()-1],id,10,0,1});}service()->edit(d);});});
        form.rows.push_back({{},&outgoing,outgoing.preferredHeight()});form.addAndMakeVisible(outgoing);
        repeat=&form.toggle("Repeat after completion; position conditions require a new crossing",working.repeat);
        error=&form.note({},45);error->setColour(Label::textColourId,Colours::orange);
        form.button("Apply event",[this]{safe([&]{auto e=collect();auto d=service()->snapshot().document;for(auto& existing:d.events)if(existing.id==id)existing=e;service()->edit(d);working=e;error->setText("Saved to project. GO is a separate deliberate action.",dontSendNotification);});});
        kind->onChange=[this]{working.kind=Kind(kind->getSelectedId()-1);buildContent();resizeForm();};resizeForm();
    }
};
class Monitor final : public Component,private Timer {
    TextEditor log;Label metrics;
public:Monitor(){addAndMakeVisible(log);addAndMakeVisible(metrics);log.setMultiLine(true);log.setReadOnly(true);log.setFont(Font(Font::getDefaultMonospacedFontName(),12,Font::plain));startTimerHz(4);}void resized()override{metrics.setBounds(10,8,getWidth()-20,64);log.setBounds(10,80,getWidth()-20,getHeight()-90);}void timerCallback()override{auto s=service()->snapshot();StringArray lines;for(auto& line:s.monitor)lines.add(String(line));log.setText(lines.joinIntoString("\n"),false);log.moveCaretToEnd();auto& m=s.metrics;metrics.setText("Worker loop: mean "+String(m.intervalMean*1000,2)+" ms / max "+String(m.intervalMax*1000,2)+" ms | Samples "+String(int64(m.loops))+"\nQueue "+String(int(m.queue))+" / max "+String(int(m.queueMax))+" | Enqueue-to-serial-write max "+String(m.transmitDelayMax*1000,2)+" ms | Feedback max age "+String(m.feedbackAgeMax*1000,1)+" ms\nTX "+String(int64(m.tx))+" / RX "+String(int64(m.rx))+". TX means bytes written, not drive acceptance. Simulation is not hardware evidence.",dontSendNotification);}
};
}

class NativePanel::Canvas final : public Component {
    NativePanel& panel;std::map<Id,Rectangle<int>> bounds;Id dragging;Point<int> down;double initialStart=0,initialDuration=0;int initialLane=0;bool resizing=false;
public:
    double scale=55;
    explicit Canvas(NativePanel& p):panel(p){}
    void paint(Graphics& g)override{
        g.fillAll(background);bounds.clear();auto& d=panel.current.document;
        bool timeline=!panel.parent.empty();int index=0;
        if(timeline){g.setFont(12);for(int s=0;s<int(getWidth()/scale);++s){int x=30+int(s*scale);g.setColour(box.brighter());g.drawVerticalLine(x,36,float(getHeight()));g.setColour(muted);g.drawText(String(s)+" s",x+3,5,60,25,Justification::centredLeft);}}
        for(auto& e:d.events)if(e.parent==panel.parent){Rectangle<int> r;
            if(timeline)r={30+int(e.start*scale),45+e.lane*182,jmax(230,int(e.duration*scale)),e.collapsed?55:166};
            else {int cols=jmax(1,getWidth()/390),w=(getWidth()-30)/cols;r={15+(index%cols)*w,15+(index/cols)*218,w-14,e.collapsed?55:200};}
            ++index;bounds[e.id]=r;bool selected=panel.selected==e.id;g.setColour(box);g.fillRoundedRectangle(r.toFloat(),8);g.setColour(selected?accent:muted.withAlpha(.35f));g.drawRoundedRectangle(r.toFloat(),8,selected?2.f:1.f);
            auto area=r.reduced(10);g.setColour(accent);g.setFont(Font(14,Font::bold));auto st=panel.current.events.find(e.id);String status=st==panel.current.events.end()?"Idle":statusName(st->second.status);g.drawText((e.collapsed?"+ ":"- ")+String(e.name),area.removeFromTop(24),Justification::centredLeft,true);
            if(e.collapsed)continue;g.setFont(11);g.setColour(muted);g.drawText("IN  "+rulesText(d,e),area.removeFromTop(24),Justification::centredLeft,true);
            auto eventBox=area.removeFromTop(timeline?65:94);g.setColour(inner);g.fillRoundedRectangle(eventBox.toFloat(),5);g.setColour(Colours::white);g.setFont(13);
            String description=kindName(e.kind);if(e.kind==Kind::Timeline){int children=0;for(auto& child:d.events)if(child.parent==e.id)++children;description+="  [ "+String(children)+" nested boxes ]\nDouble-click to enter  |  "+String(e.duration,1)+" s";g.setColour(accent.withAlpha(.35f));for(int i=0;i<jmin(children,5);++i)g.drawRoundedRectangle(float(eventBox.getX()+8+i*42),float(eventBox.getBottom()-22),35,14,2,1);g.setColour(Colours::white);}
            else if(e.kind==Kind::DMX)description+="\nChannel "+String(e.channel)+" -> "+String(e.value)+" / fade "+String(e.fade,2)+" s";
            else {StringArray out;for(auto& id:e.outputs)out.add(labelFor(d,id));description+="\n"+labelFor(d,e.input)+" -> "+out.joinIntoString(", ");if(e.kind==Kind::Move)description+="\n"+String(e.target,1)+" deg @ "+String(e.speed,1)+" deg/s  | planning ~"+String(e.duration,1)+" s";}
            g.drawFittedText(description,eventBox.reduced(8),Justification::topLeft,3);
            g.setColour(muted);g.setFont(11);String out=(e.kind==Kind::Haptic||e.kind==Kind::Link)?"OUT  Deactivate: ":"OUT  Continue: ";int count=0;for(auto& dest:d.events)for(auto& r:dest.in)if(r.source==e.id&&r.edge!=Edge::Position)++count;out+=String(count)+" relationship(s) | "+status;
            if(timeline&&!e.in.empty())out="Earliest "+String(e.start,2)+" s; actual start waits for condition | "+status;
            g.drawText(out,area.removeFromTop(25),Justification::centredLeft,true);
        }
        if(timeline){auto it=panel.current.events.find(panel.parent);if(it!=panel.current.events.end()){g.setColour(accent);g.drawVerticalLine(30+int(it->second.time*scale),35,float(getHeight()));}}
        if(index==0){g.setColour(muted);g.setFont(18);g.drawFittedText("Add an event or timeline. Configure your motors in Hardware Setup.",getLocalBounds().reduced(30),Justification::topLeft,3);}
    }
    void mouseDown(const MouseEvent& e)override {for(auto& p:bounds)if(p.second.contains(e.getPosition())){panel.selected=p.first;down=e.getPosition();dragging=p.first;for(auto& ev:panel.current.document.events)if(ev.id==p.first){initialStart=ev.start;initialDuration=ev.duration;initialLane=ev.lane;}resizing=e.x>p.second.getRight()-12;
            if(e.x<p.second.getX()+28&&e.y<p.second.getY()+34){panel.perform([&]{auto d=service()->snapshot().document;for(auto& ev:d.events)if(ev.id==p.first)ev.collapsed=!ev.collapsed;service()->edit(d);});dragging.clear();}repaint();return;}}
    void mouseDrag(const MouseEvent& e)override{if(panel.parent.empty()||dragging.empty())return;for(auto& ev:panel.current.document.events)if(ev.id==dragging){double dx=(e.x-down.x)/scale;if(resizing)ev.duration=std::max(.1,std::round((initialDuration+dx)*10)/10);else{ev.start=std::max(0.,std::round((initialStart+dx)*10)/10);ev.lane=std::max(0,initialLane+int(std::round((e.y-down.y)/182.)));}}repaint();}
    void mouseUp(const MouseEvent&)override{if(!dragging.empty()&&!panel.parent.empty())panel.perform([&]{auto latest=service()->snapshot().document;for(auto& changed:panel.current.document.events)if(changed.id==dragging)for(auto& e:latest.events)if(e.id==dragging){e.start=changed.start;e.duration=changed.duration;e.lane=changed.lane;}service()->edit(latest);});dragging.clear();}
    void mouseDoubleClick(const MouseEvent& e)override{for(auto& p:bounds)if(p.second.contains(e.getPosition())){for(auto& ev:panel.current.document.events)if(ev.id==p.first){if(ev.kind==Kind::Timeline)panel.navigate(ev.id);else panel.openEditor(ev.id);}return;}}
    bool isDragging()const{return !dragging.empty();}
};
NativePanel::NativePanel(const String& name):ShapeShifterContentComponent(name),canvas(std::make_unique<Canvas>(*this)) {
    addAndMakeVisible(title);addAndMakeVisible(status);addAndMakeVisible(breadcrumb);addAndMakeVisible(zoom);addAndMakeVisible(viewport);viewport.setViewedComponent(canvas.get(),false);viewport.setScrollBarsShown(true,true);
    title.setText("MIDNIGHT  /  Native V0.1",dontSendNotification);title.setFont(Font(24,Font::bold));title.setColour(Label::textColourId,accent);
    add("New",[this]{auto hardware=service()->snapshot().document;Engine::mainEngine->saveIfNeededAndUserAgreesAsync([hardware](FileBasedDocument::SaveResult r){if(r==FileBasedDocument::userCancelledSave||r==FileBasedDocument::failedToWriteToFile)return;Engine::mainEngine->createNewGraph();auto d=hardware;d.events.clear();service()->edit(d);});});
    add("Open",[]{Engine::mainEngine->saveIfNeededAndUserAgreesAsync([](FileBasedDocument::SaveResult r){if(r!=FileBasedDocument::userCancelledSave&&r!=FileBasedDocument::failedToWriteToFile)MessageManager::callAsync([]{Engine::mainEngine->loadFromUserSpecifiedFileAsync(true,[](juce::Result){});});});});
    add("Save",[]{Engine::mainEngine->saveAsync(true,true,[](FileBasedDocument::SaveResult){});});
    add("Save As",[]{Engine::mainEngine->saveAsInteractiveAsync(true,[](FileBasedDocument::SaveResult){});});
    add("Hardware Setup",[]{showWindow("Hardware Setup",new HardwareSetup(),850,720);});
    add("Add event",[this]{newEvent(Kind::Move);});add("Add timeline",[this]{newEvent(Kind::Timeline);});
    add("Edit selected",[this]{require(!selected.empty(),"Select an event box");openEditor(selected);});
    add("Copy",[this]{require(!selected.empty(),"Select a box");clipboard=current.document;copied=selected;});
    add("Paste",[this]{require(!copied.empty(),"Copy a box first");auto d=service()->snapshot().document;auto events=copyEvents(clipboard,copied,parent,uuid);d.events.insert(d.events.end(),events.begin(),events.end());service()->edit(d);});
    add("Remove selected",[this]{require(!selected.empty(),"Select a box");auto d=service()->snapshot().document;std::set<Id> removed{selected};bool changed=true;while(changed){changed=false;for(auto& e:d.events)if(removed.count(e.parent)&&removed.insert(e.id).second)changed=true;}d.events.erase(std::remove_if(d.events.begin(),d.events.end(),[&](const Event& e){return removed.count(e.id)!=0;}),d.events.end());service()->edit(d);selected.clear();});
    add("GO selected",[this]{require(!selected.empty(),"Select an event box");auto id=selected;service()->action([id](Runtime& r){if(!r.armed)r.arm();r.manualGo(id);});});
    add("Pause timeline",[this]{auto id=selected.empty()?parent:selected;service()->action([id](Runtime& r){r.pause(id);});});
    add("Stop selected",[this]{auto id=selected;require(!id.empty(),"Select a box");service()->action([id](Runtime& r){r.stop(id);});});
    add("Restart selected",[this]{auto id=selected;require(!id.empty(),"Select a box");service()->action([id](Runtime& r){r.stop(id);r.go(id);});});
    add("STOP ALL",[]{service()->action([](Runtime& r){r.stopAll();});});
    add("Up / workspace",[this]{Id up;for(auto& e:current.document.events)if(e.id==parent)up=e.parent;navigate(up);});
    add("Demo (simulated)",[this]{auto demo=demonstration();auto d=service()->snapshot().document;d.adapters.insert(d.adapters.end(),demo.adapters.begin(),demo.adapters.end());d.motors.insert(d.motors.end(),demo.motors.begin(),demo.motors.end());d.events.insert(d.events.end(),demo.events.begin(),demo.events.end());service()->edit(d);navigate({});});
    add("CAN / timing status",[]{showWindow("CAN and runtime status",new Monitor(),1100,700);});
    zoom.setRange(15,180,1);zoom.setValue(55);zoom.setTextBoxStyle(Slider::TextBoxRight,false,55,24);zoom.onValueChange=[this]{canvas->scale=zoom.getValue();update();};zoom.setTooltip("Timeline pixels per second; scroll horizontally for long timelines");startTimerHz(15);update();
}
NativePanel::~NativePanel(){stopTimer();}
void NativePanel::add(const String& text,std::function<void()> fn){auto* b=buttons.add(new TextButton(text));b->onClick=[this,fn]{perform(fn);};addAndMakeVisible(b);}
void NativePanel::perform(const std::function<void()>& fn){try{require(service()!=nullptr,"Engine unavailable");require(!Engine::mainEngine->isLoadingFile&&!Engine::mainEngine->isClearing,"Project is loading");fn();localError.clear();update();}catch(const std::exception& e){localError=e.what();status.setText(localError,dontSendNotification);}}
void NativePanel::newEvent(Kind kind){auto d=service()->snapshot().document;Event e;e.id=uuid();e.parent=parent;e.kind=kind;e.name=kind==Kind::Timeline?"New timeline":"New movement";e.duration=kind==Kind::Timeline?10:5;int lane=0;for(auto& x:d.events)if(x.parent==parent)lane=std::max(lane,x.lane+1);e.lane=lane;if(kind==Kind::Move){require(!d.motors.empty(),"Add a motor in Hardware Setup first");e.outputs={d.motors[0].id};e.target=std::clamp(45.,d.motors[0].min,d.motors[0].max);e.speed=d.motors[0].speed;}d.events.push_back(e);service()->edit(d);selected=e.id;openEditor(e.id);}
std::unique_ptr<Component> createEventEditor(const Id& id){return std::make_unique<EventEditor>(id);}
std::unique_ptr<Component> createMotorEditor(const Id& id){return std::make_unique<MotorEditor>(id);}
void NativePanel::openEditor(const Id& id){showWindow("Adaptive Event Editor",createEventEditor(id).release(),920,860);}
void NativePanel::navigate(const Id& id){parent=id;selected.clear();viewport.setViewPosition(0,0);update();}
void NativePanel::timerCallback(){if(canvas->isDragging())return;update();}
void NativePanel::update(){if(!service()||Engine::mainEngine->isLoadingFile||Engine::mainEngine->isClearing)return;current=service()->snapshot();bool exists=parent.empty();for(auto& e:current.document.events)if(e.id==parent)exists=true;if(!exists)parent.clear();
    breadcrumb.setText(parent.empty()?"Workspace  /  select a box, edit or GO":"Workspace  /  "+labelFor(current.document,parent)+"  /  drag blocks to move; drag right edge to resize planning width",dontSendNotification);
    status.setText(localError.isNotEmpty()?localError:String(current.error)+(current.error.empty()?"Project: "+Engine::mainEngine->getDocumentTitle()+(Engine::mainEngine->hasChangedSinceSaved()?" *":"")+" | Opening never starts execution. GO resumes a paused timeline.":""),dontSendNotification);
    int width=jmax(750,viewport.getWidth()-18),height=500,index=0;for(auto& e:current.document.events)if(e.parent==parent){++index;if(!parent.empty()){width=jmax(width,60+int((e.start+e.duration)*canvas->scale));height=jmax(height,240+e.lane*182);}}
    if(parent.empty())height=jmax(height,30+((index+jmax(1,width/390)-1)/jmax(1,width/390))*218);canvas->setSize(width,height);canvas->repaint();repaint();
}
void NativePanel::resized(){auto a=getLocalBounds().reduced(12);title.setBounds(a.removeFromTop(36));int cols=jmax(3,a.getWidth()/140);for(int i=0;i<buttons.size();i+=cols){auto row=a.removeFromTop(33);int w=row.getWidth()/cols;for(int j=0;j<cols&&i+j<buttons.size();++j)buttons[i+j]->setBounds(row.removeFromLeft(w).reduced(2));}status.setBounds(a.removeFromTop(28));auto nav=a.removeFromTop(30);zoom.setBounds(nav.removeFromRight(180));breadcrumb.setBounds(nav);auto cupboards=a.removeFromBottom(116);ignoreUnused(cupboards);viewport.setBounds(a);update();}
void NativePanel::paint(Graphics& g){g.fillAll(background);int n=int(current.document.motors.size());if(!n)return;auto area=getLocalBounds().reduced(18).removeFromBottom(100);int width=jmin(260,area.getWidth()/n);for(auto& m:current.document.motors){auto cell=area.removeFromLeft(width).reduced(5);auto f=current.motors[m.id];g.setColour(box);g.fillRoundedRectangle(cell.toFloat(),6);g.setColour(accent);g.setFont(12);g.drawText(String(m.name),cell.removeFromTop(22),Justification::centred);auto center=cell.getCentre().toFloat();g.setColour(muted);g.drawLine(center.x-25,center.y,center.x+25,center.y,1);double angle=radians(f.at>=0?f.position:0);g.setColour(f.at>=0?accent:muted);g.drawLine(center.x,center.y,center.x+float(std::cos(angle)*30),center.y-float(std::sin(angle)*30),4);g.setColour(Colours::white);g.drawText(f.at>=0?String(f.position,1)+" deg measured":"No feedback",cell.removeFromBottom(18),Justification::centred);}}
}
