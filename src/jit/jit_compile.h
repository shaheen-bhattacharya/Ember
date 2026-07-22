#pragma once

#include <cstdint>

class VM;
struct ObjFunction;

namespace jit {

// True when the JIT is compiled in for this architecture AND enabled via
// EMBER_JIT=1. Checked on the call path, so it must be cheap.
bool enabled();

// Call count at which a function tiers up (EMBER_JIT_THRESHOLD overrides;
// defaults to the profiler's hot threshold).
uint32_t threshold();

// Attempts to template-compile fn, setting jitState to COMPILED or
// UNSUPPORTED. Idempotent.
void compile(ObjFunction* fn);

// Runs fn's compiled entry against the current top frame. Returns 1 on
// success (frame popped, result pushed), 0 after a runtime error.
int execute(VM* vm, ObjFunction* fn);

// Prints compilation totals to stderr (functions compiled/rejected, bytecode
// vs native bytes). Called at VM teardown when EMBER_LOG_JIT is set.
void printStats();

}  // namespace jit
