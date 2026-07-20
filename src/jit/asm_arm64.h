#pragma once

#include "jit_memory.h"

#if EMBER_JIT_SUPPORTED

#include <cstdint>
#include <vector>

// Minimal AArch64 encoder: exactly the instructions the baseline template
// compiler needs, nothing more. Registers are plain integers (0-30, 31 = sp
// or zr depending on instruction). FP registers are d0-d31.
namespace a64 {

enum Cond : uint32_t {
  EQ = 0, NE = 1, CS = 2, CC = 3, MI = 4, PL = 5, VS = 6, VC = 7,
  HI = 8, LS = 9, GE = 10, LT = 11, GT = 12, LE = 13, AL = 14,
};

// A branch target: bind() fixes its position; branches to an unbound label
// are recorded and patched when it binds.
struct Label {
  ptrdiff_t pos = -1;                 // byte offset in the buffer, -1 = unbound
  std::vector<size_t> uses;           // byte offsets of branch instructions
};

class Assembler {
 public:
  explicit Assembler(CodeBuffer& buf) : buf_(buf) {}

  size_t offset() const { return buf_.size(); }
  bool ok() const { return buf_.valid() && unbound_ == 0; }

  // --- moves and immediates ---
  void movz(int rd, uint16_t imm, int shift = 0);
  void movk(int rd, uint16_t imm, int shift);
  void movImm64(int rd, uint64_t value);   // movz + up to 3 movk
  void movReg(int rd, int rm);             // orr rd, xzr, rm

  // --- integer arithmetic / compare ---
  void addReg(int rd, int rn, int rm);
  void addImm(int rd, int rn, uint32_t imm12);
  void subImm(int rd, int rn, uint32_t imm12);
  void cmpImmW(int rn, uint32_t imm12);    // subs wzr, wn, #imm
  void csetX(int rd, Cond cond);

  // --- loads/stores (64-bit X regs) ---
  void ldrX(int rt, int rn, uint32_t byteOffset);    // scaled unsigned
  void strX(int rt, int rn, uint32_t byteOffset);
  void ldurX(int rt, int rn, int32_t simm9);         // unscaled signed
  void sturX(int rt, int rn, int32_t simm9);
  void ldrbW(int rt, int rn, uint32_t byteOffset);
  void ldurbW(int rt, int rn, int32_t simm9);
  void ldpX(int rt, int rt2, int rn, int32_t byteOffset);   // scaled imm7
  void stpX(int rt, int rt2, int rn, int32_t byteOffset);
  void stpPreX(int rt, int rt2, int rn, int32_t byteOffset);  // [rn, #imm]!
  void ldpPostX(int rt, int rt2, int rn, int32_t byteOffset); // [rn], #imm

  // --- floating point (double) ---
  void ldurD(int dt, int rn, int32_t simm9);
  void sturD(int dt, int rn, int32_t simm9);
  void faddD(int dd, int dn, int dm);
  void fsubD(int dd, int dn, int dm);
  void fmulD(int dd, int dn, int dm);
  void fdivD(int dd, int dn, int dm);
  void fnegD(int dd, int dn);
  void fcmpD(int dn, int dm);

  // --- control flow ---
  void b(Label& label);
  void bCond(Cond cond, Label& label);
  void cbzX(int rt, Label& label);
  void cbzW(int rt, Label& label);
  void bind(Label& label);
  void blr(int rn);
  void ret();

 private:
  CodeBuffer& buf_;
  int unbound_ = 0;

  void emit(uint32_t word) { buf_.emit32(word); }
  void branchTo(Label& label, uint32_t opcode);
  void patchBranch(size_t at, ptrdiff_t target);
};

}  // namespace a64

#endif  // EMBER_JIT_SUPPORTED
