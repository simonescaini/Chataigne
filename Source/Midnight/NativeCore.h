// Midnight's device/relationship adapter. No GUI, serial handles or file I/O.
// Protocol and coupling provenance: pinned references in MIDNIGHT_NATIVE_V01_TEST_REPORT.md.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace midnightNative {
using Id = std::string;
constexpr double pi = 3.14159265358979323846;
inline double radians(double d) { return d * pi / 180; }
inline double degrees(double r) { return r * 180 / pi; }
inline void require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
inline void finite(double v) { require(std::isfinite(v), "Enter a finite number"); }
struct Frame {
    uint32_t id = 0;
    std::array<uint8_t, 8> data{};
    bool extended = true;
    uint8_t length = 8;
    bool operator==(const Frame& b) const { return id == b.id && extended == b.extended && length == b.length && length <= 8 && std::equal(data.begin(),data.begin()+length,b.data.begin()); }
};
inline Frame packet(int type, int motor, int host) {
    require(motor >= 0 && motor <= 255 && host >= 0 && host <= 255, "CAN IDs must be 0..255");
    Frame f; f.id = (uint32_t(type) << 24) | (host << 8) | motor; return f;
}
inline Frame read(int motor, int host, int index = 0x7019) {
    auto f = packet(0x11, motor, host); f.data[0] = uint8_t(index); f.data[1] = uint8_t(index >> 8); return f;
}
inline std::array<double, 3> limits(const std::string& model) {
    if(model=="RS04")return {20,90,120}; // CSP/current/torque parameters, not electrical ratings or packet scales.
    require(model == "RS01" || model == "RS02", "Unsupported RobStride model");
    return model == "RS01" ? std::array<double,3>{44,23,17} : std::array<double,3>{33,16,14};
}
inline Frame write(int motor, int host, int index, double value, const std::string& model) {
    finite(value); auto lim = limits(model); auto f = packet(0x12, motor, host);
    f.data[0] = uint8_t(index); f.data[1] = uint8_t(index >> 8);
    if (index == 0x7005) { require(value == 0 || value == 5, "Unsupported drive mode"); f.data[4] = uint8_t(value); }
    else {
        require(index == 0x7016 || index == 0x7017 || index == 0x7018 || index == 0x700b, "Unsupported writable parameter");
        int n = index == 0x7017 ? 0 : index == 0x7018 ? 1 : 2;
        if (index != 0x7016) require(value >= 0 && value <= lim[n], "Outside " + model + " parameter range");
        float v = float(value); require(std::isfinite(v) && (value == 0 || v != 0), "Outside float32 representation");
        uint32_t bits; std::memcpy(&bits, &v, 4);
        for (int i=0;i<4;++i) f.data[4+i] = uint8_t(bits >> (i*8));
    }
    return f;
}
inline double decodeFloat(const Frame& f) {
    uint32_t bits=0; for(int i=0;i<4;++i) bits |= uint32_t(f.data[i+4]) << (i*8);
    float v; std::memcpy(&v,&bits,4); finite(v); return v;
}
struct PacketScales {double position,velocity,torque,kp,kd;};
inline PacketScales packetScales(const std::string& model) {
    limits(model);return model=="RS04"?PacketScales{4*pi,15,120,5000,100}:PacketScales{4*pi,44,17,500,5};
}
inline Frame operation(int motor,const std::string& model,double position,double velocity,double nm,double kp,double kd) {
    auto s=packetScales(model);packet(1,motor,0);
    auto encode=[](double v,double low,double high){finite(v);require(v>=low&&v<=high,"Outside model Type-1 packet range");return uint16_t(std::nearbyint((v-low)*65535/(high-low)));};
    // Python reference uses round-to-even (including zero -> 32768).
    Frame f;f.id=0x01000000|(uint32_t(encode(nm,-s.torque,s.torque))<<8)|motor;
    std::array<uint16_t,4> values{encode(position,-s.position,s.position),encode(velocity,-s.velocity,s.velocity),encode(kp,0,s.kp),encode(kd,0,s.kd)};
    for(int i=0;i<4;++i){f.data[2*i]=uint8_t(values[i]>>8);f.data[2*i+1]=uint8_t(values[i]);}return f;
}
inline Frame torque(int motor,double nm,const std::string& model="RS02") {return operation(motor,model,0,0,nm,0,0);}
inline std::string slcan(const Frame& f) {
    require(f.length<=8&&f.id<=(f.extended?0x1fffffffu:0x7ffu),"Invalid classic CAN frame");
    constexpr char hex[]="0123456789ABCDEF"; std::string s=f.extended?"T":"t";
    for(int i=f.extended?7:2;i>=0;--i) s += hex[(f.id >> (i*4)) & 15]; s += hex[f.length];
    for(int i=0;i<f.length;++i) {auto b=f.data[i];s+=hex[b>>4];s+=hex[b&15];} return s+'\r';
}
inline char slcanBitrate(int bitrate) {
    // Pinned python-can SLCAN profile: S7 is 750 kbit/s, S9 is 83.3 kbit/s.
    const std::array<int,10> rates{10000,20000,50000,100000,125000,250000,500000,750000,1000000,83300};
    auto found=std::find(rates.begin(),rates.end(),bitrate);
    require(found!=rates.end(),"Unsupported SLCAN bitrate");
    return char('0'+std::distance(rates.begin(),found));
}
inline bool parseSlcan(const std::string& s, Frame& f) {
    if(s.empty()||(s[0]!='T'&&s[0]!='t'))return false;
    auto hex=[](char c)->int { if(c>='0'&&c<='9')return c-'0'; if(c>='A'&&c<='F')return c-'A'+10; if(c>='a'&&c<='f')return c-'a'+10; return -1; };
    Frame parsed;parsed.extended=s[0]=='T';int digits=parsed.extended?8:3;
    if(s.size()<size_t(digits+2))return false;
    int length=hex(s[digits+1]);if(length<0||length>8||s.size()!=size_t(digits+2+2*length))return false;
    parsed.length=uint8_t(length);
    for(int i=1;i<=digits;++i){int n=hex(s[i]);if(n<0)return false;parsed.id=(parsed.id<<4)|n;}
    if(parsed.id>(parsed.extended?0x1fffffffu:0x7ffu))return false;
    for(int i=0;i<length;++i){int a=hex(s[digits+2+i*2]),b=hex(s[digits+3+i*2]);if(a<0||b<0)return false;parsed.data[i]=uint8_t(a*16+b);}
    f=parsed;return true;
}
struct Adapter {
    Id id; std::string name="CANable", port, identity;
    int bitrate=1000000, baud=2000000; bool simulated=false, reconnect=true;
};
struct SlcanStream {
    std::string pending;
    std::vector<Frame> feed(const std::string& bytes) {
        pending+=bytes;require(pending.find('\a')==std::string::npos,"CANable rejected a command (BEL)");
        std::vector<Frame> frames;size_t end;
        while((end=pending.find('\r'))!=std::string::npos){Frame f;auto line=pending.substr(0,end);pending.erase(0,end+1);if(parseSlcan(line,f))frames.push_back(f);}
        require(pending.size()<=8192,"Malformed SLCAN receive buffer");return frames;
    }
};
struct Port { std::string port, identity, description, hardware; };
inline std::string resolvePort(const Adapter& a, const std::vector<Port>& ports) {
    std::vector<std::string> matches;
    for(auto& p:ports) if(!a.identity.empty() && p.identity==a.identity) matches.push_back(p.port);
    if(matches.size()==1)return matches[0];
    if(!a.identity.empty()) return {}; // Never fall back to a different adapter now occupying the old COM port.
    for(auto& p:ports) if(p.port==a.port)return p.port; // Explicit manual assignment, no claim of unique identity.
    return {};
}
struct Motor {
    Id id, adapter; std::string name="Cupboard", model="RS02";
    int can=126, host=254;
    double min=0,max=180,speed=30,current=3,tolerance=.5;
    // Empty only for old in-memory callers; serialization writes the effective profile.
    std::string protocol;
    int direction=1;
};
inline std::string defaultProtocol(const std::string& model) {return model=="RMD-L-7025"?"myactuator-v3-v4.2":model=="RS04"?"robstride-private-rs04-260713":"robstride-private-v01";}
inline std::string protocolOf(const Motor& m){return m.protocol.empty()?defaultProtocol(m.model):m.protocol;}
inline uint32_t littleUnsigned(const Frame& f,int offset,int count) {uint32_t value=0;for(int i=0;i<count;++i)value|=uint32_t(f.data[offset+i])<<(8*i);return value;}
inline int64_t littleSigned(const Frame& f,int offset,int count) {auto v=littleUnsigned(f,offset,count);return (v&(uint32_t(1)<<(count*8-1)))?int64_t(v)-(int64_t(1)<<(count*8)):v;}
inline void putLittle(Frame& f,int offset,uint32_t value,int count) {for(int i=0;i<count;++i)f.data[offset+i]=uint8_t(value>>(8*i));}
// A deliberately small protocol boundary. Execution, ownership and scheduling stay common.
struct Driver {
    const Motor& m;
    bool rmd() const {return m.model=="RMD-L-7025";}
    bool haptics() const {return !rmd();}
    bool separateEnable() const {return !rmd();}
    Frame command(uint8_t opcode) const {require(rmd()&&m.can>=1&&m.can<=32,"MyActuator device ID must be 1..32 (not the arbitration ID)");Frame f;f.extended=false;f.id=0x140+m.can;f.data[0]=opcode;return f;}
    void validateSpeed(double speed) const {finite(speed);if(rmd())require(speed>=0&&speed<=65535&&speed==std::floor(speed),"MyActuator A4 speed must be whole degrees/s, 0..65535 (0 means no speed limit)");else write(m.can,m.host,0x7017,radians(speed),m.model);}
    Frame target(double target,double speed) const {
        finite(target);validateSpeed(speed);
        if(!rmd())return write(m.can,m.host,0x7016,radians(m.direction*target),m.model);
        double value=std::round(m.direction*target*100);require(value>=INT32_MIN&&value<=INT32_MAX,"MyActuator target exceeds signed 32-bit centidegrees");
        auto f=command(0xa4);putLittle(f,2,uint32_t(speed),2);putLittle(f,4,uint32_t(int32_t(value)),4);return f;
    }
    Frame disable() const {return rmd()?command(0x80):packet(4,m.can,m.host);}
    Frame positionRead() const {return rmd()?command(0x92):read(m.can,m.host);}
    Frame zero() const {if(rmd())return command(0x64);auto f=packet(6,m.can,m.host);f.data[0]=1;return f;}
    bool routes(const Frame& f) const {return f.length==8&&(rmd()?(!f.extended&&f.id==uint32_t(0x240+m.can)):(f.extended&&(f.id&255)==uint32_t(m.host)&&((f.id>>8)&255)==uint32_t(m.can)));}
    bool targetPacket(const Frame& f) const {return f.length==8&&(rmd()?(!f.extended&&f.id==uint32_t(0x140+m.can)&&f.data[0]==0xa4&&!f.data[1]):(f.extended&&f.id==packet(18,m.can,m.host).id&&f.data[0]==0x16&&f.data[1]==0x70));}
};
enum class Kind { Move, Link, Haptic, DMX, Timeline };
enum class Edge { Manual, Started, Stopped, Completed, Superseded, Position };
enum class Status { Idle, Waiting, Running, Paused, Completed, Stopped, Superseded, Failed };
inline const char* statusName(Status s) {
    switch(s) {case Status::Idle:return "Idle"; case Status::Waiting:return "Waiting";case Status::Running:return "Running";case Status::Paused:return "Paused";case Status::Completed:return "Completed";case Status::Stopped:return "Stopped";case Status::Superseded:return "Superseded";default:return "Failed";}
}
struct Rule {
    Edge edge=Edge::Manual; Id source; double threshold=10,delay=0; int direction=1;
};
struct Event {
    Id id,parent,input; std::string name="Event"; Kind kind=Kind::Move;
    std::vector<Id> outputs;
    double start=0,duration=5,target=45,speed=30;
    bool keepActive=true,all=false,stopAll=false,repeat=false,collapsed=false;
    std::vector<Rule> in,out;
    double stiffness=1,damping=.05,torqueLimit=.5,deadband=.2,offset=0;
    int signA=1,signB=1; bool align=true;
    int channel=1,value=255; double fade=0;
    int lane=0;
    std::vector<Id> copiedExternalReferences;
};
struct Document { std::vector<Adapter> adapters; std::vector<Motor> motors; std::vector<Event> events; std::string dmxHost="127.0.0.1"; int dmxPort=6454,dmxUniverse=0; };
struct Feedback {
    double position=0,at=-1,velocity=0,velocityAt=-1,hapticPosition=0,hapticAt=-1,wrapped=0;
    bool hasWrapped=false,anchored=false;
    double measuredTorque=0,temperature=0,statusAt=-1,requested=0,requestedAt=-1,commandedTorque=0;
    uint32_t faults=0,reportFaults=0; int observedMode=-1;
    bool enableSent=false,connected=false,initialized=false,torqueMode=false;
    double lastCurrent=-1,lastSpeed=-1,positionReadAt=-1,velocityReadAt=-1;
    double statusPollAt=-1;
    std::map<int,double> pendingReplies;
    uint64_t generation=0; Id owner; std::string message="No feedback";
};
struct RuleState { bool latched=false,hasPrevious=false; double previous=0,at=0; uint64_t seen=0; };
struct Execution {
    Status status=Status::Idle; double began=0,time=0,offset=0;
    bool eligible=false,clockEnded=false;
    std::vector<RuleState> in,out;
    std::map<Id,double> followed;
    uint64_t started=0,stopped=0,completed=0,superseded=0;
};
struct Transmission { Id adapter,motor; uint64_t generation=0; Frame frame; double queuedAt=0,gap=0; bool urgent=false; };
struct ClockRequest { Id event; enum Action { Play,Pause,Stop,Restart } action=Play; double duration=0; };
struct Runtime {
    Document document;
    std::map<Id,Feedback> feedback;
    std::map<Id,Execution> execution;
    std::function<void(const Transmission&)> send;
    std::function<void(const Id&,uint64_t)> invalidate;
    std::function<void(const ClockRequest&)> clock;
    std::function<void(int,int)> dmx;
    double now=0,lastPoll=-1;
    uint64_t serial=0;
    bool armed=false;
    std::string error;
    std::array<int,512> dmxValues{};
    std::map<int,Id> dmxOwners;

