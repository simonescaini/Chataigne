#pragma once
#include "Recording.h"
#include <random>
class Mapping;
namespace midnightShow {
using namespace midnightNative;
enum class CueType { Motion, Reaction, Step, Mapping, Package, Group, End };
enum class GroupMode { Timeline, StartFirst, Enter, Playlist, Random };
enum class Outcome { Pending, Succeeded, Failed, Cancelled, Superseded };
const char* cueTypeName(CueType);const char* outcomeName(Outcome);
struct CueSpec {
    Id id;String name;CueType type=CueType::Motion;GroupMode mode=GroupMode::Timeline;
    double pre=0,post=0;int continuation=0;bool ignore=false,eligible=true,wait=false,deactivate=false,live=false,cleanup=true;
    Id target,recording,sourceMapping;std::vector<Event> actions;Event input;
    std::vector<std::shared_ptr<const CueSpec>> children;var mapping,package,parameters,aliases;String song;
};
class Cue final:public ControllableContainer {
public:
    Id id=uuid();CueType type;StringParameter *title,*target,*song,*sourceMapping;
    FloatParameter *pre,*post;EnumParameter *continuation,*repeat,*groupMode;
    BoolParameter *eligible,*wait,*deactivate,*live,*cleanup;
    MidnightMotorChoice* inputMotor;FloatParameter *reference,*offset,*stiffness,*damping,*effort;BoolParameter *capture,*spring,*rearm;
    std::vector<std::shared_ptr<MidnightBehaviour>> actions;std::vector<std::shared_ptr<Cue>> children;
    Id recording;var mapping,package,parameters,aliases;std::shared_ptr<Mapping> mappingEditor;
    explicit Cue(CueType);std::shared_ptr<MidnightBehaviour> addMotion(Kind=Kind::Move);
    var save()const;static std::shared_ptr<Cue> load(const var&);std::shared_ptr<const CueSpec> snapshot()const;
    void onContainerParameterChanged(Parameter*)override;
};
struct Member {
    Event authored;LaunchReceipt receipt;Outcome outcome=Outcome::Pending;bool held=false,clockEnded=false;std::string detail;
};
class MappingInstance;class PackageSession;class SongTransport;
struct Run {
    Id id=uuid(),parent,clockRoot;uint64_t generation=0;std::shared_ptr<const CueSpec> cue,successor;
    double acceptedAt=0,due=0,startedAt=-1,late=0,outcomeAt=-1;Outcome outcome=Outcome::Pending;
    bool admitted=true,started=false,continuationSent=false,scheduleClosed=false,paused=false,route=false;
    size_t step=0,nextChild=0;uint64_t routeGeneration=0,watermark=0;Id routeInput;
    std::vector<Member> members;std::vector<Id> children;std::vector<Id> stepRuns;
    std::map<Id,NativeHandle> inputs;std::map<Id,uint64_t> counters;
    std::shared_ptr<MappingInstance> mapping;std::shared_ptr<PackageSession> package;std::shared_ptr<SongTransport> song;
    std::vector<Id> order;std::set<Id> dispatched;uint64_t epoch=0,iteration=0;double songPosition=0;bool clockValid=true;
    String detail="Accepted; awaiting action launch";
};
class Coordinator final:private Timer {
public:
    std::vector<std::shared_ptr<Cue>> cues;Id selected,listFocus;std::map<Id,Id> standby;
    std::map<Id,std::shared_ptr<Run>> runs;std::deque<String> log;uint64_t serial=0,revision=0;std::mt19937 random{0x4d49444e};
    std::map<Id,std::set<Id>> randomHistory;std::vector<var> undoStates;
    explicit Coordinator(bool automatic=true);~Coordinator();
    std::shared_ptr<Cue> find(const Id&)const;std::vector<std::shared_ptr<Cue>>* list(const Id& parent);
    std::shared_ptr<Cue> add(CueType,const Id& parent={});void erase(const Id&);Id duplicate(const Id&);void reorder(const Id&,int);void remember();void undo();
    void select(const Id& id){selected=id;}void goTo(const Id& id);Id go();Id direct(const Id&,bool restart=false,bool preview=false);
    Id start(std::shared_ptr<const CueSpec>,Id parent={},Id clockRoot={},bool bypassWait=false,bool suppressContinuation=false);
    void end(const Id&,uint64_t expected=0);void retry(const Id&);void deactivateInput(const Id&,const Id&);void next(const Id&);
    void tick(double now=monotonic());void launch(const std::shared_ptr<Run>&,double);void refresh(const std::shared_ptr<Run>&,double);
    void armStep(const std::shared_ptr<Run>&);void reactionTick(const std::shared_ptr<Run>&,const Snapshot&,double);
    void launchMembers(const std::shared_ptr<Run>&,bool retryOnly=false);void finish(const std::shared_ptr<Run>&,Outcome,const String&);
    bool owns(const Run&)const;bool isFinite(const CueSpec&)const;double clock(const Run&,double)const;bool schedulingAllowed(const Run&)const;
    void continueRun(const std::shared_ptr<Run>&,double);std::shared_ptr<const CueSpec> nextSpec(const Run&)const;
    var save()const;void load(const var&,bool teardown=true);void clear();void note(const String&);
    void startGroup(const std::shared_ptr<Run>&,double);void groupTick(const std::shared_ptr<Run>&,double);
    void pause(const Id&,bool);void seek(const Id&,double);void tap(const Id&);
private:
    void timerCallback()override{tick();}bool ticking=false;
};
Coordinator* coordinator();
}
