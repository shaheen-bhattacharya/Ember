#include "value.h"

#include <cmath>
#include <cstdio>

#include "object.h"

bool valuesEqual(const Value& a, const Value& b) {
  if (a.type != b.type) return false;
  switch (a.type) {
    case ValueType::NIL: return true;
    case ValueType::BOOL: return a.as.boolean == b.as.boolean;
    case ValueType::NUMBER: return a.as.number == b.as.number;
    // Strings are interned, so pointer identity is string equality.
    case ValueType::OBJ: return a.as.obj == b.as.obj;
  }
  return false;
}

bool isFalsey(const Value& v) {
  return v.isNil() || (v.isBool() && !v.as.boolean);
}

const char* typeName(const Value& v) {
  switch (v.type) {
    case ValueType::NIL: return "nil";
    case ValueType::BOOL: return "boolean";
    case ValueType::NUMBER: return "number";
    case ValueType::OBJ:
      switch (v.as.obj->type) {
        case ObjType::STRING: return "string";
        case ObjType::FUNCTION:
        case ObjType::CLOSURE: return "function";
        case ObjType::NATIVE: return "native function";
        case ObjType::UPVALUE: return "upvalue";
        case ObjType::ARRAY: return "array";
      }
  }
  return "value";
}

static std::string valueToStringDepth(const Value& v, int depth);

static std::string arrayToString(ObjArray* array, int depth) {
  // Depth cap keeps self-referential arrays printable.
  if (depth >= 3) return "[...]";
  std::string out = "[";
  for (size_t i = 0; i < array->items.size(); i++) {
    if (i > 0) out += ", ";
    out += valueToStringDepth(array->items[i], depth + 1);
  }
  return out + "]";
}

std::string valueToString(const Value& v) {
  return valueToStringDepth(v, 0);
}

static std::string valueToStringDepth(const Value& v, int depth) {
  switch (v.type) {
    case ValueType::NIL:
      return "nil";
    case ValueType::BOOL:
      return v.as.boolean ? "true" : "false";
    case ValueType::NUMBER: {
      // Normalize NaN: x86 produces negative NaNs from 0/0 and glibc prints
      // them as "-nan"; the sign of a NaN is meaningless, so hide it.
      if (std::isnan(v.as.number)) return "nan";
      char buf[32];
      snprintf(buf, sizeof(buf), "%.14g", v.as.number);
      return buf;
    }
    case ValueType::OBJ:
      switch (v.as.obj->type) {
        case ObjType::STRING:
          return asString(v)->chars;
        case ObjType::FUNCTION: {
          ObjFunction* fn = asFunction(v);
          if (fn->name == nullptr) return "<script>";
          return "<fn " + fn->name->chars + ">";
        }
        case ObjType::NATIVE:
          return "<native fn>";
        case ObjType::CLOSURE: {
          ObjFunction* fn = asClosure(v)->function;
          if (fn->name == nullptr) return "<script>";
          return "<fn " + fn->name->chars + ">";
        }
        case ObjType::UPVALUE:
          return "<upvalue>";
        case ObjType::ARRAY:
          return arrayToString(asArray(v), depth);
      }
  }
  return "<unknown>";
}
