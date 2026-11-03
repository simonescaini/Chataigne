// Deterministic production-core acceptance. No serial hardware is constructed.
#include "../Source/Midnight/NativeCore.cpp"
#include "../Modules/juce_timeline/timeline/Sequence/SequencePlaybackClock.h"
#include <iostream>
#include <fstream>
#include <iomanip>
using namespace midnightNative;
static int checks=0;
std::string traceLine(const Frame& frame){auto line=slcan(frame);line.pop_back();return line;}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<"\n";}
template<class F>void rejects(F fn,const char* message){bool rejected=false;try{fn();}catch(...){rejected=true;}check(rejected,message);}
Document fixture(){Document d;d.adapters.push_back({"bus","fake","","",1000000,2000000,true,true});for(int i=0;i<3;++i){Motor m;m.id="m"+std::to_string(i);m.adapter="bus";m.can=126+i;m.min=-180;m.max=180;d.motors.push_back(m);}return d;}
struct Rig {
    Runtime r;std::vector<Transmission> sent;std::vector<ClockRequest> clocks;std::map<Id,uint64_t> generation;
    Rig(){r.document=fixture();r.connection("bus",true);r.send=[this](const Transmission& t){sent.push_back(t);};r.invalidate=[this](const Id& id,uint64_t g){generation[id]=g;};r.clock=[this](const ClockRequest& c){clocks.push_back(c);};}
    void position(Id id,double degrees,double at){auto& m=r.motor(id);auto request=read(m.can,m.host);r.transmitted({"bus",id,r.feedback[id].generation,request,at,0,false},at);auto f=read(m.host,m.can);float v=float(radians(degrees));uint32_t bits;std::memcpy(&bits,&v,4);for(int i=0;i<4;++i)f.data[4+i]=uint8_t(bits>>(i*8));r.receive("bus",f,at);}
    void flush(double at){auto frames=sent;for(auto& f:frames)if(generation[f.motor]==f.generation)r.transmitted(f,at);}
    Event& add(Id id,Kind kind=Kind::Move,Id parent={}){Event e;e.id=id;e.name=id;e.kind=kind;e.parent=parent;if(kind==Kind::Move)e.outputs={"m0"};r.document.events.push_back(e);return r.document.events.back();}
};
int main(int argc,char** argv){try{
    {check(slcan(torque(4,60,"RS04"))=="T01BFFF0488000800000000000\r","RS04 independent +60 Nm vector at half positive scale");
        check(torque(4,-60,"RS04").id==0x01400004&&torque(4,0,"RS04").id==0x01800004,"RS04 negative and zero torque literal vectors");
        check(operation(4,"RS04",2*pi,-7.5,60,2500,25).data==std::array<uint8_t,8>{0xbf,0xff,0x40,0,0x80,0,0x40,0},"RS04 position velocity and gain scaling from manual table");
        check(limits("RS04")==std::array<double,3>{20,90,120},"RS04 parameter limits separate from packet scales");
        Rig g;g.r.motor("m0").model="RS04";g.r.validate();Frame f=packet(2,254,126);f.data={0xbf,0xff,0x40,0,0xbf,0xff,1,44};g.r.receive("bus",f,1);
        auto& s=g.r.feedback["m0"];check(std::abs(s.hapticPosition-2*pi)<.001&&std::abs(s.velocity+7.5)<.001&&std::abs(s.measuredTorque-60)<.003&&s.temperature==30,"RS04 literal Type-2 signed feedback uses endpoint model");
        g.r.move("m0",30,20);check(g.sent.size()==7&&g.sent[2].frame==write(126,254,0x7018,3,"RS04"),"RS04 CSP preserves user current rather than selecting maximum");
        g.sent.clear();g.r.beginTorque("m0",.5);bool report=false;for(auto& t:g.sent)report|=((t.frame.id>>24)&31)==24;check(!report,"RS04 haptics use command replies without ambiguous active-report write");
        g.r.mechanicalZero("m0");check(g.sent.back().frame==Frame{0x0600fe7e,{1,0,0,0,0,0,0,0}},"RS04 explicit Type-6 zero literal vector");}
    {Frame f;f.extended=false;f.id=0x141;f.data[0]=0x92;
        check(slcan(f)=="t14189200000000000000\r","literal standard SLCAN TX");
        Frame rx;check(parseSlcan("t241892000000A08C0000",rx)&&!rx.extended&&rx.id==0x241&&rx.length==8,"literal standard SLCAN RX");
        auto other=f;other.extended=true;check(!(f==other),"format participates in CAN equality even with same numeric ID");
        check(parseSlcan("t1232AABB",rx)&&rx.length==2&&slcan(rx)=="t1232AABB\r","short transport frames retain real DLC");
        for(auto line:{"t8000","t1419","t1418920000","T1100FE7E8197000000000000000","t1411GG","r1418","t141-"})check(!parseSlcan(line,rx),"reject malformed CAN frame without padding or truncation");
        check(parseSlcan("T000001412AABB",rx)&&rx.extended,"small numeric identifier remains explicitly extended");}
    check(packet(3,126,254).id==0x0300fe7e,"literal enable CAN identifier");
    check(slcan(read(126,254))=="T1100FE7E81970000000000000\r","literal extended SLCAN mechanical-position request");
    check(slcanBitrate(1000000)=='8'&&slcanBitrate(750000)=='7'&&slcanBitrate(83300)=='9',"pinned SLCAN bitrate encodings");
    rejects([]{slcanBitrate(800000);},"unsupported bitrate cannot silently select a different bus rate");
    check(write(126,254,0x7016,1.5,"RS02").data==std::array<uint8_t,8>{0x16,0x70,0,0,0,0,0xc0,0x3f},"literal float32 target packet");
    Frame parsed;auto wire=slcan(torque(126,0));wire.pop_back();check(parseSlcan(wire,parsed)&&parsed==torque(126,0),"SLCAN roundtrip Type-1 torque");
    check(!parseSlcan("TFFFFFFFF80000000000000000",parsed),"reject out-of-range extended identifier");
    check(torque(126,0).id==0x0180007e&&torque(126,0).data==std::array<uint8_t,8>{128,0,128,0,0,0,0,0},"reference torque rounding and gain packing");
    write(126,254,0x7017,44,"RS01");write(126,254,0x7018,23,"RS01");check(true,"RS01 distinct speed/current profile");
    rejects([]{write(126,254,0x7017,33.001,"RS02");},"RS02 speed constraint");rejects([]{write(126,254,0x7018,16.001,"RS02");},"RS02 current constraint");
    rejects([]{write(126,254,0x7016,1e40,"RS02");},"reject float32 overflow");
    write(126,254,0x7016,20*pi,"RS02");check(true,"float target not constrained to Type-2 wrapped extent");
    {Adapter a;a.port="COM6";a.identity="unique";std::vector<Port> ports{{"COM11","unique","","VID_16D0&PID_117E"},{"COM6","different","","VID_16D0&PID_117E"}};
        check(resolvePort(a,ports)=="COM11","remember identity across COM renumbering");ports.push_back({"COM12","unique","",""});check(resolvePort(a,ports).empty(),"ambiguous identity requires manual selection");ports.erase(ports.begin());check(resolvePort(a,{{"COM6","different","",""}}).empty(),"never treat VID/PID or old COM as unique identity");a.identity.clear();check(resolvePort(a,ports)=="COM6","explicit manual port fallback");}
    {Rig g;g.r.move("m0",degrees(1.5),degrees(1));check(g.sent.size()==7,"first move seven-packet CSP sequence");
        std::vector<Frame> expected{packet(4,126,254),write(126,254,0x7005,5,"RS02"),write(126,254,0x7018,3,"RS02"),write(126,254,0x7017,1,"RS02"),packet(3,126,254),write(126,254,0x7017,1,"RS02"),write(126,254,0x7016,1.5,"RS02")};
        for(size_t i=0;i<expected.size();++i)check(g.sent[i].frame==expected[i],"CSP reference sequence packet parity");
        if(argc>1){std::ofstream out(argv[1]);for(auto& t:g.sent)out<<traceLine(t.frame)<<'\n';}
        g.flush(.1);g.sent.clear();g.r.move("m0",45,90);check(g.sent.size()==2,"subsequent move transmits speed + new target without arrival gate");
        g.r.disable("m0");g.sent.clear();g.r.move("m0",10,30);check(g.sent.size()==7,"Stop/Disable followed by deliberate Move configures and enables");
        rejects([&]{g.r.move("m0",181,30);},"saved target bounds enforced");
    }
    {Rig g;g.position("m0",12,1);check(std::abs(g.r.feedback["m0"].position-12)<1e-4,"mechanical position reply decoded");check(g.r.feedback["m1"].at<0,"shared bus reply does not update a different motor");g.r.receive("bus",read(126,254),1);check(std::abs(g.r.feedback["m0"].position-12)<1e-4,"request echo is not measured zero");g.r.receive("other",read(254,126),1);check(g.r.feedback["m0"].at==1,"other adapter cannot route feedback");
        g.r.mechanicalZero("m0");check(((g.sent.back().frame.id>>24)&31)==6&&g.sent.back().frame.data[0]==1,"explicit mechanical zero uses controller protocol");}
    {Rig g;auto feedback=[](double rad){auto f=packet(2,254,126);int p=int(std::nearbyint((rad+4*pi)*65535/(8*pi)));f.data={uint8_t(p>>8),uint8_t(p),128,0,128,0,1,44};return f;};
        g.r.receive("bus",feedback(4*pi-.01),1);double before=g.r.feedback["m0"].hapticPosition;g.r.receive("bus",feedback(-4*pi+.01),1.01);
        check(std::abs(g.r.feedback["m0"].hapticPosition-before-.02)<.001,"Type-2 wrap crossing unwraps continuously");
        g.r.move("m0",30,20);g.flush(1.02);auto fault=feedback(0);fault.id|=1u<<16;g.r.receive("bus",fault,1.03);
        check(g.r.feedback["m0"].faults==1&&!g.r.feedback["m0"].enableSent,"addressed Type-2 fault disables without claiming acceptance");
        rejects([&]{g.r.move("m0",20,20);},"reported drive faults remain effective for subsequent Move");
    }
    {Rig g;auto& slow=g.add("slow");slow.target=130;slow.speed=5;slow.keepActive=false;auto& fast=g.add("fast");fast.target=45;fast.speed=90;fast.in={{Edge::Position,"m0",10,0,1}};
        g.r.validate();g.position("m0",0,0);g.r.arm();g.r.go("slow");g.flush(.01);g.position("m0",9,.02);g.r.step(.02);check(g.r.execution["fast"].status==Status::Waiting,"no position trigger before crossing");
        g.position("m0",10.01,.03);g.r.step(.03);check(g.r.execution["slow"].status==Status::Superseded&&g.r.execution["fast"].status==Status::Running,"crossing measured 10 degrees supersedes slow 130 target with fast 45 target");
        auto serial=g.r.execution["fast"].started;g.position("m0",11,.04);g.r.step(.04);check(g.r.execution["fast"].started==serial,"one-shot crossing does not fire per feedback sample");
        g.flush(.04);g.position("m0",45,.05);g.r.step(.05);check(g.r.execution["fast"].status==Status::Completed,"fresh feedback determines completion");
        check(g.r.feedback["m0"].enableSent,"obsolete disable-after event cannot disable newer completed owner");
    }
    {Rig g;g.add("a");auto& b=g.add("b");b.outputs={"m1"};b.in={{Edge::Started,"a",0,.1,1},{Edge::Stopped,"a",0,0,1}};g.r.arm();g.r.go("a");g.r.step(.01);check(g.r.execution["b"].status==Status::Waiting,"start-edge delay waits");g.r.step(.12);check(g.r.execution["b"].status==Status::Running,"ANY start OR stop supports delayed start");g.r.stopAll();g.r.arm();g.r.go("a");g.r.stop("a");g.r.step(.13);check(g.r.execution["b"].status==Status::Running,"ANY start OR stop supports explicit stop edge");}
    {Rig g;g.add("a");auto& b=g.add("b");b.outputs={"m1"};b.all=true;b.in={{Edge::Started,"a"},{Edge::Position,"m2",10}};g.position("m2",0,0);g.r.arm();g.r.go("a");g.r.step(.01);check(g.r.execution["b"].status==Status::Waiting,"ALL waits for both conditions");g.position("m2",11,.02);g.r.step(.02);check(g.r.execution["b"].status==Status::Running,"ALL combines latched event and position edges");}
    {Rig g;auto& a=g.add("manual");a.outputs={"m1"};a.all=true;a.in={{Edge::Manual,{},0,.05},{Edge::Position,"m0",10}};g.position("m0",0,0);g.r.arm();g.r.manualGo("manual");g.r.step(.06);check(g.r.execution["manual"].status==Status::Waiting,"explicit Manual GO latches without bypassing ALL position condition");g.position("m0",11,.07);g.r.step(.07);check(g.r.execution["manual"].status==Status::Running,"Manual GO and measured crossing satisfy ALL together");}
    {Rig g;auto& a=g.add("manual");a.in={{Edge::Manual,{},0,.05},{Edge::Position,"m2",10}};g.r.arm();g.r.manualGo("manual");check(g.r.execution["manual"].status==Status::Waiting,"explicit Manual GO honors its optional delay");g.r.step(.06);check(g.r.execution["manual"].status==Status::Running,"ANY accepts delayed Manual GO without a position crossing");}
    {Rig g;auto& a=g.add("a");a.target=5;auto& complete=g.add("complete");complete.outputs={"m1"};complete.in={{Edge::Completed,"a"}};auto& stop=g.add("stop");stop.outputs={"m2"};stop.in={{Edge::Stopped,"a"}};auto& replace=g.add("replace");replace.outputs={"m2"};replace.in={{Edge::Superseded,"a"}};
        g.r.arm();g.r.go("a");g.flush(.01);g.position("m0",5,.02);g.r.step(.02);
        check(g.r.execution["complete"].status==Status::Running&&g.r.execution["stop"].status==Status::Waiting,"normal completion fires completion edge without a stop edge");
        g.r.move("m0",20,30);g.r.step(.03);check(g.r.execution["replace"].status==Status::Running&&g.r.execution["stop"].status==Status::Waiting,"supersession fires its own edge without synthesizing explicit stop");
    }
    {Rig g;auto& light=g.add("repeat",Kind::DMX);light.repeat=true;light.in={{Edge::Position,"m0",10}};g.position("m0",0,0);g.r.arm();g.position("m0",11,.01);g.r.step(.01);auto first=g.r.execution["repeat"].started;g.position("m0",12,.02);g.r.step(.02);check(first>0&&g.r.execution["repeat"].started==first,"repeated condition waits for a new crossing after completion");g.position("m0",9,.03);g.r.step(.03);g.position("m0",11,.04);g.r.step(.04);check(g.r.execution["repeat"].started>first,"repeat rearms after returning below the increasing threshold");}
    {Rig g;auto& t=g.add("parent",Kind::Timeline);t.duration=1;auto& nested=g.add("nested",Kind::Timeline,"parent");nested.start=.2;auto& move=g.add("child",Kind::Move,"nested");move.keepActive=true;auto& waiting=g.add("waiting",Kind::Move,"parent");waiting.start=5;g.r.arm();g.r.go("parent");g.r.timelineTime("parent",.3,false);g.r.step(.3);check(g.r.execution["nested"].status==Status::Running,"nested timeline starts from parent time");g.r.timelineTime("nested",.1,false);g.r.step(.31);check(g.r.execution["child"].status==Status::Running,"nested child executes");
        g.r.pause("parent");check(g.r.execution["nested"].status==Status::Paused,"parent pause freezes nested timelines");check(g.r.execution["child"].status==Status::Running,"pause leaves already commanded movement running");g.r.resume("parent");g.r.timelineTime("parent",1,true);g.r.step(1);check(g.r.execution["parent"].status==Status::Completed&&g.r.execution["waiting"].status==Status::Stopped,"normal parent end cancels waiting children");check(g.r.execution["child"].status==Status::Running,"normal parent completion does not disable active children");g.r.stop("parent");check(g.r.execution["child"].status==Status::Stopped,"explicit parent stop recursively stops active children");}
    {Rig g;auto& link=g.add("link",Kind::Link);link.input="m0";link.outputs={"m1","m2"};for(auto id:{"m0","m1","m2"})g.position(id,0,0);g.r.go("link");g.position("m0",20,.01);g.r.step(.01);check(g.r.feedback["m1"].requested==g.r.feedback["m2"].requested&&std::abs(g.r.feedback["m1"].requested-20)<1e-4,"one master updates two simultaneous followers (three simulated motors)");g.flush(.02);size_t before=g.sent.size();g.position("m0",20.1,.03);g.r.step(.03);int targets=0;for(size_t i=before;i<g.sent.size();++i)if(g.sent[i].frame.data[0]==0x16&&g.sent[i].frame.data[1]==0x70)++targets;check(targets==0,"following deadband suppresses redundant targets");g.r.stop("link");g.position("m0",25,.04);g.r.step(.04);check(g.r.execution["link"].status==Status::Stopped&&!g.r.feedback["m1"].enableSent,"deactivated link cannot restart followers");}
    {Event h;h.stiffness=2;h.damping=.5;h.deadband=.2;h.torqueLimit=1;auto t=springDamper(h,0,.1,0,.2,0);check(std::abs(t[0]-.3)<1e-12&&t[1]==-t[0],"reference symmetric spring damping law");t=springDamper(h,0,.001,0,.2,0);check(std::abs(t[0]-.1)<1e-12,"damping remains active inside position deadband");h.signB=-1;t=springDamper(h,0,-1,0,0,0);check(t[0]==1&&t[1]==1,"haptic signs and torque saturation");}
    {Rig g;auto& h=g.add("h",Kind::Haptic);h.input="m0";h.outputs={"m1"};for(auto id:{"m0","m1"})g.position(id,0,0);g.r.go("h");g.flush(.06);g.r.step(.07);check(g.r.feedback["m0"].torqueMode&&g.r.feedback["m1"].torqueMode,"haptic setup enters operation mode for both motors");g.r.move("m1",20,30);check(g.r.execution["h"].status==Status::Superseded&&!g.r.feedback["m0"].enableSent,"deliberate Move stops the whole haptic producer before CSP");check(!g.r.feedback["m1"].torqueMode,"haptic-to-movement transition restores CSP path");}
    {Rig g;auto& h=g.add("h",Kind::Haptic);h.input="m0";h.outputs={"m1"};for(auto id:{"m0","m1"})g.position(id,0,0);g.r.go("h");g.flush(.06);g.r.step(.2);check(g.r.execution["h"].status==Status::Failed&&!g.r.feedback["m0"].enableSent,"stale haptic feedback zeroes and disables pair");}
    {Rig g;auto& d=g.add("lights",Kind::DMX);d.fade=1;d.value=200;std::vector<int> values;g.r.dmx=[&](int channel,int value){check(channel==1,"DMX channel routes correctly");values.push_back(value);};g.r.stopAll();g.r.go("lights");g.r.step(.5);g.r.step(1);check(values==std::vector<int>{100,200},"linear DMX fade executes after Stop All without a phantom owner");}
    {Rig g;auto& parent=g.add("t",Kind::Timeline);parent.name="renamable";g.add("a",Kind::Move,"t");auto& b=g.add("b",Kind::Move,"t");b.in={{Edge::Completed,"a"},{Edge::Position,"m0",10}};int n=0;auto copy=copyEvents(g.r.document,"t",{},[&]{return "copy"+std::to_string(++n);});check(copy.size()==3,"timeline copy includes descendants");check(copy[2].in[0].source==copy[1].id&&copy[2].in[1].source=="m0","copy remaps internal event references and retains external motor reference");check(copy[1].parent==copy[0].id&&copy[1].id!="a","copy has independent stable identity");}
    {SequencePlaybackClock clock;clock.reset(100,0,250,1);check(clock.frameAt(101)==0&&clock.frameAt(112)==3&&clock.timeForFrame(3)==.012,"native Chataigne playback clock used without GUI refresh");}
    if(argc>1){
        std::ofstream out(std::string(argv[1])+".parity");out<<std::setprecision(17);
        for(double value:{-17.,-14.,-1.,-.001,0.,.001,1.,14.,17.})out<<"TORQUE|"<<value<<'|'<<traceLine(torque(126,value))<<'\n';
        for(std::string model:{"RS01","RS02"}){auto range=limits(model);for(int i=0;i<4;++i){int index=i==0?0x7016:i==1?0x7017:i==2?0x7018:0x700b;double value=i==0?1.5:range[i-1];out<<"WRITE|"<<model<<'|'<<index<<'|'<<value<<'|'<<traceLine(write(126,254,index,value,model))<<'\n';}}
        Event h;h.stiffness=2;h.damping=.5;h.deadband=.2;h.torqueLimit=1;
        for(int a:{-1,1})for(int b:{-1,1})for(double position:{-.2,.001,3.}){h.signA=a;h.signB=b;auto t=springDamper(h,.3,position,.7,-.9,.1);out<<"HAPTIC|"<<a<<'|'<<b<<"|0.3|"<<position<<"|0.7|-0.9|0.1|"<<t[0]<<'|'<<t[1]<<'\n';}
    }
    std::cout<<"SUCCESS "<<checks<<" core checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
