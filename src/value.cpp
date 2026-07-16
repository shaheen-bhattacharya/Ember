#include "value.h"

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

std::string valueToString(const Value& v) {
  switch (v.type) {
    case ValueType::NIL:
      return "nil";
    case ValueType::BOOL:
      return v.as.boolean ? "true" : "false";
    case ValueType::NUMBER: {
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
      }
  }
  return "<unknown>";
}
