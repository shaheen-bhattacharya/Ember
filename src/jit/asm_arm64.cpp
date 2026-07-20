#include "asm_arm64.h"

#if EMBER_JIT_SUPPORTED

#include <cstring>

namespace a64 {

void Assembler::addReg(int rd, int rn, int rm) {
  emit(0x8B000000u | (uint32_t(rm) << 16) | (uint32_t(rn) << 5) | rd);
}

void Assembler::movz(int rd, uint16_t imm, int shift) {
  emit(0xD2800000u | (uint32_t(shift / 16) << 21) | (uint32_t(imm) << 5) | rd);
}

void Assembler::movk(int rd, uint16_t imm, int shift) {
  emit(0xF2800000u | (uint32_t(shift / 16) << 21) | (uint32_t(imm) << 5) | rd);
}

void Assembler::movImm64(int rd, uint64_t value) {
  movz(rd, value & 0xffff, 0);
  if ((value >> 16) & 0xffff) movk(rd, (value >> 16) & 0xffff, 16);
  if ((value >> 32) & 0xffff) movk(rd, (value >> 32) & 0xffff, 32);
  if ((value >> 48) & 0xffff) movk(rd, (value >> 48) & 0xffff, 48);
}

void Assembler::movReg(int rd, int rm) {
  emit(0xAA0003E0u | (uint32_t(rm) << 16) | rd);
}

void Assembler::addImm(int rd, int rn, uint32_t imm12) {
  emit(0x91000000u | ((imm12 & 0xfff) << 10) | (uint32_t(rn) << 5) | rd);
}

void Assembler::subImm(int rd, int rn, uint32_t imm12) {
  emit(0xD1000000u | ((imm12 & 0xfff) << 10) | (uint32_t(rn) << 5) | rd);
}

void Assembler::cmpImmW(int rn, uint32_t imm12) {
  emit(0x7100001Fu | ((imm12 & 0xfff) << 10) | (uint32_t(rn) << 5));
}

void Assembler::csetX(int rd, Cond cond) {
  // csinc rd, xzr, xzr, invert(cond)
  emit(0x9A9F07E0u | ((uint32_t(cond) ^ 1) << 12) | rd);
}

void Assembler::ldrX(int rt, int rn, uint32_t byteOffset) {
  emit(0xF9400000u | ((byteOffset / 8) << 10) | (uint32_t(rn) << 5) | rt);
}

void Assembler::strX(int rt, int rn, uint32_t byteOffset) {
  emit(0xF9000000u | ((byteOffset / 8) << 10) | (uint32_t(rn) << 5) | rt);
}

void Assembler::ldurX(int rt, int rn, int32_t simm9) {
  emit(0xF8400000u | ((uint32_t(simm9) & 0x1ff) << 12) | (uint32_t(rn) << 5) |
       rt);
}

void Assembler::sturX(int rt, int rn, int32_t simm9) {
  emit(0xF8000000u | ((uint32_t(simm9) & 0x1ff) << 12) | (uint32_t(rn) << 5) |
       rt);
}

void Assembler::ldrbW(int rt, int rn, uint32_t byteOffset) {
  emit(0x39400000u | (byteOffset << 10) | (uint32_t(rn) << 5) | rt);
}

void Assembler::ldurbW(int rt, int rn, int32_t simm9) {
  emit(0x38400000u | ((uint32_t(simm9) & 0x1ff) << 12) | (uint32_t(rn) << 5) |
       rt);
}

void Assembler::ldpX(int rt, int rt2, int rn, int32_t byteOffset) {
  uint32_t imm7 = uint32_t(byteOffset / 8) & 0x7f;
  emit(0xA9400000u | (imm7 << 15) | (uint32_t(rt2) << 10) |
       (uint32_t(rn) << 5) | rt);
}

void Assembler::stpX(int rt, int rt2, int rn, int32_t byteOffset) {
  uint32_t imm7 = uint32_t(byteOffset / 8) & 0x7f;
  emit(0xA9000000u | (imm7 << 15) | (uint32_t(rt2) << 10) |
       (uint32_t(rn) << 5) | rt);
}

void Assembler::stpPreX(int rt, int rt2, int rn, int32_t byteOffset) {
  uint32_t imm7 = uint32_t(byteOffset / 8) & 0x7f;
  emit(0xA9800000u | (imm7 << 15) | (uint32_t(rt2) << 10) |
       (uint32_t(rn) << 5) | rt);
}

void Assembler::ldpPostX(int rt, int rt2, int rn, int32_t byteOffset) {
  uint32_t imm7 = uint32_t(byteOffset / 8) & 0x7f;
  emit(0xA8C00000u | (imm7 << 15) | (uint32_t(rt2) << 10) |
       (uint32_t(rn) << 5) | rt);
}

void Assembler::ldurD(int dt, int rn, int32_t simm9) {
  emit(0xFC400000u | ((uint32_t(simm9) & 0x1ff) << 12) | (uint32_t(rn) << 5) |
       dt);
}

void Assembler::sturD(int dt, int rn, int32_t simm9) {
  emit(0xFC000000u | ((uint32_t(simm9) & 0x1ff) << 12) | (uint32_t(rn) << 5) |
       dt);
}

void Assembler::faddD(int dd, int dn, int dm) {
  emit(0x1E602800u | (uint32_t(dm) << 16) | (uint32_t(dn) << 5) | dd);
}

void Assembler::fsubD(int dd, int dn, int dm) {
  emit(0x1E603800u | (uint32_t(dm) << 16) | (uint32_t(dn) << 5) | dd);
}

void Assembler::fmulD(int dd, int dn, int dm) {
  emit(0x1E600800u | (uint32_t(dm) << 16) | (uint32_t(dn) << 5) | dd);
}

void Assembler::fdivD(int dd, int dn, int dm) {
  emit(0x1E601800u | (uint32_t(dm) << 16) | (uint32_t(dn) << 5) | dd);
}

void Assembler::fnegD(int dd, int dn) {
  emit(0x1E614000u | (uint32_t(dn) << 5) | dd);
}

void Assembler::fcmpD(int dn, int dm) {
  emit(0x1E602000u | (uint32_t(dm) << 16) | (uint32_t(dn) << 5));
}

void Assembler::blr(int rn) { emit(0xD63F0000u | (uint32_t(rn) << 5)); }

void Assembler::ret() { emit(0xD65F03C0u); }

void Assembler::branchTo(Label& label, uint32_t opcode) {
  size_t at = offset();
  if (label.pos >= 0) {
    emit(opcode);  // placeholder emit so patch target exists
    patchBranch(at, label.pos);
  } else {
    if (label.uses.empty()) unbound_++;
    label.uses.push_back(at);
    emit(opcode);
  }
}

void Assembler::b(Label& label) { branchTo(label, 0x14000000u); }

void Assembler::bCond(Cond cond, Label& label) {
  branchTo(label, 0x54000000u | cond);
}

void Assembler::cbzX(int rt, Label& label) {
  branchTo(label, 0xB4000000u | rt);
}

void Assembler::cbzW(int rt, Label& label) {
  branchTo(label, 0x34000000u | rt);
}

void Assembler::bind(Label& label) {
  label.pos = static_cast<ptrdiff_t>(offset());
  if (!label.uses.empty()) unbound_--;
  for (size_t use : label.uses) patchBranch(use, label.pos);
  label.uses.clear();
}

void Assembler::patchBranch(size_t at, ptrdiff_t target) {
  int64_t delta = (target - static_cast<ptrdiff_t>(at)) / 4;
  const uint8_t* code = buf_.base();
  uint32_t insn;
  memcpy(&insn, code + at, sizeof(insn));
  if ((insn & 0xFC000000u) == 0x14000000u) {
    insn = 0x14000000u | (uint32_t(delta) & 0x03FFFFFFu);  // b, imm26
  } else {
    // b.cond / cbz / cbnz: imm19 at bits [23:5]
    insn = (insn & ~0x00FFFFE0u) | ((uint32_t(delta) & 0x7FFFFu) << 5);
  }
  buf_.patch32(at, insn);
}

}  // namespace a64

#endif  // EMBER_JIT_SUPPORTED
