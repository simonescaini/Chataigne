#pragma once
#include "ShowControl.h"
#include <atomic>
namespace midnightShow {
var embedSong(const File&);File songAsset(const CueSpec&);
struct SongClock {double position=0,elapsed=0,length=0,loopStart=0,loopEnd=0,lastCallback=0;uint64_t epoch=1,iteration=0,blocks=0;bool valid=false,playing=false,finished=false;String problem;};
class SongTransport {
public:
    SongTransport(const CueSpec&,uint64_t epoch,bool offline=false);~SongTransport();
    SongClock poll();void pause(bool);void seek(double);void stop();
    void renderOffline(AudioBuffer<float>&,double sampleRate); // Same native clip renderer, without a device; acceptance fixture only.
    static std::shared_ptr<SongTransport> virtualClock(double length,double loopStart=0,double loopEnd=0,int iterations=1);
    void publishVirtual(double position,uint64_t iteration=0,bool valid=true,bool finished=false,uint64_t epoch=0);
    File file()const;
private:
    SongTransport();struct Impl;std::unique_ptr<Impl> impl;
};
}
