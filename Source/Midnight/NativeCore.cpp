#include "NativeCore.h"

namespace midnightNative {
void Runtime::validate() const {
    std::set<Id> ids; std::map<Id,const Adapter*> adapters; std::map<Id,const Motor*> motors; std::map<Id,const Event*> events;
    auto id=[&](const Id& s){require(!s.empty()&&ids.insert(s).second,"Duplicate or empty stable identifier");};
    std::set<std::string> physicalPorts,identities;
    for(auto& a:document.adapters) {
        id(a.id); adapters[a.id]=&a;
        require(a.baud>0,"Serial baud must be positive");
        slcanBitrate(a.bitrate);
        if(!a.simulated&&!a.port.empty())require(physicalPorts.insert(a.port).second,"Use the existing shared adapter for this port");
        if(!a.simulated&&!a.identity.empty())require(identities.insert(a.identity).second,"Use the existing shared adapter for this identity");
    }
    std::set<std::pair<Id,int>> addresses;
    for(auto& m:document.motors) {
        id(m.id); motors[m.id]=&m; require(adapters.count(m.adapter)!=0,"Motor has no adapter");
        packet(3,m.can,m.host); require(addresses.insert({m.adapter,m.can}).second,"Duplicate motor CAN ID on shared adapter");
        finite(m.min);finite(m.max);finite(m.tolerance);require(m.min<m.max&&m.tolerance>0,"MIN must be below MAX; arrival tolerance must be positive");
        write(m.can,m.host,0x7017,radians(m.speed),m.model); write(m.can,m.host,0x7018,m.current,m.model);
    }
    for(auto& e:document.events) {id(e.id); events[e.id]=&e;}
    for(auto& e:document.events) {
        finite(e.start);finite(e.duration);require(e.start>=0&&e.duration>0,"Start must be nonnegative and duration positive");
        if(!e.parent.empty())require(events.count(e.parent)&&events.at(e.parent)->kind==Kind::Timeline,"Parent must be a timeline");
        std::set<Id> ancestry{e.id}; Id p=e.parent;
        while(!p.empty()){require(ancestry.insert(p).second,"Timeline nesting cycle");require(events.count(p)!=0,"Missing parent");p=events.at(p)->parent;}
        require(int(e.kind)>=0&&int(e.kind)<=4,"Unknown event type");
        if(e.kind==Kind::Move||e.kind==Kind::Haptic)require(e.outputs.size()==1,"Movement and haptic events need exactly one output motor");
        if(e.kind==Kind::Link||e.kind==Kind::Haptic)require(motors.count(e.input)!=0&&!e.outputs.empty(),"Select a master/input and follower/output");
        std::set<Id> outputs;
        for(auto& o:e.outputs) {require(motors.count(o)!=0,"Missing output motor");require(outputs.insert(o).second&&o!=e.input,"Select distinct motors");}
        if(e.kind==Kind::Move) {
            auto& m=*motors.at(e.outputs[0]); finite(e.target); require(e.target>=m.min&&e.target<=m.max,"Event target is outside saved MIN/MAX");
            write(m.can,m.host,0x7016,radians(e.target),m.model);write(m.can,m.host,0x7017,radians(e.speed),m.model);
        }
        finite(e.deadband);require(e.deadband>=0,"Deadband must be nonnegative");
        if(e.kind==Kind::Haptic) {
            finite(e.stiffness);finite(e.damping);finite(e.torqueLimit);finite(e.offset);
            require(e.stiffness>=0&&e.damping>=0&&e.torqueLimit>=0,"Haptic parameters must be nonnegative");
            require(std::abs(e.signA)==1&&std::abs(e.signB)==1,"Axis signs must be +1 or -1");
            for(auto& k:{e.input,e.outputs[0]}) {auto& m=*motors.at(k);write(m.can,m.host,0x700b,e.torqueLimit,m.model);}
        }
        require(e.channel>=1&&e.channel<=512&&e.value>=0&&e.value<=255,"DMX channel/value out of range");finite(e.fade);require(e.fade>=0,"Fade cannot be negative");
        for(auto* rr:{&e.in,&e.out})for(auto& r:*rr) {
            finite(r.delay);finite(r.threshold);require(r.delay>=0,"Delay cannot be negative");
            require(int(r.edge)>=0&&int(r.edge)<=5,"Unknown condition type");
            if(r.edge==Edge::Position)require(motors.count(r.source)&&std::abs(r.direction)<=1,"Missing position source or crossing direction");
            else if(r.edge!=Edge::Manual)require(events.count(r.source)&&r.source!=e.id,"Missing or self-referencing condition source");
        }
    }
    require(document.dmxPort>0&&document.dmxPort<=65535&&document.dmxUniverse>=0&&document.dmxUniverse<=32767,"Invalid Art-Net address");
}
void Runtime::replace(Document d) {
    Runtime check; check.document=d;check.validate();stopAll();auto old=feedback;document=std::move(d);execution.clear();feedback.clear();armed=false;
    for(auto& m:document.motors){feedback[m.id]={};feedback[m.id].generation=old[m.id].generation;} dmxOwners.clear();dmxValues.fill(0);error.clear();
}
void Runtime::emit(const Id& id,const Frame& f,double gap,bool urgent) {
    auto& m=motor(id);auto& s=feedback[id];require(s.connected,"Adapter is disconnected");
    if(send)send({m.adapter,id,s.generation,f,now,gap,urgent});
}
void Runtime::cancelMotor(const Id& id) {
    auto& s=feedback[id];++s.generation;if(invalidate)invalidate(id,s.generation);
}
void Runtime::disable(const Id& id) {
    auto& m=motor(id);auto& s=feedback[id];cancelMotor(id);
    if(s.connected){if(s.torqueMode)emit(id,torque(m.can,0),0,true);emit(id,packet(4,m.can,m.host),0,true);}
    s.enableSent=false;s.initialized=false;s.torqueMode=false;s.commandedTorque=0;s.owner.clear();
    s.message="Stop/disable queued; torque release requires delivery and drive response";
}
void Runtime::stopMotor(const Id& id) {
    auto owner=feedback[id].owner;
    if(!owner.empty()&&execution.count(owner))stop(owner);else disable(id);
}
void Runtime::move(const Id& id,double target,double speed,const Id& owner) {
    auto& m=motor(id);finite(target);require(target>=m.min&&target<=m.max,"Target outside saved MIN/MAX");
    auto destination=write(m.can,m.host,0x7016,radians(target),m.model);
    auto velocity=write(m.can,m.host,0x7017,radians(speed),m.model);
    auto current=write(m.can,m.host,0x7018,m.current,m.model);
    auto& s=feedback[id];require(s.connected,"Adapter disconnected");require(!s.faults&&!s.reportFaults,"Drive reports a fault; resolve it before movement");
    if(s.owner!=owner&&!s.owner.empty())stop(s.owner,Status::Superseded);
    // Latest deliberate command wins. Incomplete old setup is cancelled and rebuilt.
    cancelMotor(id);s.owner=owner;
    if(s.torqueMode){emit(id,torque(m.can,0),0,true);s.initialized=false;s.torqueMode=false;}
    if(!s.initialized) {
        emit(id,packet(4,m.can,m.host));emit(id,write(m.can,m.host,0x7005,5,m.model),.01);
        emit(id,current,.01);emit(id,velocity,.01);emit(id,packet(3,m.can,m.host),.01);
        emit(id,velocity,.01);emit(id,destination);
    } else {
        if(s.lastCurrent!=m.current)emit(id,current);
        emit(id,velocity,s.lastCurrent!=m.current?.01:0);
        if(!s.enableSent){emit(id,packet(3,m.can,m.host),.01);emit(id,velocity,.01);}
        emit(id,destination);
    }
    s.requested=target;s.requestedAt=-1;s.lastSpeed=speed;s.lastCurrent=m.current;
    s.message="Movement queued; transmission is not drive acceptance";
}
void Runtime::enable(const Id& id) {
    auto& m=motor(id);auto& s=feedback[id];require(!s.faults&&!s.reportFaults,"Drive fault remains active");
    stopMotor(id);emit(id,packet(4,m.can,m.host));emit(id,write(m.can,m.host,0x7005,5,m.model),.01);
    emit(id,write(m.can,m.host,0x7018,m.current,m.model),.01);emit(id,write(m.can,m.host,0x7017,radians(m.speed),m.model),.01);
    emit(id,packet(3,m.can,m.host),.01);s.message="Enable queued; drive may retain its previous target";
}
void Runtime::mechanicalZero(const Id& id) {
    auto& m=motor(id);stopMotor(id);auto f=packet(6,m.can,m.host);f.data[0]=1;emit(id,f,.01);
    feedback[id].at=-1;feedback[id].hasWrapped=false;feedback[id].anchored=false;
    feedback[id].message="Mechanical zero command queued; persistence across power cycles is unverified";
}
void Runtime::beginTorque(const Id& id,double limit) {
    auto& m=motor(id);auto& s=feedback[id];auto cap=write(m.can,m.host,0x700b,limit,m.model);
    require(!s.faults&&!s.reportFaults,"Drive fault remains active");cancelMotor(id);
    emit(id,packet(4,m.can,m.host));emit(id,write(m.can,m.host,0x7005,0,m.model),.01);emit(id,cap,.01);
    auto report=packet(0x18,m.can,m.host);report.data[0]=1;emit(id,report,.01);emit(id,packet(3,m.can,m.host),.01);emit(id,torque(m.can,0),.01);
    s.initialized=false;s.enableSent=false;s.torqueMode=true;
}
void Runtime::transmitted(const Transmission& t,double at) {
    auto it=feedback.find(t.motor);if(it==feedback.end()||it->second.generation!=t.generation)return;
    auto& s=it->second;int type=(t.frame.id>>24)&31;int index=t.frame.data[0]|(t.frame.data[1]<<8);
    if(type==3)s.enableSent=true;
    if(type==4)s.enableSent=false;
    if(type==0x12&&index==0x7016){s.initialized=true;s.requestedAt=at;s.message="Target sent; check measured position and drive status";}
    if(type==0x11&&index==0x7019)s.positionReadAt=at;
    if(type==0x11&&index==0x701b)s.velocityReadAt=at;
}
void Runtime::connection(const Id& adapter,bool connected) {
    for(auto& m:document.motors)if(m.adapter==adapter) {
        auto& s=feedback[m.id];if(s.connected==connected)continue;
        if(!connected){s.connected=false;stopMotor(m.id);}
        s.connected=connected;s.initialized=false;s.enableSent=false;s.at=-1;s.hapticAt=-1;s.velocityAt=-1;
        s.message=connected?"Connected for observation; no movement restored":"Disconnected; no command can be delivered";
    }
}
void Runtime::receive(const Id& adapter,const Frame& f,double at) {
    int type=(f.id>>24)&31;if(type!=2&&type!=0x11&&type!=0x15)return;
    for(auto& m:document.motors)if(m.adapter==adapter&&(f.id&255)==uint32_t(m.host)&&((f.id>>8)&255)==uint32_t(m.can)) {
        auto& s=feedback[m.id];
        try {
            if(type==0x11) {
                if(((f.id>>16)&255)||f.data[2]||f.data[3])throw std::runtime_error("Rejected/malformed parameter response");
                int index=f.data[0]|(f.data[1]<<8);double v=decodeFloat(f);
                if(index==0x7019&&s.positionReadAt>=0&&at>=s.positionReadAt&&at-s.positionReadAt<=1) {
                    s.position=degrees(v);s.at=at;s.positionReadAt=-1;
                    if(s.hasWrapped&&!s.anchored){s.hapticPosition+=std::nearbyint((v-s.hapticPosition)/(8*pi))*(8*pi);s.anchored=true;}
                }
                if(index==0x701b&&s.velocityReadAt>=0&&at>=s.velocityReadAt&&at-s.velocityReadAt<=1){s.velocity=v;s.velocityAt=at;s.velocityReadAt=-1;}
            } else if(type==2) {
                auto u=[&](int i){return double((f.data[i]<<8)|f.data[i+1])/65535;};
                double wrapped=u(0)*8*pi-4*pi;
                if(!s.hasWrapped){s.hapticPosition=wrapped;if(s.at>=0){s.hapticPosition+=std::nearbyint((radians(s.position)-wrapped)/(8*pi))*8*pi;s.anchored=true;}}
                else {double delta=wrapped-s.wrapped; if(delta>4*pi)delta-=8*pi;else if(delta< -4*pi)delta+=8*pi;s.hapticPosition+=delta;}
                s.hasWrapped=true;s.wrapped=wrapped;s.hapticAt=at;s.velocity=u(2)*88-44;s.velocityAt=at;
                s.measuredTorque=u(4)*34-17;s.temperature=((f.data[6]<<8)|f.data[7])/10.;s.statusAt=at;
                s.faults=(f.id>>16)&63;s.observedMode=(f.id>>22)&3;
            } else {s.reportFaults=0;for(int i=0;i<4;++i)s.reportFaults|=uint32_t(f.data[i])<<(8*i);s.statusAt=at;}
            if(s.faults||s.reportFaults){auto owner=s.owner;if(!owner.empty())stop(owner,Status::Failed);else disable(m.id);s.message="Drive reported a fault; disable attempted";}
        } catch(const std::exception& e){s.message=e.what();}
    }
}
std::vector<Id> Runtime::members(const Event& e) const {
    auto result=e.outputs;if(e.kind==Kind::Haptic||e.kind==Kind::Link)result.push_back(e.input);return result;
}
void Runtime::prepare(const Id& id) {
    auto& e=event(id);auto& x=execution[id];x.status=Status::Waiting;x.eligible=e.parent.empty();x.clockEnded=false;x.time=0;
    x.in.assign(e.in.size(),{});x.out.assign(e.out.size(),{});x.followed.clear();
    auto init=[&](const std::vector<Rule>& rules,std::vector<RuleState>& states) {
        for(size_t i=0;i<rules.size();++i) {auto& r=rules[i];auto& s=states[i];
            if(r.edge==Edge::Position){auto& f=feedback[r.source];s.hasPrevious=f.at>=0;s.previous=f.position;}
            else if(r.edge!=Edge::Manual){auto& source=execution[r.source];s.seen=r.edge==Edge::Started?source.started:r.edge==Edge::Stopped?source.stopped:r.edge==Edge::Completed?source.completed:source.superseded;}
        }
    };init(e.in,x.in);init(e.out,x.out);
}
void Runtime::arm(){armed=true;for(auto& e:document.events)if(execution[e.id].status!=Status::Running&&execution[e.id].status!=Status::Paused)prepare(e.id);}
void Runtime::claim(const Event& e) {
    std::set<Id> previous;
    for(auto& id:members(e)) {auto& f=feedback[id];require(f.connected,"Connect all event motors first");require(!f.faults&&!f.reportFaults,"A motor reports a drive fault");if(!f.owner.empty()&&f.owner!=e.id)previous.insert(f.owner);}
    for(auto& id:previous)stop(id,Status::Superseded);
    for(auto& id:members(e))feedback[id].owner=e.id;
}
void Runtime::go(const Id& id) {
    auto& e=event(id);auto& x=execution[id];
    if(x.status==Status::Paused){resume(id);return;}
    if(x.status==Status::Running)stop(id);
    prepare(id);x.began=now;x.status=Status::Running;x.started=++serial;
    try {
        if(e.kind==Kind::Move){claim(e);move(e.outputs[0],e.target,e.speed,e.id);}
        if(e.kind==Kind::Link||e.kind==Kind::Haptic) {
            for(auto& key:members(e))require(feedback[key].at>=0&&now-feedback[key].at<.15,"Fresh mechanical feedback is required for interaction activation");
            claim(e);
            if(e.kind==Kind::Haptic){auto& a=feedback[e.input];auto& b=feedback[e.outputs[0]];x.offset=e.align?e.signB*radians(b.position)-e.signA*radians(a.position):radians(e.offset);for(auto& key:members(e))beginTorque(key,e.torqueLimit);}
        }
        if(e.kind==Kind::DMX){auto previous=dmxOwners.find(e.channel);if(previous!=dmxOwners.end()&&!previous->second.empty()&&previous->second!=id)stop(previous->second,Status::Superseded);dmxOwners[e.channel]=id;x.offset=dmxValues[e.channel-1];}
        if(e.kind==Kind::Timeline){for(auto& child:document.events)if(child.parent==id)prepare(child.id);if(clock)clock({id,ClockRequest::Restart,e.duration});}
    }catch(const std::exception& err){error=e.name+": "+err.what();stop(id,Status::Failed);}
}
void Runtime::manualGo(const Id& id) {
    auto& e=event(id);auto& x=execution[id];
    if(x.status==Status::Paused){resume(id);return;}
    bool explicitManual=std::any_of(e.in.begin(),e.in.end(),[](const Rule& r){return r.edge==Edge::Manual;});
    if(!explicitManual){go(id);return;}
    if(x.status!=Status::Waiting){if(x.status==Status::Running)stop(id);prepare(id);}
    for(size_t i=0;i<e.in.size();++i)if(e.in[i].edge==Edge::Manual&&!x.in[i].latched){x.in[i].latched=true;x.in[i].at=now;}
    if(!e.parent.empty()){auto& parent=execution[e.parent];x.eligible=parent.status==Status::Running&&parent.time>=e.start;}
    if(x.eligible&&!blockedByPause(e)&&rules(e.in,x.in,e.all))go(id);
}
void Runtime::stop(Id id,Status status) {
    auto& e=event(id);auto& x=execution[id];
    // A completed movement can still own an enabled drive.
    if(e.kind==Kind::Timeline){if(clock)clock({id,ClockRequest::Stop,e.duration});for(auto& child:document.events)if(child.parent==id)stop(child.id,status);}
    if(e.kind==Kind::Move||e.kind==Kind::Link||e.kind==Kind::Haptic)for(auto& m:members(e))if(feedback[m].owner==id)disable(m);
    if(e.kind==Kind::DMX){auto owner=dmxOwners.find(e.channel);if(owner!=dmxOwners.end()&&owner->second==id)dmxOwners.erase(owner);}
    x.status=status;if(status==Status::Stopped)x.stopped=++serial;if(status==Status::Superseded)x.superseded=++serial;
}
void Runtime::complete(const Id& id) {
    auto& e=event(id);auto& x=execution[id];x.status=Status::Completed;x.completed=++serial;
    if(e.kind==Kind::Move&&!e.keepActive&&feedback[e.outputs[0]].owner==id)disable(e.outputs[0]);
    if(e.kind==Kind::Timeline)for(auto& child:document.events)if(child.parent==id&&execution[child.id].status==Status::Waiting)stop(child.id);
}
void Runtime::pause(const Id& id){auto& e=event(id);if(e.kind!=Kind::Timeline||execution[id].status!=Status::Running)return;execution[id].status=Status::Paused;if(clock)clock({id,ClockRequest::Pause,e.duration});for(auto& c:document.events)if(c.parent==id&&c.kind==Kind::Timeline)pause(c.id);}
void Runtime::resume(const Id& id){auto& e=event(id);if(execution[id].status!=Status::Paused)return;execution[id].status=Status::Running;if(clock)clock({id,ClockRequest::Play,e.duration});for(auto& c:document.events)if(c.parent==id&&c.kind==Kind::Timeline)resume(c.id);}
void Runtime::stopAll(){for(auto& e:document.events)if(e.parent.empty())stop(e.id);for(auto& m:document.motors)if(feedback[m.id].connected)disable(m.id);armed=false;}
void Runtime::timelineTime(const Id& id,double t,bool ended){auto& x=execution[id];if(x.status!=Status::Running)return;x.time=t;x.clockEnded=ended;}
bool Runtime::blockedByPause(const Event& e){Id p=e.parent;while(!p.empty()){if(execution[p].status==Status::Paused)return true;p=event(p).parent;}return false;}
bool Runtime::rules(const std::vector<Rule>& definitions,std::vector<RuleState>& states,bool all) {
    if(definitions.empty())return false;bool any=false,every=true;
    for(size_t i=0;i<definitions.size();++i){auto& r=definitions[i];auto& s=states[i];bool hit=false;
        if(r.edge==Edge::Position){auto& f=feedback[r.source];if(f.at>=0&&now-f.at<.15){if(s.hasPrevious){bool up=s.previous<r.threshold&&f.position>=r.threshold;bool down=s.previous>r.threshold&&f.position<=r.threshold;hit=(r.direction>=0&&up)||(r.direction<=0&&down);}s.previous=f.position;s.hasPrevious=true;}}
        else if(r.edge!=Edge::Manual){auto& x=execution[r.source];uint64_t v=r.edge==Edge::Started?x.started:r.edge==Edge::Stopped?x.stopped:r.edge==Edge::Completed?x.completed:x.superseded;hit=v>s.seen;s.seen=v;}
        if(hit&&!s.latched){s.latched=true;s.at=now;}bool ready=s.latched&&now-s.at>=r.delay;any|=ready;every&=ready;
    }return all?every:any;
}
void Runtime::step(double time) {
    now=time;
    if(now-lastPoll>=.02){lastPoll=now;for(auto& m:document.motors)if(feedback[m.id].connected){emit(m.id,read(m.can,m.host));if(feedback[m.id].torqueMode)emit(m.id,read(m.can,m.host,0x701b));}}
    for(auto& e:document.events){auto& x=execution[e.id];
        try {
            if(x.status==Status::Waiting&&!blockedByPause(e)) {
                if(!e.parent.empty()){auto& p=execution[e.parent];x.eligible=p.status==Status::Running&&p.time>=e.start;}
                bool ready=rules(e.in,x.in,e.all);
                if(x.eligible&&((e.in.empty()&&!e.parent.empty())||ready))go(e.id);
            }
            if(x.status!=Status::Running)continue;
            if(!blockedByPause(e)&&rules(e.out,x.out,e.stopAll)){stop(e.id);continue;}
            if(e.kind==Kind::Move) {
                auto& f=feedback[e.outputs[0]];
                if(f.owner!=e.id){stop(e.id,Status::Superseded);continue;}
                if(f.requestedAt>=0&&f.at>=f.requestedAt&&now-f.at<.15&&std::abs(f.position-e.target)<=motor(e.outputs[0]).tolerance)complete(e.id);
            } else if(e.kind==Kind::Link) {
                auto& f=feedback[e.input];require(f.at>=0&&now-f.at<.15,"Master feedback stale; linking stopped");
                for(auto& output:e.outputs)if(!x.followed.count(output)||std::abs(x.followed[output]-f.position)>=e.deadband){
                    // Preserve an in-flight first setup. The next observed master sample
                    // replaces its target immediately after that setup reaches the wire.
                    if(x.followed.count(output)&&feedback[output].requestedAt<0)continue;
                    move(output,f.position,motor(output).speed,e.id);x.followed[output]=f.position;
                }
            } else if(e.kind==Kind::Haptic) {
                auto& a=feedback[e.input];auto& b=feedback[e.outputs[0]];
                auto p=[&](Feedback& f){if(f.hapticAt>=0&&now-f.hapticAt<.15)return f.hapticPosition;require(f.at>=0&&now-f.at<.15,"Haptic position feedback stale");return radians(f.position);};
                double pa=p(a),pb=p(b);
                // Differentiate only genuinely new positions if velocity feedback is unavailable.
                auto velocity=[&](const Id& key,Feedback& f,double pos){double at=std::max(f.at,f.hapticAt);auto pk=key+"/pos",tk=key+"/time",vk=key+"/vel";double v=0;if(x.followed.count(tk)&&at>x.followed[tk])v=(pos-x.followed[pk])/(at-x.followed[tk]);else if(x.followed.count(vk))v=x.followed[vk];x.followed[pk]=pos;x.followed[tk]=at;x.followed[vk]=v;return f.velocityAt>=0&&now-f.velocityAt<.15?f.velocity:v;};
                auto t=springDamper(e,pa,pb,velocity(e.input,a,pa),velocity(e.outputs[0],b,pb),x.offset);
                if(a.enableSent&&b.enableSent){emit(e.input,torque(motor(e.input).can,t[0]));emit(e.outputs[0],torque(motor(e.outputs[0]).can,t[1]));a.commandedTorque=t[0];b.commandedTorque=t[1];}
            } else if(e.kind==Kind::DMX){double f=e.fade==0?1:std::clamp((now-x.began)/e.fade,0.,1.);int value=int(std::lround(x.offset+(e.value-x.offset)*f));if(dmxValues[e.channel-1]!=value){dmxValues[e.channel-1]=value;if(dmx)dmx(e.channel,value);}if(f>=1)complete(e.id);}
            else if(e.kind==Kind::Timeline&&x.clockEnded)complete(e.id);
        }catch(const std::exception& err){error=e.name+": "+err.what();stop(e.id,Status::Failed);}
        if(e.repeat&&x.status==Status::Completed)prepare(e.id);
    }
}
}
