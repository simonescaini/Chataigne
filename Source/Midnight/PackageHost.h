#pragma once
#include "ShowControl.h"
#include <atomic>
#include <condition_variable>
#include <thread>
namespace midnightShow {
var importPackage(const File& folder);void validatePackage(const var& bundle);String packageHash(const var& bundle);String packageSource(const var& bundle);var packageParameters(const var& bundle,const var& authored);
class PackageSession {
public:
 struct Binding {String alias;bool motor=false,required=false;Event event;WeakReference<Parameter> parameter;NativeHandle handle;uint64_t request=0;Outcome last=Outcome::Pending;};
 struct Input {String alias;Id motor;WeakReference<Parameter> parameter;bool live=false;};
 struct Message {enum Type{Request,Complete,Status,Error}type=Request;uint64_t request=0;String alias;var value;bool restart=false;};
 uint64_t generation;var bundle,params;String hash,status;std::map<String,Binding> bindings;std::vector<Input> inputs;
 bool logicComplete=false,failed=false;double lastFrame=-1;String failure;
 PackageSession(Coordinator&,const std::shared_ptr<Run>&);~PackageSession();
 void update(Coordinator&,const std::shared_ptr<Run>&,double now);void stop(bool cleanup=true);bool owns()const;
 void frame(double live,double scene,bool playing,const var& inputs);void requestResult(uint64_t,bool,const String&);std::vector<Message> drain();bool isOpen()const;
private:struct Impl;std::unique_ptr<Impl> impl;
};
}
