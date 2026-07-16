#pragma once

#include <string>

#include "chunk.h"
#include "value.h"

struct ObjString : Obj {
  std::string chars;
  explicit ObjString(std::string s) : chars(std::move(s)) { type = ObjType::STRING; }
};

struct ObjFunction : Obj {
  int arity = 0;
  Chunk chunk;
  ObjString* name = nullptr;  // nullptr for the top-level script
  ObjFunction() { type = ObjType::FUNCTION; }
};

using NativeFn = Value (*)(int argCount, Value* args);

struct ObjNative : Obj {
  NativeFn function;
  explicit ObjNative(NativeFn fn) : function(fn) { type = ObjType::NATIVE; }
};

inline ObjString* asString(const Value& v) { return static_cast<ObjString*>(v.as.obj); }
inline ObjFunction* asFunction(const Value& v) { return static_cast<ObjFunction*>(v.as.obj); }
inline ObjNative* asNative(const Value& v) { return static_cast<ObjNative*>(v.as.obj); }

// Returns the interned string for `s`, allocating it on first sight.
ObjString* copyString(const std::string& s);
