#pragma once
#include "NativeCore.h"
namespace midnightNative {
struct MeasuredSample { double elapsed=0,receivedAt=0,angle=0; uint64_t sequence=0; };
struct CaptureProgress {bool active=false;size_t samples=0;double startedAt=0;};
struct MeasuredTake {
    Id id;Motor motor;Adapter adapter;double startedAt=0,stoppedAt=0;
    std::vector<MeasuredSample> samples;uint64_t staleBeforeStart=0,dropped=0,gaps=0;
    double largestGap=0;bool referenceChanged=false;std::string stoppedReason;
};
// Only accepted mechanical encoder replies enter this adapter. No UI sampling or output.
struct MeasuredCapture {
    MeasuredTake take;bool active=false;static constexpr size_t capacity=1000000;
    void start(Id id,const Motor& motor,const Adapter& adapter,double at){
        require(!active,"A take is already recording; Stop it first");take={};take.id=std::move(id);take.motor=motor;take.adapter=adapter;take.startedAt=at;active=true;
    }
    void sample(const Id& motor,double angle,double receivedAt){
        if(!active||motor!=take.motor.id)return;
        if(receivedAt<take.startedAt){++take.staleBeforeStart;return;}
        if(take.samples.size()>=capacity){++take.dropped;active=false;take.stoppedAt=receivedAt;take.stoppedReason="Capture buffer full; no further samples retained";return;}
        if(!take.samples.empty()){double gap=receivedAt-take.samples.back().receivedAt;take.largestGap=std::max(gap,take.largestGap);if(gap>.15)++take.gaps;}
        take.samples.push_back({receivedAt-take.startedAt,receivedAt,angle,uint64_t(take.samples.size()+1)});
    }
    MeasuredTake stop(double at){active=false;take.stoppedAt=at;if(take.stoppedReason.empty())take.stoppedReason="Operator stopped capture";return take;}
};
}
