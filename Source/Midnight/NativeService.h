#pragma once
#include "JuceHeader.h"
#include "NativeCore.h"
#include "MeasuredRecording.h"
#include <memory>

namespace midnightNative {
inline Id uuid() { return Uuid().toString().toStdString(); }
inline double monotonic() { return Time::getMillisecondCounterHiRes()/1000.; }
struct Metrics {
    uint64_t loops=0,tx=0,rx=0; double intervalMean=0,intervalMax=0,transmitDelayMax=0;
    size_t queue=0,queueMax=0; double feedbackAgeMax=0;
};
struct Snapshot {
    Document document; std::map<Id,Feedback> motors; std::map<Id,Execution> events;
    std::map<Id,std::string> adapters; std::vector<std::string> monitor;
    Metrics metrics; std::string error; bool armed=false;
};
var encodeDocument(const Document&);
Document decodeDocument(const var&);
Document demonstration();
std::vector<Port> discoverPorts();
struct NativeHandle {Id id;uint64_t started=0;explicit operator bool()const{return !id.empty()&&started!=0;}};
struct LaunchReceipt {bool accepted=false;NativeHandle handle;std::string reason;};
struct NativeObservation {bool current=false,held=false,succeeded=false;Execution execution;std::string reason;};
class Service {
public:
    Service(); ~Service();
    Snapshot snapshot() const;
    void edit(const Document&); // Validates before changing any existing definitions.
    void load(const var&); var save() const;
    void clear(bool retainHardware=false);
    void action(const std::function<void(Runtime&)>&);
    void connect(const Id&,bool enabled=true);
    void injectPosition(const Id&,double); // Strictly limited to simulated adapters.
    void simulatePresence(const Id&,bool); // Fake adapter only; no physical IO.
    void resetMetrics();
    void startCapture(const Id& motor);
    MeasuredTake stopCapture();
    MeasuredCapture captureSnapshot() const;
    CaptureProgress captureProgress() const;
    LaunchReceipt tryLaunch(Event);
    NativeObservation observe(const NativeHandle&) const;
    bool endOwned(const NativeHandle&);
    void launch(Event); // Per-use definitions; runtime instances are not saved in the hardware registry.
    void end(const Id&);
private:
    struct Impl; std::unique_ptr<Impl> impl;
};
Service* service();
}
