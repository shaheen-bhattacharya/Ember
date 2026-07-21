#pragma once

#include <cstdint>

class VM;
struct ObjFunction;
struct ObjString;
struct Value;

// Static accessors and slow-path implementations with private VM access
// (declared friend by VM). The extern "C" wrappers below are what emitted
// code actually calls.
struct JitRuntime {
  static Value** stackTopAddr(VM* vm);
  static Value* topFrameSlots(VM* vm);
  static void syncIp(VM* vm, int bcOffset);
  static Value pop(VM* vm);
  static void push(VM* vm, const Value& v);
  static const Value& peek(VM* vm, int distance);
  static int binary(VM* vm, int op, int bcOffset);
  static int negate(VM* vm, int bcOffset);
  static void notOp(VM* vm);
  static void equal(VM* vm);
  static void print(VM* vm);
  static int getGlobal(VM* vm, ObjString* name, int bcOffset);
  static int setGlobal(VM* vm, ObjString* name, int bcOffset);
  static void defineGlobal(VM* vm, ObjString* name);
  static void returnOp(VM* vm);
  static int callOp(VM* vm, int argCount, int bcOffset);
  static void closureOp(VM* vm, ObjFunction* target, const uint8_t* pairs);
  static void getUpvalue(VM* vm, int slot);
  static void setUpvalue(VM* vm, int slot);
  static void closeUpvalue(VM* vm);
};

// C-ABI slow-path helpers that template-compiled code calls. Each fallible
// helper returns 1 on success, 0 after raising a runtime error. `bcOffset` is
// the bytecode offset of the executing opcode, used to point the frame's ip
// at the right line for error reporting. All helpers read and write the VM
// value stack directly, so stackTop is always coherent at GC safepoints.
extern "C" {
int ember_jit_binary(VM* vm, int op, int bcOffset);
int ember_jit_negate(VM* vm, int bcOffset);
void ember_jit_not(VM* vm);
void ember_jit_equal(VM* vm);
void ember_jit_print(VM* vm);
int ember_jit_get_global(VM* vm, ObjString* name, int bcOffset);
int ember_jit_set_global(VM* vm, ObjString* name, int bcOffset);
void ember_jit_define_global(VM* vm, ObjString* name);
void ember_jit_return(VM* vm);
int ember_jit_call(VM* vm, int argCount, int bcOffset);
void ember_jit_closure(VM* vm, ObjFunction* target, const uint8_t* pairs);
void ember_jit_get_upvalue(VM* vm, int slot);
void ember_jit_set_upvalue(VM* vm, int slot);
void ember_jit_close_upvalue(VM* vm);
}
