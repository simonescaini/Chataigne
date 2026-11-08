#include "MainIncludes.h"
#include "Module/ModuleIncludes.h"
#include "NativeService.h"
#include "NativeCore.cpp"
#include <juce_timeline/timeline/Sequence/SequencePlaybackClock.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>
#if JUCE_WINDOWS
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#endif

namespace midnightNative {
namespace {
void prepareCurve(Event& e,const Document& d) {
    if(e.curveData.empty()){e.curve={};return;}
    auto automation=std::make_shared<Automation>("Absolute position / degrees");
    automation->loadJSONData(JSON::fromString(String(e.curveData)));
    require(automation->items.size()>=2,"An absolute curve requires at least two keys");
    require(e.outputs.size()==1,"Select one movement motor");
    auto found=std::find_if(d.motors.begin(),d.motors.end(),[&](const Motor& m){return m.id==e.outputs[0];});
    require(found!=d.motors.end(),"Missing movement motor");
    const auto& m=*found;
    require(std::abs(automation->items[0]->position->doubleValue())<.000001,"The authored first key must be at time zero");
    require(std::abs(automation->items.getLast()->position->doubleValue()-e.duration)<.0001,"The authored endpoint must be at the movement duration");
    for(int i=0;i<automation->items.size();++i){
        auto* key=automation->items[i];double value=key->value->doubleValue();
        require(value>=m.min&&value<=m.max,"Absolute curve key outside saved Move MIN/MAX; curve not changed");
        if(i+1<automation->items.size()){
            require(key->position->doubleValue()<automation->items[i+1]->position->doubleValue(),"Curve times must be strictly increasing");
            require(key->easing&&(key->easing->type==Easing::LINEAR||key->easing->type==Easing::BEZIER||key->easing->type==Easing::HOLD),"Movement supports Linear, Bezier and Hold curve segments");
            if(key->easing->type==Easing::BEZIER){
                auto bounds=key->easing->getBounds(false);
                if(bounds.getHeight()>0)require(bounds.getY()>=m.min&&bounds.getBottom()<=m.max,"Bezier curve crosses a Move limit; curve not changed");
            }
        }
    }
    e.target=automation->items.getLast()->value->doubleValue();
    e.curve=[automation](double time){return automation->getValueAtPosition(time);};
}
var object(){return var(new DynamicObject());}
void set(var& v,const char* k,const var& x){v.getDynamicObject()->setProperty(k,x);}
std::string str(const var& v,const char* k,const std::string& d={}){return v.getProperty(k,String(d)).toString().toStdString();}
double num(const var& v,const char* k,double d){auto n=double(v.getProperty(k,d));finite(n);return n;}
bool boolean(const var& v,const char* k,bool d){return bool(v.getProperty(k,d));}
var rulesJSON(const std::vector<Rule>& rr){Array<var> a;for(auto& r:rr){auto v=object();set(v,"edge",int(r.edge));set(v,"source",String(r.source));set(v,"threshold",r.threshold);set(v,"delay",r.delay);set(v,"direction",r.direction);a.add(v);}return a;}
std::vector<Rule> loadRules(const var& v){std::vector<Rule> rr;if(auto* a=v.getArray())for(auto& r:*a)rr.push_back({Edge(int(num(r,"edge",0))),str(r,"source"),num(r,"threshold",10),num(r,"delay",0),int(num(r,"direction",1))});return rr;}
}
var encodeDocument(const Document& d) {
    auto root=object();set(root,"version",1);Array<var> adapters,motors,events;
    for(auto& a:d.adapters){auto v=object();set(v,"id",String(a.id));set(v,"name",String(a.name));set(v,"port",String(a.port));set(v,"identity",String(a.identity));set(v,"bitrate",a.bitrate);set(v,"baud",a.baud);set(v,"simulated",a.simulated);set(v,"reconnect",a.reconnect);adapters.add(v);}
    for(auto& m:d.motors){auto v=object();set(v,"id",String(m.id));set(v,"adapter",String(m.adapter));set(v,"name",String(m.name));set(v,"model",String(m.model));set(v,"protocol",String(protocolOf(m)));set(v,"direction",m.direction);set(v,"can",m.can);set(v,"host",m.host);set(v,"min",m.min);set(v,"max",m.max);set(v,"speed",m.speed);set(v,"current",m.current);set(v,"tolerance",m.tolerance);motors.add(v);}
    for(auto& e:d.events){if(e.external)continue;auto v=object();set(v,"id",String(e.id));set(v,"parent",String(e.parent));set(v,"name",String(e.name));set(v,"kind",int(e.kind));set(v,"input",String(e.input));Array<var> o;for(auto& id:e.outputs)o.add(String(id));set(v,"outputs",o);
        set(v,"start",e.start);set(v,"duration",e.duration);set(v,"curveData",String(e.curveData));set(v,"nominalSpeed",e.nominalSpeed);set(v,"currentLimit",e.currentLimit);set(v,"target",e.target);set(v,"speed",e.speed);set(v,"keepActive",e.keepActive);set(v,"all",e.all);set(v,"stopAll",e.stopAll);set(v,"repeat",e.repeat);set(v,"collapsed",e.collapsed);set(v,"in",rulesJSON(e.in));set(v,"out",rulesJSON(e.out));set(v,"stiffness",e.stiffness);set(v,"damping",e.damping);set(v,"torqueLimit",e.torqueLimit);set(v,"deadband",e.deadband);set(v,"offset",e.offset);set(v,"signA",e.signA);set(v,"signB",e.signB);set(v,"align",e.align);set(v,"channel",e.channel);set(v,"value",e.value);set(v,"fade",e.fade);set(v,"lane",e.lane);Array<var> external;for(auto& ref:e.copiedExternalReferences)external.add(String(ref));set(v,"copiedExternalReferences",external);events.add(v);
    }
    set(root,"adapters",adapters);set(root,"motors",motors);set(root,"events",events);set(root,"dmxHost",String(d.dmxHost));set(root,"dmxPort",d.dmxPort);set(root,"dmxUniverse",d.dmxUniverse);return root;
}
Document decodeDocument(const var& root) {
    Document d;if(root.isVoid())return d;require(root.isObject()&&int(root.getProperty("version",0))==1,"Unsupported Midnight document version");
    if(auto* aa=root["adapters"].getArray())for(auto& v:*aa){Adapter a;a.id=str(v,"id");a.name=str(v,"name");a.port=str(v,"port");a.identity=str(v,"identity");a.bitrate=int(num(v,"bitrate",1000000));a.baud=int(num(v,"baud",2000000));a.simulated=boolean(v,"simulated",false);a.reconnect=boolean(v,"reconnect",true);d.adapters.push_back(a);}
    if(auto* aa=root["motors"].getArray())for(auto& v:*aa){Motor m;m.id=str(v,"id");m.adapter=str(v,"adapter");m.name=str(v,"name");m.model=str(v,"model","RS02");m.protocol=str(v,"protocol",defaultProtocol(m.model));m.direction=int(num(v,"direction",1));m.can=int(num(v,"can",126));m.host=int(num(v,"host",254));m.min=num(v,"min",0);m.max=num(v,"max",180);m.speed=num(v,"speed",30);m.current=num(v,"current",3);m.tolerance=num(v,"tolerance",.5);d.motors.push_back(m);}
    if(auto* aa=root["events"].getArray())for(auto& v:*aa){Event e;e.id=str(v,"id");e.parent=str(v,"parent");e.name=str(v,"name");e.kind=Kind(int(num(v,"kind",0)));require(int(e.kind)>=0&&int(e.kind)<=4,"Unknown event type");e.input=str(v,"input");if(auto* oo=v["outputs"].getArray())for(auto& o:*oo)e.outputs.push_back(o.toString().toStdString());
        e.start=num(v,"start",0);e.duration=num(v,"duration",5);e.curveData=str(v,"curveData");e.nominalSpeed=num(v,"nominalSpeed",30);e.currentLimit=num(v,"currentLimit",-1);e.target=num(v,"target",45);e.speed=num(v,"speed",30);e.keepActive=boolean(v,"keepActive",true);e.all=boolean(v,"all",false);e.stopAll=boolean(v,"stopAll",false);e.repeat=boolean(v,"repeat",false);e.collapsed=boolean(v,"collapsed",false);e.in=loadRules(v["in"]);e.out=loadRules(v["out"]);e.stiffness=num(v,"stiffness",1);e.damping=num(v,"damping",.05);e.torqueLimit=num(v,"torqueLimit",.5);e.deadband=num(v,"deadband",.2);e.offset=num(v,"offset",0);e.signA=int(num(v,"signA",1));e.signB=int(num(v,"signB",1));e.align=boolean(v,"align",true);e.channel=int(num(v,"channel",1));e.value=int(num(v,"value",255));e.fade=num(v,"fade",0);e.lane=int(num(v,"lane",0));if(auto* refs=v["copiedExternalReferences"].getArray())for(auto& ref:*refs)e.copiedExternalReferences.push_back(ref.toString().toStdString());d.events.push_back(e);
    }
    d.dmxHost=str(root,"dmxHost","127.0.0.1");d.dmxPort=int(num(root,"dmxPort",6454));d.dmxUniverse=int(num(root,"dmxUniverse",0));for(auto& e:d.events)prepareCurve(e,d);Runtime check;check.document=d;check.validate();return d;
}
Document demonstration() {
    Document d;Adapter a;a.id=uuid();a.name="Demo loopback CANable (no physical port)";a.simulated=true;d.adapters.push_back(a);
    for(int i=0;i<3;++i){Motor m;m.id=uuid();m.adapter=a.id;m.name="Cupboard "+std::to_string(i+1);m.can=126+i;m.model=i==1?"RS01":"RS02";d.motors.push_back(m);}
    auto add=[&](std::string name,Kind kind,Id parent={}){Event e;e.id=uuid();e.name=name;e.kind=kind;e.parent=parent;return e;};
    auto main=add("Movement + measured position reactions",Kind::Timeline);main.duration=12;d.events.push_back(main);
    auto slow=add("Cupboard 1 - slow toward 130 deg",Kind::Move,main.id);slow.outputs={d.motors[0].id};slow.target=130;slow.speed=5;slow.duration=26;d.events.push_back(slow);
    auto independent=add("Cupboard 2 - independent move",Kind::Move,main.id);independent.outputs={d.motors[1].id};independent.target=65;independent.speed=20;independent.lane=1;d.events.push_back(independent);
    auto change=add("At measured 10 deg - faster target 45 deg",Kind::Move,main.id);change.outputs={d.motors[0].id};change.target=45;change.speed=90;change.in={{Edge::Position,d.motors[0].id,10,0,1}};change.lane=2;d.events.push_back(change);
    auto lights=add("Position crossing -> Art-Net fade",Kind::DMX,main.id);lights.in=change.in;lights.channel=1;lights.value=200;lights.fade=1;lights.lane=3;d.events.push_back(lights);
    auto nested=add("Nested timeline",Kind::Timeline,main.id);nested.start=5;nested.duration=4;nested.lane=4;d.events.push_back(nested);
    auto back=add("Cupboard 3 - nested movement",Kind::Move,nested.id);back.outputs={d.motors[2].id};back.target=30;back.speed=30;back.keepActive=false;d.events.push_back(back);
    auto link=add("Master 1 -> followers 2 + 3 (manual)",Kind::Link);link.input=d.motors[0].id;link.outputs={d.motors[1].id,d.motors[2].id};d.events.push_back(link);
    auto h=add("Haptic pair 1 <-> 2 (manual)",Kind::Haptic);h.input=d.motors[0].id;h.outputs={d.motors[1].id};h.out={{Edge::Position,d.motors[0].id,60,0,1}};d.events.push_back(h);
    return d;
}
std::vector<Port> discoverPorts() {
    std::vector<Port> ports;for(auto& p:serial::list_ports())ports.push_back({p.port,{},p.description,p.hardware_id});
#if JUCE_WINDOWS
    auto info=SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS,nullptr,nullptr,DIGCF_PRESENT);
    if(info!=INVALID_HANDLE_VALUE){SP_DEVINFO_DATA data{};data.cbSize=sizeof(data);
        for(DWORD i=0;SetupDiEnumDeviceInfo(info,i,&data);++i){wchar_t instance[1024]{};if(!SetupDiGetDeviceInstanceIdW(info,&data,instance,1024,nullptr))continue;
            auto key=SetupDiOpenDevRegKey(info,&data,DICS_FLAG_GLOBAL,0,DIREG_DEV,KEY_READ);if(key==INVALID_HANDLE_VALUE)continue;
            wchar_t name[256]{};DWORD size=sizeof(name);auto code=RegQueryValueExW(key,L"PortName",nullptr,nullptr,reinterpret_cast<BYTE*>(name),&size);RegCloseKey(key);
            if(code==ERROR_SUCCESS)for(auto& p:ports)if(p.port==String(name).toStdString())p.identity=String(instance).toStdString();
        }SetupDiDestroyDeviceInfoList(info);
    }
#endif
    return ports;
}

