#include "jit_runtime.h"

#include <cmath>
#include <cstddef>
#include <cstdio>

#include "../chunk.h"
#include "../memory.h"
#include "../object.h"
#include "../value.h"
#include "../vm.h"

// The template compiler hard-codes these offsets and tag values into machine
// code; fail the build if the layout drifts.
static_assert(sizeof(Value) == 16, "JIT assumes 16-byte values");
static_assert(offsetof(Value, type) == 0, "JIT assumes tag at offset 0");
static_assert(offsetof(Value, as) == 8, "JIT assumes payload at offset 8");
static_assert(static_cast<int>(ValueType::NIL) == 0 &&
                  static_cast<int>(ValueType::BOOL) == 1 &&
                  static_cast<int>(ValueType::NUMBER) == 2 &&
                  static_cast<int>(ValueType::OBJ) == 3,
              "JIT assumes stable ValueType tags");

// All access to VM internals goes through this friend.
struct JitRuntime {
  static void syncIp(VM* vm, int bcOffset) {
    CallFrame& frame = vm->frames_[vm->frameCount_ - 1];
    frame.ip = frame.closure->function->chunk.code.data() + bcOffset + 1;
  }

  static Value pop(VM* vm) { return *--vm->stackTop_; }
  static void push(VM* vm, const Value& v) { *vm->stackTop_++ = v; }
  static const Value& peek(VM* vm, int distance) {
    return vm->stackTop_[-1 - distance];
  }

  static int binary(VM* vm, int op, int bcOffset) {
    syncIp(vm, bcOffset);
    if (op == OP_ADD && peek(vm, 0).isString() && peek(vm, 1).isString()) {
      vm->concatenate();
      return 1;
    }
    if (!peek(vm, 0).isNumber() || !peek(vm, 1).isNumber()) {
      if (op == OP_ADD) {
        vm->runtimeError("Operands must be two numbers or two strings.");
      } else if (op == OP_GREATER || op == OP_LESS) {
        if (peek(vm, 0).isString() && peek(vm, 1).isString()) {
          ObjString* b = asString(pop(vm));
          ObjString* a = asString(pop(vm));
          push(vm, Value::boolean(op == OP_GREATER ? a->chars > b->chars
                                                   : a->chars < b->chars));
          return 1;
        }
        vm->runtimeError("Operands must be two numbers or two strings.");
      } else {
        vm->runtimeError("Operands must be numbers.");
      }
      return 0;
    }
    double b = pop(vm).as.number;
    double a = pop(vm).as.number;
    switch (op) {
      case OP_ADD: push(vm, Value::number(a + b)); break;
      case OP_SUBTRACT: push(vm, Value::number(a - b)); break;
      case OP_MULTIPLY: push(vm, Value::number(a * b)); break;
      case OP_DIVIDE: push(vm, Value::number(a / b)); break;
      case OP_MODULO: push(vm, Value::number(fmod(a, b))); break;
      case OP_GREATER: push(vm, Value::boolean(a > b)); break;
      case OP_LESS: push(vm, Value::boolean(a < b)); break;
      default: return 0;
    }
    return 1;
  }

  static int negate(VM* vm, int bcOffset) {
    if (!peek(vm, 0).isNumber()) {
      syncIp(vm, bcOffset);
      vm->runtimeError("Operand must be a number.");
      return 0;
    }
    push(vm, Value::number(-pop(vm).as.number));
    return 1;
  }

  static void notOp(VM* vm) { push(vm, Value::boolean(isFalsey(pop(vm)))); }

  static void equal(VM* vm) {
    Value b = pop(vm);
    Value a = pop(vm);
    push(vm, Value::boolean(valuesEqual(a, b)));
  }

  static void print(VM* vm) {
    printf("%s\n", valueToString(pop(vm)).c_str());
  }

  static int getGlobal(VM* vm, ObjString* name, int bcOffset) {
    auto it = vm->globals_.find(name);
    if (it == vm->globals_.end()) {
      syncIp(vm, bcOffset);
      vm->runtimeError("Undefined variable '%s'.", name->chars.c_str());
      return 0;
    }
    push(vm, it->second);
    return 1;
  }

  static int setGlobal(VM* vm, ObjString* name, int bcOffset) {
    auto it = vm->globals_.find(name);
    if (it == vm->globals_.end()) {
      syncIp(vm, bcOffset);
      vm->runtimeError("Undefined variable '%s'.", name->chars.c_str());
      return 0;
    }
    it->second = peek(vm, 0);
    return 1;
  }

  static void defineGlobal(VM* vm, ObjString* name) {
    vm->globals_[name] = peek(vm, 0);
    pop(vm);
  }

  static void returnOp(VM* vm) {
    // JIT-compiled functions never contain OP_CLOSURE, so no open upvalues
    // can point into this frame; skipping closeUpvalues is safe.
    Value result = pop(vm);
    CallFrame& frame = vm->frames_[vm->frameCount_ - 1];
    vm->frameCount_--;
    vm->stackTop_ = frame.slots;
    push(vm, result);
  }
};

extern "C" {

int ember_jit_binary(VM* vm, int op, int bcOffset) {
  return JitRuntime::binary(vm, op, bcOffset);
}
int ember_jit_negate(VM* vm, int bcOffset) {
  return JitRuntime::negate(vm, bcOffset);
}
void ember_jit_not(VM* vm) { JitRuntime::notOp(vm); }
void ember_jit_equal(VM* vm) { JitRuntime::equal(vm); }
void ember_jit_print(VM* vm) { JitRuntime::print(vm); }
int ember_jit_get_global(VM* vm, ObjString* name, int bcOffset) {
  return JitRuntime::getGlobal(vm, name, bcOffset);
}
int ember_jit_set_global(VM* vm, ObjString* name, int bcOffset) {
  return JitRuntime::setGlobal(vm, name, bcOffset);
}
void ember_jit_define_global(VM* vm, ObjString* name) {
  JitRuntime::defineGlobal(vm, name);
}
void ember_jit_return(VM* vm) { JitRuntime::returnOp(vm); }

}  // extern "C"
