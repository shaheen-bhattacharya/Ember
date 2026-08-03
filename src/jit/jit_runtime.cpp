#include "jit_runtime.h"

#include <cmath>
#include <cstddef>
#include <cstdio>

#include "../chunk.h"
#include "../memory.h"
#include "../object.h"
#include "../value.h"
#include "../vm.h"
#include "jit_compile.h"

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

Value** JitRuntime::stackTopAddr(VM* vm) { return &vm->stackTop_; }
Value* JitRuntime::topFrameSlots(VM* vm) {
  return vm->frames_[vm->frameCount_ - 1].slots;
}

void JitRuntime::syncIp(VM* vm, int bcOffset) {
  CallFrame& frame = vm->frames_[vm->frameCount_ - 1];
  frame.ip = frame.closure->function->chunk.code.data() + bcOffset + 1;
}

Value JitRuntime::pop(VM* vm) { return *--vm->stackTop_; }
void JitRuntime::push(VM* vm, const Value& v) { *vm->stackTop_++ = v; }
const Value& JitRuntime::peek(VM* vm, int distance) {
  return vm->stackTop_[-1 - distance];
}

int JitRuntime::binary(VM* vm, int op, int bcOffset) {
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

int JitRuntime::negate(VM* vm, int bcOffset) {
  if (!peek(vm, 0).isNumber()) {
    syncIp(vm, bcOffset);
    vm->runtimeError("Operand must be a number.");
    return 0;
  }
  push(vm, Value::number(-pop(vm).as.number));
  return 1;
}

void JitRuntime::notOp(VM* vm) {
  push(vm, Value::boolean(isFalsey(pop(vm))));
}

void JitRuntime::equal(VM* vm) {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, Value::boolean(valuesEqual(a, b)));
}

void JitRuntime::print(VM* vm) {
  printf("%s\n", valueToString(pop(vm)).c_str());
}

int JitRuntime::getGlobal(VM* vm, ObjString* name, int bcOffset) {
  auto it = vm->globals_.find(name);
  if (it == vm->globals_.end()) {
    syncIp(vm, bcOffset);
    vm->runtimeError("Undefined variable '%s'.", name->chars.c_str());
    return 0;
  }
  push(vm, it->second);
  return 1;
}

int JitRuntime::setGlobal(VM* vm, ObjString* name, int bcOffset) {
  auto it = vm->globals_.find(name);
  if (it == vm->globals_.end()) {
    syncIp(vm, bcOffset);
    vm->runtimeError("Undefined variable '%s'.", name->chars.c_str());
    return 0;
  }
  it->second = peek(vm, 0);
  return 1;
}

// Inline-cache misses: resolve the global and remember its map slot.
// unordered_map nodes are address-stable across inserts and rehashes, and
// Ember has no way to delete a global, so a filled cache never goes stale.
int JitRuntime::getGlobalIC(VM* vm, ObjString* name, Value** cell,
                            int bcOffset) {
  auto it = vm->globals_.find(name);
  if (it == vm->globals_.end()) {
    syncIp(vm, bcOffset);
    vm->runtimeError("Undefined variable '%s'.", name->chars.c_str());
    return 0;
  }
  *cell = &it->second;
  push(vm, it->second);
  return 1;
}

int JitRuntime::setGlobalIC(VM* vm, ObjString* name, Value** cell,
                            int bcOffset) {
  auto it = vm->globals_.find(name);
  if (it == vm->globals_.end()) {
    syncIp(vm, bcOffset);
    vm->runtimeError("Undefined variable '%s'.", name->chars.c_str());
    return 0;
  }
  *cell = &it->second;
  it->second = peek(vm, 0);
  return 1;
}

void JitRuntime::defineGlobal(VM* vm, ObjString* name) {
  vm->globals_[name] = peek(vm, 0);
  pop(vm);
}

int JitRuntime::callOp(VM* vm, int argCount, int bcOffset) {
  // Keep the caller frame's ip at the call site so stack traces through JIT
  // frames report the right line.
  syncIp(vm, bcOffset);
  const Value& callee = peek(vm, argCount);
  if (callee.isObj()) {
    if (callee.as.obj->type == ObjType::CLOSURE) {
      ObjClosure* closure = asClosure(callee);
      ObjFunction* fn = closure->function;
      if (fn->jitState == JitState::NONE &&
          fn->callCount + 1 >= jit::threshold()) {
        jit::compile(fn);
      }
      if (fn->jitState == JitState::COMPILED) {
        if (!vm->call(closure, argCount)) return 0;
        return jit::execute(vm, fn);  // JIT -> JIT, native recursion
      }
      // JIT -> interpreter: run just this callee, then take back control.
      if (!vm->call(closure, argCount)) return 0;
      return vm->run(vm->frameCount_ - 1) == InterpretResult::OK ? 1 : 0;
    }
    if (callee.as.obj->type == ObjType::NATIVE) {
      NativeFn native = asNative(callee)->function;
      Value result = native(argCount, vm->stackTop_ - argCount);
      vm->stackTop_ -= argCount + 1;
      push(vm, result);
      return 1;
    }
  }
  vm->runtimeError("Can only call functions; got a %s.", typeName(callee));
  return 0;
}

