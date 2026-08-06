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
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.fcmpD(0, 1);
    ok &= check(firstWord(buf) == 0x1E612000u, "fcmp d0, d1");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.csetX(2, a64::MI);
    ok &= check(firstWord(buf) == 0x9A9F57E2u, "cset x2, mi");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.ldurbW(1, 0, -16);
    ok &= check(firstWord(buf) == 0x385F0001u, "ldurb w1, [x0, #-16]");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.blr(9);
    ok &= check(firstWord(buf) == 0xD63F0120u, "blr x9");
  }
  {
    CodeBuffer buf(64);
    a64::Assembler a(buf);
    a.ldpX(2, 3, 1, 0);
    ok &= check(firstWord(buf) == 0xA9400C22u, "ldp x2, x3, [x1]");
  }
  return ok;
}

// fcmp/cset materializing booleans in a loop: counts values below a
// threshold, exercising the comparison encodings end to end.
bool testCompareLoop() {
  CodeBuffer buf(4096);
  a64::Assembler a(buf);
  // long countBelow(double threshold in d0): counts how many of d1..d1+9
  // (starting at 0, step 1) are < threshold.
  a64::Label loop, done;
  a.movz(0, 0);        // count
  a.movz(1, 0);        // i
  a.movImm64(2, 10);   // limit
  a.movz(3, 0);
  a.bind(loop);
  // d1 = (double)x1 via scratch: use fmov? Not encoded; instead track the
  // running value in d1 by adding 1.0 each iteration, starting from 0.0.
  a.fcmpD(1, 0);       // d1 < threshold?
  a.csetX(4, a64::MI);
  a.addReg(0, 0, 4);
  // d1 += 1.0: 1.0 preloaded in d2 by caller convention below.
  a.faddD(1, 1, 2);
  a.addImm(1, 1, 1);
  a.subImm(2, 2, 1);
  a.cbzX(2, done);
  a.b(loop);
  a.bind(done);
  a.ret();
  buf.finalize();
  // Signature trick: d0 = threshold, d1 = 0.0, d2 = 1.0 as double args.
  using Fn = long (*)(double, double, double);
  long result = buf.entry<Fn>()(4.5, 0.0, 1.0);
  return check(a.ok() && result == 5, "compare loop counts 5 below 4.5");
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
            testForwardBranch() && testCondBranchLoop() && testCompareLoop();
  if (ok) {
    printf("jit selftest: ok (encodings, execution, branches, compares)\n");
  }
  return ok;
}

#else

bool jitSelftest() {
  printf("jit selftest: skipped (unsupported architecture)\n");
  return true;
}

#endif