// One bus thread/receiver per saved adapter. It never waits for a motor to arrive.
class Bus final : public Thread {
public:
    struct Receipt {Transmission tx;Frame frame;double at=0;bool sent=false;};
    Adapter config;std::mutex mutex;std::deque<Transmission> outgoing;std::deque<Receipt> incoming;
    std::map<Id,uint64_t> generations;std::map<Id,double> lastMotorTx;
    struct SimMotor{Motor profile;double position=0,target=0,speed=30,current=3,torque=0,velocity=0;int mode=5;bool enabled=false,present=true;};
    std::map<std::pair<bool,int>,SimMotor> sim;
    std::unique_ptr<serial::Serial> port;SlcanStream stream;std::string status="Disconnected";
    std::atomic<bool> connected{false},wanted{false};std::atomic<double> lastAttempt{-10};double lastStep=0;size_t highWater=0;
    explicit Bus(const Adapter& a):Thread("Midnight shared CANable"),config(a){startThread();}
    ~Bus(){
        // Shutdown may follow document replacement. Drain the explicit disables
        // queued by the runtime before closing the adapter, within a bounded time.
        auto deadline=monotonic()+.25;while(monotonic()<deadline&&connected){bool pending;{std::lock_guard<std::mutex> l(mutex);pending=!outgoing.empty();}if(!pending)break;notify();Thread::sleep(2);}
        signalThreadShouldExit();notify();stopThread(1500);
    }
    void configureMotors(const std::vector<Motor>& motors){std::lock_guard<std::mutex> l(mutex);
        std::map<std::pair<bool,int>,SimMotor> next;
        for(auto& m:motors)if(m.adapter==config.id){auto key=std::make_pair(!Driver{m}.rmd(),m.can);auto old=sim.find(key);
            auto& state=next[key];if(old!=sim.end()&&old->second.profile.id==m.id)state=old->second;
            state.profile=m;state.current=m.current;
        }sim=std::move(next);
    }
    void submit(const Transmission& t){std::lock_guard<std::mutex> l(mutex);
        require(outgoing.size()<512,"CAN transmit backlog exceeded 512 packets");
        int type=(t.frame.id>>24)&31;
        // Streaming samples replace only other streaming samples. Never replace the authored
        // first target or any mode/current/speed setup packet.
        if(t.streamSample)for(auto& pending:outgoing)if(pending.streamSample&&pending.motor==t.motor&&pending.generation==t.generation){pending=t;return;}
        // Replace unsent torque/read samples of the same kind. Setup/target ordering is retained.
        if(t.frame.extended&&(type==1||type==0x11))for(auto it=outgoing.begin();it!=outgoing.end();++it)if(it->motor==t.motor&&it->generation==t.generation&&it->frame.extended&&it->frame.length==t.frame.length&&((it->frame.id>>24)&31)==uint32_t(type)&&(type==1||it->frame==t.frame)){*it=t;return;}
        if(!t.frame.extended&&t.frame.length==8&&(t.frame.data[0]==0x92||t.frame.data[0]==0x9a||t.frame.data[0]==0x9c))
            for(auto& pending:outgoing)if(pending.motor==t.motor&&pending.generation==t.generation&&pending.frame==t.frame){pending=t;return;}
        outgoing.push_back(t);highWater=std::max(highWater,outgoing.size());notify();
    }
    void cancel(const Id& id,uint64_t generation){std::lock_guard<std::mutex> l(mutex);generations[id]=generation;outgoing.erase(std::remove_if(outgoing.begin(),outgoing.end(),[&](const Transmission& t){return t.motor==id&&t.generation!=generation;}),outgoing.end());}
    void request(bool value){std::lock_guard<std::mutex> l(mutex);wanted=value;lastAttempt=-10;notify();}
    void position(const Id& id,double degrees){std::lock_guard<std::mutex> l(mutex);require(config.simulated,"Position injection is simulation only");for(auto& p:sim)if(p.second.profile.id==id){p.second.position=p.second.profile.direction*degrees;p.second.velocity=0;}}
    void presence(const Id& id,bool present){std::lock_guard<std::mutex> l(mutex);require(config.simulated,"Presence injection is simulation only");for(auto& p:sim)if(p.second.profile.id==id)p.second.present=present;}
    void record(Receipt r){
        // The fake adapter exercises the same wire codec as serial TX/RX.
        if(config.simulated){auto wire=slcan(r.frame);wire.pop_back();Frame decoded;require(parseSlcan(wire,decoded),"Fake adapter wire decode failed");r.frame=decoded;}
        if(incoming.size()>=4096)incoming.pop_front();incoming.push_back(r);
    }
    void simulate(const Transmission& t,double now){
        auto& f=t.frame;if(f.length!=8)return;
        int device=f.extended?int(f.id&255):int(f.id)-0x140;
        auto it=sim.find({f.extended,device});if(it==sim.end()||!it->second.present)return;auto& m=it->second;
        if(!f.extended){
            int op=f.data[0];auto reply=f;reply.id=0x240+device;
            if(op==0xa4){m.target=double(littleSigned(f,4,4))*.01;m.speed=littleUnsigned(f,2,2);m.mode=5;m.enabled=true;}
            if(op==0x80)m.enabled=false;
            if(op==0x81)m.target=m.position;
            if(op==0x64){reply.data={0x64,0,0,0,0,0,0,0};} // ROM write does not alter live coordinates before restart.
            if(op==0x92)putLittle(reply,4,uint32_t(int32_t(std::round(m.position*100))),4);
            if(op==0xa4||op==0x9c){reply.data={uint8_t(op),25,0,0,0,0,0,0};putLittle(reply,4,uint16_t(int16_t(m.velocity)),2);putLittle(reply,6,uint16_t(int16_t(std::clamp(m.position,-32767.,32767.))),2);}
            if(op==0x9a)reply.data={0x9a,25,0,0,0xe0,1,0,0};
            record({{},reply,now,false});return;
        }
        int type=(f.id>>24)&31,index=f.data[0]|(f.data[1]<<8);
        if(type==3)m.enabled=true;if(type==4)m.enabled=false;if(type==6&&f.data[0]==1){m.position=0;m.target=0;}
        if(type==0x12){if(index==0x7005)m.mode=f.data[4];if(index==0x7016&&m.enabled)m.target=degrees(decodeFloat(f));if(index==0x7017)m.speed=degrees(decodeFloat(f));if(index==0x7018)m.current=decodeFloat(f);}
        if(type==1){auto scale=packetScales(m.profile.model);m.torque=double((f.id>>8)&65535)*2*scale.torque/65535-scale.torque;
            auto reply=packet(2,m.profile.host,m.profile.can);reply.id|=uint32_t(m.enabled?2:0)<<22;
            auto encode=[](double v,double range){return uint16_t(std::nearbyint((std::clamp(v,-range,range)+range)*65535/(2*range)));};
            double wrapped=std::fmod(radians(m.position)+4*pi,8*pi);if(wrapped<0)wrapped+=8*pi;wrapped-=4*pi;
            std::array<uint16_t,4> values{encode(wrapped,scale.position),encode(radians(m.velocity),scale.velocity),encode(m.torque,scale.torque),250};
            for(int i=0;i<4;++i){reply.data[2*i]=uint8_t(values[i]>>8);reply.data[2*i+1]=uint8_t(values[i]);}record({{},reply,now,false});
        }
        if(type==0x11){auto reply=read(m.profile.host,m.profile.can,index);float value=float(index==0x7019?radians(m.position):radians(m.velocity));uint32_t bits;std::memcpy(&bits,&value,4);for(int i=0;i<4;++i)reply.data[4+i]=uint8_t(bits>>(i*8));record({{},reply,now,false});}
    }
    void run() override {
        while(!threadShouldExit()){
            bool shouldConnect;{std::lock_guard<std::mutex> l(mutex);shouldConnect=wanted;}
            double now=monotonic();
            if(shouldConnect&&!connected&&now-lastAttempt>=2){lastAttempt=now;
                try{if(!config.simulated){auto name=resolvePort(config,discoverPorts());require(!name.empty(),"Remembered adapter missing or ambiguous; select a port in Add Motor");
                        port=std::make_unique<serial::Serial>(name,uint32_t(config.baud),serial::Timeout(0,1,0,200,0));
                        {std::lock_guard<std::mutex> l(mutex);status="Serial open; waiting 2 seconds for adapter startup";}
                        // Match the pinned python-can adapter startup interval, without
                        // holding the execution/queue mutex or delaying another adapter.
                        auto ready=monotonic()+2;
                        while(monotonic()<ready&&wanted&&!threadShouldExit())wait(10);
                        if(!wanted||threadShouldExit()){port.reset();continue;}
                        auto command=std::string("\rC\rS")+slcanBitrate(config.bitrate)+"\rO\r";require(port->write(command)==command.size(),"Incomplete SLCAN initialization write");}
                    std::lock_guard<std::mutex> l(mutex);connected=true;status=config.simulated?"SIMULATION - packet loopback":"SLCAN open; waiting for addressed feedback";outgoing.clear();stream.pending.clear();lastMotorTx.clear();lastStep=now;
                }catch(const std::exception& e){port.reset();std::lock_guard<std::mutex> l(mutex);connected=false;status=e.what();}
            }
            try{
                bool drained;{std::lock_guard<std::mutex> l(mutex);drained=outgoing.empty();}
                if(connected&&!shouldConnect&&drained){if(port){port->write("C\r");port.reset();}std::lock_guard<std::mutex> l(mutex);connected=false;for(auto& p:sim)p.second.enabled=false;status="Disconnected";}
                if(connected){
                    if(port){auto count=std::min<size_t>(4096,port->available());if(count){auto frames=stream.feed(port->read(count));std::lock_guard<std::mutex> l(mutex);for(auto& f:frames)record({{},f,monotonic(),false});}}
                    if(config.simulated){std::lock_guard<std::mutex> l(mutex);double dt=std::clamp(now-lastStep,0.,.05);for(auto& pair:sim){auto& m=pair.second;double old=m.position;if(m.enabled&&m.mode==5)m.position+=std::clamp(m.target-m.position,-(m.speed==0&&Driver{m.profile}.rmd()?65535:m.speed)*dt,(m.speed==0&&Driver{m.profile}.rmd()?65535:m.speed)*dt);m.velocity=dt>0?(m.position-old)/dt:0;}lastStep=now;}
                    // A delayed setup on one motor never blocks packets for another motor.
                    for(int batch=0;batch<32;++batch){Transmission t;bool found=false;
                        {std::lock_guard<std::mutex> l(mutex);std::set<Id> seen;
                            for(auto it=outgoing.begin();it!=outgoing.end();){
                                if(generations[it->motor]>it->generation){it=outgoing.erase(it);continue;}
                                bool earlier=!seen.insert(it->motor).second;
                                if(!earlier&&monotonic()-lastMotorTx[it->motor]>=it->gap){t=*it;outgoing.erase(it);found=true;break;}++it;
                            }
                        }
                        if(!found)break;
                        // No queue/runtime mutex spans OS serial I/O. A cancellation can
                        // invalidate queued frames immediately; one in-flight write may finish.
                        if(port){auto wire=slcan(t.frame);require(port->write(wire)==wire.size(),"Incomplete SLCAN frame write");}
                        double at=monotonic();{std::lock_guard<std::mutex> l(mutex);record({t,t.frame,at,true});if(config.simulated)simulate(t,at);lastMotorTx[t.motor]=at;}
                    }
                }
            }catch(const std::exception& e){port.reset();std::lock_guard<std::mutex> l(mutex);connected=false;outgoing.clear();status=e.what();}
            wait(1);
        }
        if(port){try{port->write("C\r");}catch(...){}port.reset();}
    }
};

