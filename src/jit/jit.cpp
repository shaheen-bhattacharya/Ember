#include "jit.h"

#include <cstdio>

#include "jit_memory.h"

#if EMBER_JIT_SUPPORTED

#include <cstring>

#include "asm_arm64.h"

namespace {

bool check(bool cond, const char* what) {
  if (!cond) fprintf(stderr, "jit selftest: FAILED %s\n", what);
  return cond;
}

uint32_t firstWord(const CodeBuffer& buf) {
  uint32_t w;
  memcpy(&w, buf.base(), sizeof(w));
  return w;
}

// Golden encodings, cross-checked against an independent assembler.
bool testEncodings() {
  bool ok = true;
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.movz(0, 42);
    ok &= check(firstWord(buf) == 0xD2800540u, "movz x0, #42");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.faddD(0, 0, 1);
    ok &= check(firstWord(buf) == 0x1E612800u, "fadd d0, d0, d1");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.ldrX(2, 20, 16);
    ok &= check(firstWord(buf) == 0xF9400A82u, "ldr x2, [x20, #16]");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.stpPreX(29, 30, 31, -16);
    ok &= check(firstWord(buf) == 0xA9BF7BFDu, "stp x29, x30, [sp, #-16]!");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.ret();
    ok &= check(firstWord(buf) == 0xD65F03C0u, "ret");
  }
  return ok;
}

bool testIntAdd() {
  CodeBuffer buf(4096);
  a64::Assembler a(buf);
  a.addReg(0, 0, 1);  // x0 = x0 + x1
  a.ret();
  buf.finalize();
  using Fn = long (*)(long, long);
  return check(buf.entry<Fn>()(40, 2) == 42, "int add(40, 2) == 42");
}

bool testDoubleAdd() {
  CodeBuffer buf(4096);
  a64::Assembler a(buf);
  a.faddD(0, 0, 1);  // d0 = d0 + d1
  a.ret();
  buf.finalize();
  using Fn = double (*)(double, double);
  return check(buf.entry<Fn>()(2.5, 39.5) == 42.0, "fadd(2.5, 39.5) == 42");
}

bool testForwardBranch() {
  CodeBuffer buf(4096);
  a64::Assembler a(buf);
  a64::Label skip;
  a.movz(0, 42);
  a.b(skip);
  a.movz(0, 7);  // must be skipped
  a.bind(skip);
  a.ret();
  buf.finalize();
  using Fn = long (*)();
  return check(a.ok() && buf.entry<Fn>()() == 42, "forward branch skips");
}

bool testCondBranchLoop() {
  // Counts down from 5: x0 = 5; loop: sub x0,x0,#1; cbz exits; b loop.
  CodeBuffer buf(4096);
  a64::Assembler a(buf);
  a64::Label loop, done;
  a.movz(0, 5);
  a.movz(1, 0);
  a.bind(loop);
  a.addImm(1, 1, 2);  // x1 += 2 each iteration
  a.subImm(0, 0, 1);
  a.cbzX(0, done);
  a.b(loop);
  a.bind(done);
  a.movReg(0, 1);
  a.ret();
  buf.finalize();
  using Fn = long (*)();
  return check(a.ok() && buf.entry<Fn>()() == 10, "backward branch loop");
}

}  // namespace

bool jitSelftest() {
  bool ok = testEncodings() && testIntAdd() && testDoubleAdd() &&
            testForwardBranch() && testCondBranchLoop();
  if (ok) printf("jit selftest: ok (encodings, execution, branches)\n");
  return ok;
}

#else

bool jitSelftest() {
  printf("jit selftest: skipped (unsupported architecture)\n");
  return true;
}

#endif
