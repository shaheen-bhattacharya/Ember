#include "jit.h"

#include <cstdio>

#include "jit_memory.h"

#if EMBER_JIT_SUPPORTED

bool jitSelftest() {
  // Emit `movz x0, #42; ret`, execute it, expect 42 back.
  CodeBuffer buf(4096);
  if (!buf.valid()) {
    fprintf(stderr, "jit selftest: FAILED to map executable memory\n");
    return false;
  }
  buf.emit32(0xD2800540);  // movz x0, #42
  buf.emit32(0xD65F03C0);  // ret
  buf.finalize();

  using Fn = long (*)();
  long result = buf.entry<Fn>()();
  if (result != 42) {
    fprintf(stderr, "jit selftest: FAILED (returned %ld, expected 42)\n",
            result);
    return false;
  }
  printf("jit selftest: ok (aarch64 code emission + execution)\n");
  return true;
}

#else

bool jitSelftest() {
  printf("jit selftest: skipped (unsupported architecture)\n");
  return true;
}

#endif
