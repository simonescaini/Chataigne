// Literal vectors derive from the pinned manufacturer pages in the multi-motor report.
// Included by the production-core test after its Rig helpers.
void multiMotorChecks(){
    {SlcanStream stream;auto a=stream.feed("\rt2418920000");check(a.empty(),"mixed SLCAN stream retains incomplete frame");
        auto b=stream.feed("00A08C0000\rT110001FE8197000000000803F\rt1419\rt241492000000\r");
        check(b.size()==3&&!b[0].extended&&b[1].extended&&b[2].length==4,"real receive stream handles interleaved formats malformed line and short DLC");
        rejects([&]{stream.feed("\a");},"SLCAN adapter rejection remains visible");}
    Rig g;auto& m=g.r.motor("m0");m.model="RMD-L-7025";m.can=1;m.host=-999;m.min=-1000;m.max=1000;
    g.r.motor("m1").can=1;g.r.motor("m1").model="RS04";g.r.motor("m2").model="RS01";
    g.r.validate();Driver driver{m};
    check(slcan(driver.target(360,500))=="t1418A400F401A08C0000\r","MyActuator manual A4 +360 degree / 500 dps exact example");
    check(slcan(driver.target(-90,30))=="t1418A4001E00D8DCFFFF\r","MyActuator negative signed centidegree vector");
    check(slcan(driver.positionRead())=="t14189200000000000000\r","MyActuator multi-turn angular query avoids assumed CPR");
    check(slcan(driver.disable())=="t14188000000000000000\r"&&slcan(driver.command(0x81))=="t14188100000000000000\r","MyActuator output-off and retained-loop stop are distinct");
    check(slcan(driver.zero())=="t14186400000000000000\r","MyActuator ROM zero exact manual vector");
    rejects([&]{driver.target(0,.1);},"reject sub-LSB speed instead of rounding it to unlimited");
    rejects([&]{driver.target(3e7,30);},"reject position outside signed centidegree representation");
    auto position=[&](const std::string& line,double at){auto req=driver.positionRead();g.r.transmitted({"bus",m.id,g.r.feedback[m.id].generation,req},at-.001);Frame f;check(parseSlcan(line,f),"parse independent MyActuator reply fixture");g.r.receive("bus",f,at);};
    position("t241892000000A08C0000",1);check(g.r.feedback[m.id].position==360,"decode manual +360 output-shaft angle without gearing multiplier");
    position("t241892000000D8DCFFFF",1.01);check(g.r.feedback[m.id].position==-90,"decode negative two's complement position");
    m.direction=-1;check(driver.target(90,30).data==std::array<uint8_t,8>{0xa4,0,30,0,0xd8,0xdc,0xff,0xff},"explicit direction inverts target without application zero");
    position("t241892000000D8DCFFFF",1.02);check(g.r.feedback[m.id].position==90,"explicit direction inverts measured coordinates consistently");
    m.direction=1;
    position("t241892000000A08C0000",1.03);position("t241892000000048D0000",1.04);check(g.r.feedback[m.id].position==361,"multi-turn feedback crosses 360 without single-turn wrapping");
    position("t241892000000FFFFFF7F",1.05);check(g.r.feedback[m.id].position==21474836.47,"signed 32-bit angle upper endpoint retains full extent");
    position("t24189200000000000080",1.06);check(g.r.feedback[m.id].position==-21474836.48,"signed angle lower endpoint is not interpreted as unsigned or modulo 360");
    double previous=g.r.feedback[m.id].at;
    for(auto line:{"t14189200000000000000","T0000024189200000000000000","t24289200000000000000","t241492000000"}){
        Frame f;check(parseSlcan(line,f),"wire fixture has declared format and DLC");g.r.receive("bus",f,1.07);
        check(g.r.feedback[m.id].at==previous,"request echo wrong format wrong device and short feedback cannot route");
    }
    Frame wrong;parseSlcan("t24189200000000000000",wrong);g.r.receive("other",wrong,1.08);check(g.r.feedback[m.id].at==previous,"other adapter cannot route MyActuator feedback");
    position("t24189201000000000000",1.09);check(g.r.feedback[m.id].at==previous,"reserved-byte malformed reply cannot become measured zero");
    auto& first=g.add("rmd-first");first.target=-10;first.speed=20;first.keepActive=false;auto& next=g.add("rmd-next");next.target=12;next.in={{Edge::Completed,"rmd-first"}};
    g.r.now=2;g.r.arm();g.r.go("rmd-first");check(g.r.feedback[m.id].requestedAt<0,"queued MyActuator command is not marked transmitted");
    g.flush(2.01);check(g.r.feedback[m.id].requestedAt==2.01&&g.r.feedback[m.id].initialized,"actual A4 TX updates command bookkeeping without RobStride register");
    g.r.step(2.02);check(g.r.execution["rmd-first"].status==Status::Running,"TX alone cannot complete movement");
    position("t24189200000018FCFFFF",2.03);g.r.step(2.03);check(g.r.execution["rmd-first"].status==Status::Completed&&g.r.execution["rmd-next"].status==Status::Running,"fresh measured MyActuator arrival starts subsequent event");
    g.r.haltMotor(m.id);check(g.sent.back().frame.data[0]==0x81&&g.r.execution["rmd-next"].status==Status::Stopped,"retained-loop stop ends event ownership distinctly");
    g.r.disable(m.id);check(g.sent.back().frame.data[0]==0x80,"explicit disable dispatches MyActuator output-off");
    size_t before=g.sent.size();rejects([&]{g.r.enable(m.id);},"unsupported MyActuator separate Enable gives explanation");check(g.sent.size()==before,"unsupported enable emits no guessed opcode");
    g.r.move(m.id,20,15);check(g.sent.back().frame.data[0]==0xa4,"Move after Stop/Disable uses deliberate A4 without 0x88");
    auto status=driver.command(0x9c);g.r.transmitted({"bus",m.id,g.r.feedback[m.id].generation,status},3);Frame f;parseSlcan("t24189C3264000CFE2D00",f);g.r.receive("bus",f,3.01);
    check(std::abs(degrees(g.r.feedback[m.id].velocity)+500)<1e-9&&g.r.feedback[m.id].temperature==50,"signed status speed uses output-shaft dps");
    check(g.r.feedback[m.id].position==-10,"coarse A4/9C angle never overwrites precise 0x92 arrival position");
    g.r.transmitted({"bus",m.id,g.r.feedback[m.id].generation,driver.command(0x9a)},3.02);parseSlcan("t24189A320001E5010400",f);g.r.receive("bus",f,3.03);
    check(g.r.feedback[m.id].reportFaults==4&&!g.r.feedback[m.id].enableSent,"manual low-voltage reply routes fault and disable to MyActuator only");
    check(g.r.feedback["m1"].reportFaults==0,"same-number RobStride motor cannot consume standard fault reply");
    g.r.feedback[m.id].reportFaults=0;g.r.mechanicalZero(m.id);check(g.sent.back().frame==driver.zero()&&g.r.feedback[m.id].position==-10,"zero queues ROM command without resetting local position");
    auto& h=g.add("bad-haptic",Kind::Haptic);h.input=m.id;h.outputs={"m1"};rejects([&]{g.r.validate();},"MyActuator haptics rejected at definition validation");g.r.document.events.pop_back();
    auto bad=g.r.document;bad.motors[1].model="RMD-L-7025";Runtime r;r.document=bad;rejects([&]{r.validate();},"same standard reply address collision rejected");
    bad=g.r.document;bad.adapters[0].bitrate=500000;r.document=bad;rejects([&]{r.validate();},"mixed bus incompatible bitrate rejected without rewriting adapter");
    bad=g.r.document;bad.motors[0].protocol="unknown";r.document=bad;rejects([&]{r.validate();},"unknown protocol cannot silently fall back");
    for(auto a:{"RS01","RS02","RS04"})for(auto b:{"RS01","RS02","RS04"}){
        Rig pair;pair.r.motor("m0").model=a;pair.r.motor("m1").model=b;auto& e=pair.add("h",Kind::Haptic);e.input="m0";e.outputs={"m1"};e.align=false;
        pair.position("m0",0,0);pair.position("m1",10,0);pair.r.validate();pair.r.go("h");pair.flush(.06);pair.r.step(.07);
        auto ta=pair.sent[pair.sent.size()-2].frame,tb=pair.sent.back().frame;
        check(ta==torque(126,radians(10),a)&&tb==torque(127,-radians(10),b),"all nine RobStride haptic pair permutations encode each endpoint scale");
    }
}
