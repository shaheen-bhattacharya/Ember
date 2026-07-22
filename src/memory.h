#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

#include "value.h"

class VM;
struct ObjString;

// Owns every heap object the language allocates. Mark-sweep GC: roots are the
// VM's value stack, call frames, and globals; reachability flows through
// function constant pools. Collection is only enabled while the VM is running
// (compile-time allocations become reachable via the compiled script function).
class Heap {
 public:
  Obj* objects = nullptr;
  std::unordered_map<std::string, ObjString*> strings;  // intern table (weak)
  size_t bytesAllocated = 0;
  size_t nextGC = 1024 * 1024;
  VM* vm = nullptr;
  bool gcEnabled = false;
  bool logGC = false;
  bool stressGC = false;  // EMBER_GC_STRESS=1: collect before every allocation

  template <typename T, typename... Args>
  T* allocate(Args&&... args) {
    // Collect *before* allocating so a triggering allocation can't be swept
    // out from under its caller before it becomes reachable.
    if (gcEnabled && (stressGC || bytesAllocated + sizeof(T) > nextGC)) {
      collectGarbage();
    }
    T* obj = new T(std::forward<Args>(args)...);
    obj->gcSize = sizeof(T);
    obj->next = objects;
    objects = obj;
    bytesAllocated += sizeof(T);
    return obj;
  }

  void collectGarbage();
  void markValue(const Value& value);
  void markObject(Obj* object);
  void freeObjects();
};

extern Heap gHeap;
