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
 std::cout<<checks<<" range integration checks passed\n";
}
