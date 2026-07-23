#pragma once

#include <string>
#include <vector>

#include "chunk.h"
#include "value.h"

struct ObjString : Obj {
  std::string chars;
  explicit ObjString(std::string s) : chars(std::move(s)) { type = ObjType::STRING; }
};

// Observed-type bitmask recorded per bytecode site. A site whose mask has one
// bit set is monomorphic — the signal the optimizing tier specializes on.
enum TypeFeedbackBits : uint8_t {
  FB_NIL = 1,
  FB_BOOL = 2,
  FB_NUMBER = 4,
  FB_STRING = 8,
  FB_CALLABLE = 16,
  FB_OTHER = 32,
};

// Monomorphic call-site cache: remembers the one callee seen at this OP_CALL,
// or degrades to polymorphic if a second one shows up.
struct CallSiteFeedback {
  int offset = 0;
  Obj* target = nullptr;  // ObjFunction* or ObjNative*; nullptr once polymorphic
  uint32_t count = 0;
  bool polymorphic = false;
};

constexpr uint32_t kHotCallThreshold = 1000;
constexpr uint32_t kHotBackEdgeThreshold = 10000;

// Tier-1 compilation state. UNSUPPORTED means the function uses bytecode the
// baseline JIT doesn't handle (it stays on the interpreter forever).
enum class JitState : uint8_t { NONE, COMPILED, UNSUPPORTED };

struct ObjFunction : Obj {
  int arity = 0;
  int upvalueCount = 0;
  Chunk chunk;
  ObjString* name = nullptr;  // nullptr for the top-level script

  // Tier-1 JIT: entry point of compiled code (owned by the JIT's code
  // registry, which outlives all functions).
  JitState jitState = JitState::NONE;
  void* jitEntry = nullptr;

  // Profiling, recorded by the interpreter and consumed by the JIT tiers.
  uint32_t callCount = 0;
  uint32_t backEdges = 0;
  bool hot = false;
  std::vector<uint8_t> feedback;  // type mask per bytecode offset (op sites only)
  std::vector<CallSiteFeedback> callSites;

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

struct ObjArray : Obj {
  std::vector<Value> items;
  ObjArray() { type = ObjType::ARRAY; }
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
inline ObjArray* asArray(const Value& v) { return static_cast<ObjArray*>(v.as.obj); }

// Returns the interned string for `s`, allocating it on first sight.
ObjString* copyString(const std::string& s);