    Motor& motor(const Id& id) { auto it=std::find_if(document.motors.begin(),document.motors.end(),[&](const Motor& m){return m.id==id;}); require(it!=document.motors.end(),"Missing motor: "+id); return *it; }
    Event& event(const Id& id) { auto it=std::find_if(document.events.begin(),document.events.end(),[&](const Event& e){return e.id==id;}); require(it!=document.events.end(),"Missing event: "+id); return *it; }
    void validate() const;
    void replace(Document d);
    void emit(const Id& id,const Frame& f,double gap=0,bool urgent=false);
    void cancelMotor(const Id& id);
    void disable(const Id& id);
    void stopMotor(const Id& id);
    void haltMotor(const Id& id); // MyActuator 0x81 retains closed-loop state; ordinary Stop/Disable is 0x80.
    void move(const Id& id,double target,double speed,const Id& owner={});
    void enable(const Id& id);
    void mechanicalZero(const Id& id);
    void beginTorque(const Id& id,double limit);
    void receive(const Id& adapter,const Frame& f,double at);
    void connection(const Id& adapter,bool connected);
    void transmitted(const Transmission& t,double at);
    void arm();
    void prepare(const Id& id);
    void go(const Id& id);
    void manualGo(const Id& id);
    void stop(Id id,Status status=Status::Stopped);
    void pause(const Id& id);
    void resume(const Id& id);
    void stopAll();
    void claim(const Event& e);
    void complete(const Id& id);
    void timelineTime(const Id& id,double time,bool ended);
    bool rules(const std::vector<Rule>& definitions,std::vector<RuleState>& states,bool all);
    void step(double time);
    bool blockedByPause(const Event& e);
    std::vector<Id> members(const Event& e) const;
};
inline std::array<double,2> springDamper(const Event& e,double pa,double pb,double va,double vb,double offset) {
    double error=e.signB*pb-e.signA*pa-offset;
    if(std::abs(error)<radians(e.deadband))error=0;
    double t=std::clamp(e.stiffness*error+e.damping*(e.signB*vb-e.signA*va),-e.torqueLimit,e.torqueLimit);
    return {e.signA*t,-e.signB*t};
}
// Remap copied internal references once; external references remain explicit IDs.
inline std::vector<Event> copyEvents(const Document& d,const Id& root,const Id& parent,const std::function<Id()>& uuid) {
    std::set<Id> selected{root}; bool changed=true;
    while(changed) { changed=false; for(auto& e:d.events) if(selected.count(e.parent)&&selected.insert(e.id).second)changed=true; }
    std::map<Id,Id> remap; for(auto& id:selected)remap[id]=uuid();
    std::vector<Event> result;
    for(auto e:d.events) if(selected.count(e.id)) {
        bool top=e.id==root; e.id=remap[e.id]; e.name+=" copy";
        e.parent=top?parent:remap.at(e.parent);
        e.copiedExternalReferences.clear();
        for(auto* rules:{&e.in,&e.out}) for(auto& r:*rules) {
            if(r.edge!=Edge::Position&&remap.count(r.source))r.source=remap[r.source];
            else if(!r.source.empty())e.copiedExternalReferences.push_back(r.source);
        }
        result.push_back(e);
    } return result;
}
}
