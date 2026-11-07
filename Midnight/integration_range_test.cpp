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
 // All Move entry points must leave an active haptic pair untouched.
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
    auto before=wire.size();auto generation=t.feedback["0"].generation;
    t.move("0",70,30);t.move("1",80,30);t.go("curve");
    check(wire.size()==before&&t.feedback["0"].generation==generation&&t.execution["h"].status==Status::Running,"ignored direct and curve Moves send no packets and do not invalidate Haptic setup");
    check(t.execution["curve"].status==Status::Suppressed&&t.execution["curve"].started==0&&clocks==0,"suppressed curve has no start edge or clock");
    t.go("other");check(t.execution["other"].status==Status::Running,"Move on an unrelated motor is unaffected by Haptic");
    t.stop("h");wire.clear();t.step(1.01);
    bool oldTarget=false;for(const auto& x:wire)oldTarget|=x.motor=="0"&&Driver{t.motor("0")}.targetPacket(x.frame);
    check(!oldTarget&&t.execution["curve"].status==Status::Suppressed,"Haptic End does not replay a suppressed curve even when repeat is configured");
    t.go("curve");check(t.execution["curve"].status==Status::Running&&t.feedback["0"].requested==10&&clocks==1,"fresh curve trigger after End starts at unchanged authored angle");
 }
 std::cout<<checks<<" range integration checks passed\n";
}
