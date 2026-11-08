// Actual Runtime and model drivers; no service, serial adapter or physical IO.
#include "../Source/Midnight/NativeCore.cpp"
#include <iostream>
using namespace midnightNative;
int main() {
 int checks=0;
 auto check=[&](bool yes,const char* text){if(!yes)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<"\n";};
 for(const auto& model:{"RS01","RS02","RS04","RMD-L-7025"}) {
    Runtime r; Adapter a; a.id="fake";a.simulated=true;r.document.adapters.push_back(a);
    Motor master;master.id="master";master.adapter=a.id;master.model="RS02";master.can=1;
    Motor follower=master;follower.id="follower";follower.can=2;follower.model=model;
    r.document.motors={master,follower};r.validate();
    r.connection(a.id,true);r.now=1;
    std::vector<Transmission> frames;
    r.send=[&](const Transmission& t){frames.push_back(t);};
    bool rejected=false;
    try{r.move(follower.id,220,30);}catch(const std::runtime_error&){rejected=true;}
    check(rejected&&frames.empty(),"Out-of-range Move rejected before any packet");
    Event link;link.id="link";link.kind=Kind::Link;link.input=master.id;link.outputs={follower.id};
    r.document.events={link};r.feedback[master.id].position=220;r.feedback[master.id].at=1;r.feedback[follower.id].at=1;
    r.go(link.id);r.step(1);
    check(r.execution[link.id].status==Status::Running,"Link remains running beyond follower movement range");
    bool target=false;
    for(auto& t:frames)if(Driver{follower}.targetPacket(t.frame)){
        auto expected=Driver{follower}.target(220,30);target=t.frame==expected;
    }
    check(target&&r.feedback[follower.id].requested==220,"Link sends the unchanged master angle via the selected model driver");
 }
 Runtime r;Adapter a;a.id="fake";a.simulated=true;r.document.adapters={a};
 for(int n=1;n<=2;++n){Motor m;m.id=std::to_string(n);m.adapter=a.id;m.can=n;m.min=0;m.max=10;r.document.motors.push_back(m);}
 Event h;h.id="haptic";h.kind=Kind::Haptic;h.input="1";h.outputs={"2"};r.document.events={h};r.validate();r.connection(a.id,true);r.now=2;
 for(auto& m:r.document.motors){r.feedback[m.id].at=2;r.feedback[m.id].position=220;}
 r.send=[](const Transmission&){};
 r.go(h.id);r.step(2);
 check(r.execution[h.id].status==Status::Running,"Haptic activation and execution ignore movement ranges");
 // Master position control and Link are independent, including reverse start order.
 for(const auto& model:{"RS01","RS02","RS04","RMD-L-7025"}){
    Runtime t;Adapter bus;bus.id="bus";bus.simulated=true;t.document.adapters={bus};
    for(int n=0;n<3;++n){Motor m;m.id=std::to_string(n);m.adapter=bus.id;m.can=n+1;m.model=model;t.document.motors.push_back(m);}
    Event link;link.id="link";link.kind=Kind::Link;link.input="0";link.outputs={"1","2"};
    Event move;move.id="move";move.kind=Kind::Move;move.outputs={"0"};move.target=80;
    t.document.events={link,move};t.validate();t.connection(bus.id,true);t.now=1;
    for(auto& m:t.document.motors)t.feedback[m.id].at=1;
    std::vector<Transmission> wire;t.send=[&](const Transmission& x){wire.push_back(x);};
    t.go("link");t.step(1);auto linkStarted=t.execution["link"].started;
    t.move("0",30,30);
    check(t.execution["link"].status==Status::Running&&t.feedback["0"].requested==30,"direct master Move preserves its observational Link");
    t.go("move");
    check(t.execution["link"].status==Status::Running&&t.execution["link"].started==linkStarted,"master Move retains running Link and its start identity");
    t.feedback["0"].position=40;t.feedback["0"].at=1.01;
    for(const auto& x:wire)t.transmitted(x,1.01);
    t.step(1.01);
    check(t.feedback["1"].requested==40&&t.feedback["2"].requested==40,"both followers continue measured master tracking during Move");
    t.stop("link");
    check(t.execution["move"].status==Status::Running&&t.feedback["0"].owner=="move","End Link leaves the independent master Move intact");
    t.go("link");
    check(t.execution["move"].status==Status::Running&&t.feedback["0"].owner=="move","starting Link also preserves an already running master Move");
    t.move("1",30,30);
    check(t.execution["link"].status==Status::Superseded&&t.execution["move"].status==Status::Running,"follower Move retains whole-Link conflict handling without stopping master Move");
    t.go("link");t.stopMotor("0");
    check(t.execution["link"].status==Status::Stopped&&t.execution["move"].status==Status::Stopped,"explicit master stop still ends its Link and master Move");
 }
 // Assignment 2026-11-09 supersedes blanket rejection: valid Move ends its pair automatically.
 for(const auto& model:{"RS01","RS02","RS04"}){
    Runtime t;Adapter bus;bus.id="bus";bus.simulated=true;t.document.adapters={bus};
    for(int n=0;n<3;++n){Motor m;m.id=std::to_string(n);m.adapter=bus.id;m.can=n+1;m.model=model;t.document.motors.push_back(m);}
    Event h;h.id="h";h.name="test pair";h.kind=Kind::Haptic;h.input="0";h.outputs={"1"};
    Event m;m.id="curve";m.kind=Kind::Move;m.outputs={"0"};m.target=60;m.curve=[](double time){return 10+time;};m.repeat=true;
    Event other=m;other.id="other";other.outputs={"2"};other.curve={};
    Event preceding=other;preceding.id="preceding";preceding.outputs={"0"};
    Event link;link.id="link";link.kind=Kind::Link;link.input="0";link.outputs={"2"};
    t.document.events={h,m,other,preceding,link};t.validate();t.connection(bus.id,true);t.now=1;
    for(auto& motor:t.document.motors)t.feedback[motor.id].at=1;
    std::vector<Transmission> wire;t.send=[&](const Transmission& x){wire.push_back(x);};
    int clocks=0;t.clock=[&](const ClockRequest& c){if(c.action==ClockRequest::Restart)++clocks;};
    t.go("link");t.go("preceding");t.go("h");
    check(t.execution["link"].status==Status::Superseded&&t.execution["preceding"].status==Status::Superseded,"Haptic takeover retains native conflicts with both observational Link and master Move");
    t.go("other");auto independent=t.execution["other"].started;
    auto generation=t.feedback["0"].generation;
    bool rejected=false;try{t.move("0",999,30);}catch(...){rejected=true;}
    check(rejected&&t.execution["h"].status==Status::Running,"invalid Move does not end Haptic");
    rejected=false;try{t.move("0",70,30,{},999);}catch(...){rejected=true;}
    check(rejected&&t.execution["h"].status==Status::Running,"invalid current does not end Haptic");
    t.move("0",70,30);
    check(t.execution["h"].status==Status::Superseded&&!t.feedback["1"].torqueMode&&t.feedback["1"].owner.empty(),"direct Move ends the entire affected pair explicitly");
    check(t.feedback["0"].generation>generation&&!t.feedback["0"].torqueMode&&t.feedback["0"].requested==70,"Move invalidates Haptic generation and restores position mode");
    check(t.execution["other"].status==Status::Running&&t.execution["other"].started==independent,"handoff preserves unrelated motor work");
    wire.clear();t.step(1.01);
    bool oldTorque=false;for(const auto& x:wire)oldTorque|=((x.frame.id>>24)&31)==1&&(x.motor=="0"||x.motor=="1");
    check(!oldTorque,"superseded Haptic worker emits no further torque");
    t.now=1.02;for(auto& motor:t.document.motors)t.feedback[motor.id].at=t.now;
    t.go("h");wire.clear();t.go("curve");
    check(t.execution["curve"].status==Status::Running&&t.execution["curve"].started>0&&clocks==1&&t.feedback["0"].requested==10,"curve starts immediately at authored angle through automatic pair handoff");
    std::vector<int> sequence;for(const auto& x:wire)if(x.motor=="0"&&x.generation==t.feedback["0"].generation){int type=(x.frame.id>>24)&31;sequence.push_back(type==18?(x.frame.data[0]|x.frame.data[1]<<8):type);}
    check(sequence==std::vector<int>({4,0x7005,0x7018,0x7017,3,0x7017,0x7016}),"handoff orders stop, CSP mode, saved limits, enable and first target");
    t.stop("h");check(t.feedback["0"].owner=="curve","late End of superseded pair cannot end its replacement Move");

 }
 std::cout<<checks<<" range integration checks passed\n";
}
