#pragma once

#include <string>
#include <vector>

#include "chunk.h"
#include "value.h"

struct ObjString : Obj {
  std::string chars;
  explicit ObjString(std::string s) : chars(std::move(s)) { type = ObjType::STRING; }
};

struct ObjFunction : Obj {
  int arity = 0;
  int upvalueCount = 0;
  Chunk chunk;
  ObjString* name = nullptr;  // nullptr for the top-level script
  ObjFunction() { type = ObjType::FUNCTION; }
};

// A captured variable. While the variable is still live on the VM stack the
// upvalue is "open" and `location` points at the stack slot; when the slot is
// about to die, the value moves into `closed` and `location` repoints there.
// Either way the VM only ever dereferences `location`.
struct ObjUpvalue : Obj {
  Value* location;
  Value closed;
  ObjUpvalue* nextOpen = nullptr;  // VM's sorted list of open upvalues
  explicit ObjUpvalue(Value* slot) : location(slot) { type = ObjType::UPVALUE; }
};

// The runtime representation of a function value: the compiled code plus the
// captured environment. Every function is wrapped in one at runtime.
struct ObjClosure : Obj {
  ObjFunction* function;
  std::vector<ObjUpvalue*> upvalues;
  explicit ObjClosure(ObjFunction* fn)
      : function(fn), upvalues(fn->upvalueCount, nullptr) {
    type = ObjType::CLOSURE;
  }
};

using NativeFn = Value (*)(int argCount, Value* args);

struct ObjNative : Obj {
  NativeFn function;
  explicit ObjNative(NativeFn fn) : function(fn) { type = ObjType::NATIVE; }
};

inline ObjString* asString(const Value& v) { return static_cast<ObjString*>(v.as.obj); }
inline ObjFunction* asFunction(const Value& v) { return static_cast<ObjFunction*>(v.as.obj); }
inline ObjNative* asNative(const Value& v) { return static_cast<ObjNative*>(v.as.obj); }
inline ObjClosure* asClosure(const Value& v) { return static_cast<ObjClosure*>(v.as.obj); }

// Returns the interned string for `s`, allocating it on first sight.
ObjString* copyString(const std::string& s);
