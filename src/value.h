#pragma once

#include <cstdint>
#include <string>

// Heap-allocated object kinds. Every heap object starts with an Obj header so
// the GC can walk and free them uniformly.
enum class ObjType : uint8_t {
  STRING,
  FUNCTION,
  NATIVE,
};

struct Obj {
  ObjType type;
  bool marked = false;
  size_t gcSize = 0;    // set at allocation; sizeof(Obj) alone would under-count derived types at sweep
  Obj* next = nullptr;  // intrusive list of all heap objects, for the sweep phase
  virtual ~Obj() = default;
};

enum class ValueType : uint8_t {
  NIL,
  BOOL,
  NUMBER,
  OBJ,
};

// A dynamically-typed value: tagged union. NaN-boxing is a planned optimization
// (see docs/DESIGN.md) but the tagged union keeps tier 0 debuggable.
struct Value {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;
  } as;

  Value() : type(ValueType::NIL) { as.number = 0; }

  static Value nil() { return Value(); }
  static Value boolean(bool b) {
    Value v;
    v.type = ValueType::BOOL;
    v.as.boolean = b;
    return v;
  }
  static Value number(double d) {
    Value v;
    v.type = ValueType::NUMBER;
    v.as.number = d;
    return v;
  }
  static Value object(Obj* o) {
    Value v;
    v.type = ValueType::OBJ;
    v.as.obj = o;
    return v;
  }

  bool isNil() const { return type == ValueType::NIL; }
  bool isBool() const { return type == ValueType::BOOL; }
  bool isNumber() const { return type == ValueType::NUMBER; }
  bool isObj() const { return type == ValueType::OBJ; }
  bool isObjType(ObjType t) const { return isObj() && as.obj->type == t; }
  bool isString() const { return isObjType(ObjType::STRING); }
  bool isFunction() const { return isObjType(ObjType::FUNCTION); }
  bool isNative() const { return isObjType(ObjType::NATIVE); }
};

bool valuesEqual(const Value& a, const Value& b);
bool isFalsey(const Value& v);
std::string valueToString(const Value& v);