struct Service::Impl final : Thread, AsyncUpdater {
    mutable std::mutex mutex;Runtime runtime;std::map<Id,std::unique_ptr<Bus>> buses;std::deque<std::string> monitor;
    struct NativeClock {SequencePlaybackClock clock;double time=0,duration=0;bool playing=false;};
    std::map<Id,NativeClock> clocks;Metrics metrics;double previous=0;std::string message;
    std::map<int,int> pendingDMX;std::unique_ptr<DMXModule> dmxModule;
    bool quitting=false,resetDMX=false;
    Impl():Thread("Midnight continuous execution"){
        runtime.send=[this](const Transmission& t){auto it=buses.find(t.adapter);require(it!=buses.end(),"Adapter not configured");it->second->submit(t);};
        runtime.invalidate=[this](const Id& id,uint64_t g){for(auto& b:buses)b.second->cancel(id,g);};
        runtime.clock=[this](const ClockRequest& r){auto& c=clocks[r.event];c.duration=r.duration;
            if(r.action==ClockRequest::Pause){c.playing=false;return;}
            if(r.action==ClockRequest::Stop){c.playing=false;c.time=0;return;}
            if(r.action==ClockRequest::Restart)c.time=0;c.clock.reset(runtime.now*1000,c.time,250,1);c.playing=true;
        };
        runtime.dmx=[this](int channel,int value){pendingDMX[channel]=value;triggerAsyncUpdate();};startThread();
    }
    ~Impl(){ {std::lock_guard<std::mutex> l(mutex);quitting=true;try{runtime.stopAll();}catch(...){} }
        signalThreadShouldExit();stopThread(1500);cancelPendingUpdate();
        // Give already queued explicit shutdown disables a bounded opportunity to transmit.
        auto deadline=monotonic()+.25;while(monotonic()<deadline){bool pending=false;for(auto& b:buses){std::lock_guard<std::mutex> l(b.second->mutex);pending|=!b.second->outgoing.empty();}if(!pending)break;Thread::sleep(2);}buses.clear();if(dmxModule)dmxModule->clearItem();dmxModule.reset();
    }
    void reconcile(){
        for(auto it=buses.begin();it!=buses.end();){auto a=std::find_if(runtime.document.adapters.begin(),runtime.document.adapters.end(),[&](const Adapter& x){return x.id==it->first;});
            bool same=a!=runtime.document.adapters.end()&&a->port==it->second->config.port&&a->identity==it->second->config.identity&&a->baud==it->second->config.baud&&a->bitrate==it->second->config.bitrate&&a->simulated==it->second->config.simulated;
            if(!same){runtime.connection(it->first,false);it=buses.erase(it);}else ++it;
        }
        for(auto& a:runtime.document.adapters){if(!buses.count(a.id)){auto b=std::make_unique<Bus>(a);b->configureMotors(runtime.document.motors);b->request(a.reconnect);buses[a.id]=std::move(b);}else buses[a.id]->configureMotors(runtime.document.motors);}
    }
    void run() override {
        while(!threadShouldExit()){
            auto now=monotonic();
            {std::lock_guard<std::mutex> l(mutex);if(quitting)break;runtime.now=now;
                if(previous>0){double interval=now-previous;metrics.loops++;metrics.intervalMean+=(interval-metrics.intervalMean)/metrics.loops;metrics.intervalMax=std::max(metrics.intervalMax,interval);}previous=now;
                metrics.queue=0;metrics.feedbackAgeMax=0;
                for(auto& b:buses){std::deque<Bus::Receipt> received;bool connected;std::string status;
                    {std::lock_guard<std::mutex> bl(b.second->mutex);received.swap(b.second->incoming);connected=b.second->connected;status=b.second->status;metrics.queue+=b.second->outgoing.size();metrics.queueMax=std::max(metrics.queueMax,b.second->highWater);}
                    try{runtime.connection(b.first,connected);}catch(const std::exception& e){runtime.error=e.what();}
                    for(auto& r:received){if(r.sent){runtime.transmitted(r.tx,r.at);metrics.tx++;metrics.transmitDelayMax=std::max(metrics.transmitDelayMax,r.at-r.tx.queuedAt);}else{runtime.receive(b.first,r.frame,r.at);metrics.rx++;}
                        auto wire=slcan(r.frame);wire.pop_back();monitor.push_back(std::to_string(r.at)+(r.sent?" TX ":" RX ")+wire);if(monitor.size()>500)monitor.pop_front();}
                }
                for(auto& p:clocks)if(p.second.playing){auto& c=p.second;double time=c.clock.timeForFrame(c.clock.frameAt(now*1000));c.time=std::min(time,c.duration);runtime.timelineTime(p.first,c.time,time>=c.duration);if(time>=c.duration)c.playing=false;}
                try{runtime.step(now);}catch(const std::exception& e){runtime.error=e.what();}
                for(auto& p:runtime.feedback)if(p.second.at>=0)metrics.feedbackAgeMax=std::max(metrics.feedbackAgeMax,now-p.second.at);
            }
            double delay=.004-(monotonic()-now);if(delay>0)wait(std::max(1,int(delay*1000)));
        }
    }
    void handleAsyncUpdate() override {
        std::map<int,int> values;Document doc;bool reset;{std::lock_guard<std::mutex> l(mutex);if(quitting)return;values.swap(pendingDMX);doc=runtime.document;reset=resetDMX;resetDMX=false;}
        if(reset&&dmxModule){dmxModule->clearItem();dmxModule.reset();}
        if(values.empty())return;
        try{
            if(!dmxModule){dmxModule=std::make_unique<DMXModule>();dmxModule->setNiceName("Midnight Art-Net");dmxModule->dmxType->setValueWithKey("Art-Net");dmxModule->setCurrentDMXDevice(new DMXArtNetDevice());}
            auto* dev=dynamic_cast<DMXArtNetDevice*>(dmxModule->dmxDevice.get());require(dev!=nullptr,"Native Art-Net output unavailable");
            dev->inputCC->enabled->setValue(false);dev->networkInterface->setValueWithKey("Auto");dev->outputCC->enabled->setValue(true);dev->remoteHost->setValue(String(doc.dmxHost));dev->remotePort->setValue(doc.dmxPort);
            auto* universe=dmxModule->getUniverse(false,doc.dmxUniverse>>8,(doc.dmxUniverse>>4)&15,doc.dmxUniverse&15,true);
            for(auto& p:values)dmxModule->sendDMXValue(universe,p.first,uint8(p.second));
        }catch(const std::exception& e){std::lock_guard<std::mutex> l(mutex);runtime.error=e.what();}
    }
};
Service::Service():impl(std::make_unique<Impl>()){}
Service::~Service()=default;
Snapshot Service::snapshot() const {
    std::lock_guard<std::mutex> l(impl->mutex);Snapshot s;s.document=impl->runtime.document;s.document.events.erase(std::remove_if(s.document.events.begin(),s.document.events.end(),[](const Event& e){return e.external;}),s.document.events.end());s.motors=impl->runtime.feedback;s.events=impl->runtime.execution;s.metrics=impl->metrics;s.error=impl->runtime.error;s.armed=impl->runtime.armed;s.monitor.assign(impl->monitor.begin(),impl->monitor.end());
    for(auto& b:impl->buses){std::lock_guard<std::mutex> bl(b.second->mutex);s.adapters[b.first]=b.second->status;}return s;
}
void Service::edit(const Document& authored) {
    std::lock_guard<std::mutex> l(impl->mutex);
    Document d=authored;
    for(const auto& e:impl->runtime.document.events)if(e.external)d.events.push_back(e);
    for(auto& e:d.events)prepareCurve(e,d);Runtime check;check.document=d;check.validate();
    // Editing does not restart execution. Changed/deleted running definitions stop locally.
    auto old=impl->runtime.document;
    auto sameEvent=[](Event a,Event b){a.name=b.name;a.collapsed=b.collapsed;a.lane=b.lane;Document da,db;da.events={a};db.events={b};return JSON::toString(encodeDocument(da))==JSON::toString(encodeDocument(db));};
    for(auto& e:old.events){auto found=std::find_if(d.events.begin(),d.events.end(),[&](const Event& x){return x.id==e.id;});if(found==d.events.end()||!sameEvent(e,*found))impl->runtime.stop(e.id);}
    for(auto& m:old.motors){auto found=std::find_if(d.motors.begin(),d.motors.end(),[&](const Motor& x){return x.id==m.id;});
        if(found==d.motors.end()||found->adapter!=m.adapter||found->can!=m.can||found->model!=m.model||found->host!=m.host||protocolOf(*found)!=protocolOf(m)||found->direction!=m.direction){
            impl->runtime.stopMotor(m.id);auto generation=impl->runtime.feedback[m.id].generation;
            impl->runtime.feedback[m.id]={};impl->runtime.feedback[m.id].generation=generation;
        }
    }
    impl->runtime.document=d;for(auto& m:d.motors)if(!impl->runtime.feedback.count(m.id))impl->runtime.feedback[m.id]={};impl->reconcile();
    if(Engine::mainEngine)Engine::mainEngine->changed();
}
void Service::load(const var& v){auto d=decodeDocument(v);std::lock_guard<std::mutex> l(impl->mutex);impl->runtime.replace(std::move(d));impl->clocks.clear();impl->pendingDMX.clear();impl->resetDMX=true;impl->triggerAsyncUpdate();impl->reconcile();}
var Service::save() const {Document d;{std::lock_guard<std::mutex> l(impl->mutex);d=impl->runtime.document;}return encodeDocument(d);}
void Service::clear(bool retain){auto d=retain?snapshot().document:Document{};d.events.clear();load(encodeDocument(d));}
void Service::action(const std::function<void(Runtime&)>& fn){std::lock_guard<std::mutex> l(impl->mutex);impl->runtime.now=monotonic();impl->runtime.error.clear();try{fn(impl->runtime);}catch(const std::exception& e){impl->runtime.error=e.what();}}
void Service::connect(const Id& id,bool enabled){std::lock_guard<std::mutex> l(impl->mutex);auto it=impl->buses.find(id);require(it!=impl->buses.end(),"Save the motor first");if(!enabled)for(auto& m:impl->runtime.document.motors)if(m.adapter==id)impl->runtime.stopMotor(m.id);it->second->request(enabled);}
void Service::injectPosition(const Id& id,double value){std::lock_guard<std::mutex> l(impl->mutex);auto& m=impl->runtime.motor(id);finite(value);impl->buses.at(m.adapter)->position(id,value);}
void Service::simulatePresence(const Id& id,bool present){std::lock_guard<std::mutex> l(impl->mutex);auto& m=impl->runtime.motor(id);impl->buses.at(m.adapter)->presence(id,present);}
void Service::resetMetrics(){std::lock_guard<std::mutex> l(impl->mutex);impl->metrics={};impl->previous=0;for(auto& b:impl->buses){std::lock_guard<std::mutex> bl(b.second->mutex);b.second->highWater=0;}}
void Service::launch(Event e) {
    std::lock_guard<std::mutex> l(impl->mutex);
    auto& r=impl->runtime;r.now=monotonic();r.error.clear();
    // Validate before takeover. Runtime claim ends only conflicting owners, including
    // the whole Haptic pair; existing generation invalidation closes its producer.
    e.external=true;prepareCurve(e,r.document);
    Document proposed=r.document;
    auto old=std::find_if(proposed.events.begin(),proposed.events.end(),[&](const Event& value){return value.id==e.id;});
    if(old!=proposed.events.end())*old=e;else proposed.events.push_back(e);
    Runtime check;check.document=proposed;check.validate();
    auto running=std::find_if(r.document.events.begin(),r.document.events.end(),[&](const Event& value){return value.id==e.id;});
    if(running!=r.document.events.end()&&e.mappingStream&&running->kind==Kind::Move&&r.feedback[e.outputs[0]].owner==e.id&&running->outputs==e.outputs){
        *running=e;r.execution[e.id].status=Status::Running;return;
    }
    if(running!=r.document.events.end()){r.stop(e.id);*running=e;}else r.document.events.push_back(e);
    r.go(e.id);
    require(r.execution.at(e.id).status!=Status::Failed,r.error);
}
void Service::end(const Id& id) {
    std::lock_guard<std::mutex> l(impl->mutex);auto& r=impl->runtime;
    auto found=std::find_if(r.document.events.begin(),r.document.events.end(),[&](const Event& e){return e.id==id&&e.external;});
    if(found==r.document.events.end())return;
    r.stop(id);r.document.events.erase(found);r.execution.erase(id);impl->clocks.erase(id);
}
Service* service(){auto* e=dynamic_cast<ChataigneEngine*>(Engine::mainEngine);return e?e->midnight.get():nullptr;}
}
