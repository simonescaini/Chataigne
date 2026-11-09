#pragma once
#include "ShowControl.h"
#include "OutputAdmission.h"
class Mapping;
namespace midnightShow {
std::vector<Mapping*> nativeMappings();Mapping& authorMapping(Cue&);var mappingData(const Cue&);
struct MappingIdentity {
 std::vector<WeakReference<Controllable>> controls;std::vector<WeakReference<ControllableContainer>> containers;std::vector<WeakReference<CommandDefinition>> commands;
 bool valid()const;bool matches(const MappingIdentity&)const;
};
class MappingInstance {
public:
    std::shared_ptr<Mapping> instance;std::shared_ptr<OutputAdmission> gate;uint64_t generation;bool processing=false;std::shared_ptr<MappingIdentity> identity;
    MappingInstance(const var&,uint64_t);~MappingInstance();void activate();void cease(bool cleanup);bool owns()const;
};
}
