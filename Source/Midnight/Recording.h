#pragma once
#include "BehaviourDefinition.h"
namespace midnightNative {
var encodeTake(const MeasuredTake&);
MeasuredTake decodeTake(const var&);
class RecordedMovement {
public:
    Id id=uuid();String name="Recorded movement";bool editable=false;
    std::shared_ptr<const MeasuredTake> original;
    MidnightBehaviour movement{Kind::Move};
    explicit RecordedMovement(MeasuredTake);
    void convertOriginal();
};
class RecordingLibrary {
public:
    std::vector<std::shared_ptr<RecordedMovement>> items;
    std::map<Id,Id> playback;
    ~RecordingLibrary(){clear();}
    std::shared_ptr<RecordedMovement> retain(MeasuredTake,const String&);
    std::shared_ptr<RecordedMovement> find(const Id&)const;
    void play(const Id&);void end(const Id&);void clear();
    var save()const;void load(const var&);
};
RecordingLibrary* recordings();
}
