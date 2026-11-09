#pragma once
#include "ShowControl.h"
#include "OutputAdmission.h"
class Mapping;
namespace midnightShow {
std::vector<Mapping*> nativeMappings();Mapping& authorMapping(Cue&);var mappingData(const Cue&);
class MappingInstance {
public:
    std::shared_ptr<Mapping> instance;std::shared_ptr<OutputAdmission> gate;uint64_t generation;bool processing=false;
    MappingInstance(const var&,uint64_t);~MappingInstance();void activate();void cease(bool cleanup);bool owns()const;
};
}
