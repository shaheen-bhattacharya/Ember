#pragma once

#include <cstdint>

class VM;
struct ObjString;

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
}
