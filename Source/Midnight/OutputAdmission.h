#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>
namespace midnightShow {
// Only cue-owned clones install this gate. Native mappings retain their prior behavior.
struct OutputAdmission {
    std::recursive_mutex mutex;bool open=false;uint64_t generation=0,dispatched=0,rejected=0;
    bool allows(uint64_t expected){std::lock_guard<std::recursive_mutex> lock(mutex);return open&&generation==expected;}
    bool dispatch(uint64_t expected,const std::function<void()>& fn){std::lock_guard<std::recursive_mutex> lock(mutex);if(!open||generation!=expected){++rejected;return false;}fn();++dispatched;return true;}
    void close(){std::lock_guard<std::recursive_mutex> lock(mutex);open=false;}
};
}