void JitRuntime::returnOp(VM* vm) {
  Value result = pop(vm);
  CallFrame& frame = vm->frames_[vm->frameCount_ - 1];
  vm->closeUpvalues(frame.slots);
  vm->frameCount_--;
  vm->stackTop_ = frame.slots;
  push(vm, result);
}

void JitRuntime::closureOp(VM* vm, ObjFunction* target, const uint8_t* pairs) {
  ObjClosure* closure = gHeap.allocate<ObjClosure>(target);
  // Root the closure before capturing: captureUpvalue allocates.
  push(vm, Value::object(closure));
  CallFrame& frame = vm->frames_[vm->frameCount_ - 1];
  for (int i = 0; i < target->upvalueCount; i++) {
    uint8_t isLocal = pairs[i * 2];
    uint8_t index = pairs[i * 2 + 1];
    if (isLocal) {
      closure->upvalues[i] = vm->captureUpvalue(frame.slots + index);
    } else {
      closure->upvalues[i] = frame.closure->upvalues[index];
    }
  }
}

void JitRuntime::getUpvalue(VM* vm, int slot) {
  CallFrame& frame = vm->frames_[vm->frameCount_ - 1];
  push(vm, *frame.closure->upvalues[slot]->location);
}

void JitRuntime::setUpvalue(VM* vm, int slot) {
  CallFrame& frame = vm->frames_[vm->frameCount_ - 1];
  *frame.closure->upvalues[slot]->location = peek(vm, 0);
}

void JitRuntime::closeUpvalue(VM* vm) {
  vm->closeUpvalues(vm->stackTop_ - 1);
  pop(vm);
}

void JitRuntime::arrayOp(VM* vm, int count) { vm->makeArray(count); }

void JitRuntime::dup2(VM* vm) {
  push(vm, peek(vm, 1));
  push(vm, peek(vm, 1));
}

int JitRuntime::indexGetOp(VM* vm, int bcOffset) {
  syncIp(vm, bcOffset);
  return vm->indexGet() ? 1 : 0;
}

int JitRuntime::indexSetOp(VM* vm, int bcOffset) {
  syncIp(vm, bcOffset);
  return vm->indexSet() ? 1 : 0;
}

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
int ember_jit_get_global_ic(VM* vm, ObjString* name, Value** cell,
                            int bcOffset) {
  return JitRuntime::getGlobalIC(vm, name, cell, bcOffset);
}
int ember_jit_set_global_ic(VM* vm, ObjString* name, Value** cell,
                            int bcOffset) {
  return JitRuntime::setGlobalIC(vm, name, cell, bcOffset);
}
void ember_jit_return(VM* vm) { JitRuntime::returnOp(vm); }
int ember_jit_call(VM* vm, int argCount, int bcOffset) {
  return JitRuntime::callOp(vm, argCount, bcOffset);
}
void ember_jit_closure(VM* vm, ObjFunction* target, const uint8_t* pairs) {
  JitRuntime::closureOp(vm, target, pairs);
}
void ember_jit_get_upvalue(VM* vm, int slot) {
  JitRuntime::getUpvalue(vm, slot);
}
void ember_jit_set_upvalue(VM* vm, int slot) {
  JitRuntime::setUpvalue(vm, slot);
}
void ember_jit_close_upvalue(VM* vm) { JitRuntime::closeUpvalue(vm); }
void ember_jit_array(VM* vm, int count) { JitRuntime::arrayOp(vm, count); }
void ember_jit_dup2(VM* vm) { JitRuntime::dup2(vm); }
int ember_jit_index_get(VM* vm, int bcOffset) {
  return JitRuntime::indexGetOp(vm, bcOffset);
}
int ember_jit_index_set(VM* vm, int bcOffset) {
  return JitRuntime::indexSetOp(vm, bcOffset);
}

}  // extern "C"
