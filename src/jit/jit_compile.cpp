#include "jit_compile.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "../object.h"
#include "jit_memory.h"
#include "jit_runtime.h"

#if EMBER_JIT_SUPPORTED

#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../chunk.h"
#include "asm_arm64.h"

namespace {

// Compiled code must outlive the ObjFunctions that point into it (functions
// are GC'd; baseline code is not). Freed at process exit.
std::vector<std::unique_ptr<CodeBuffer>>& codeRegistry() {
  static std::vector<std::unique_ptr<CodeBuffer>> registry;
  return registry;
}

// Inline-cache cells for global accesses, one per compiled site. A deque
// keeps element addresses stable; cells live as long as the code that
// embeds their addresses (process lifetime).
Value** newGlobalCacheCell() {
  static std::deque<Value*> cells;
  cells.push_back(nullptr);
  return &cells.back();
}

struct JitStats {
  int compiled = 0;
  int rejected = 0;
  size_t bytecodeBytes = 0;
  size_t nativeBytes = 0;
};
JitStats gStats;

// Hex dump in a shape `llvm-mc --disassemble` accepts, for eyeballing the
// emitted code: EMBER_JIT_DUMP=1.
void dumpCode(const char* name, const CodeBuffer& buf) {
  fprintf(stderr, "[jit] -- %s (%zu bytes) --\n", name, buf.size());
  const uint8_t* code = buf.base();
  for (size_t i = 0; i < buf.size(); i += 4) {
    fprintf(stderr, "  0x%04zx: 0x%02x 0x%02x 0x%02x 0x%02x\n", i, code[i],
            code[i + 1], code[i + 2], code[i + 3]);
  }
}

using JitEntry = int (*)(VM* vm, Value** stackTopAddr, Value* slots);

// Register conventions inside emitted code:
//   x19 = VM*        x20 = &vm->stackTop_        x21 = frame slots base
// x0-x9 are scratch; d0/d1 hold unboxed doubles in fast paths.
constexpr int kVm = 19;
constexpr int kTopAddr = 20;
constexpr int kSlots = 21;

constexpr int kTagNumber = 2;  // ValueType::NUMBER, pinned by static_assert

class TemplateCompiler {
 public:
  explicit TemplateCompiler(ObjFunction* fn, CodeBuffer& buf)
      : chunk_(fn->chunk), a_(buf) {}

  bool compile() {
    if (!prescan()) return false;  // unsupported opcode somewhere
    emitPrologue();
    const std::vector<uint8_t>& code = chunk_.code;
    for (size_t offset = 0; offset < code.size();) {
      auto target = targets_.find(static_cast<int>(offset));
      if (target != targets_.end()) a_.bind(target->second);
      size_t next = emitOp(static_cast<int>(offset));
      if (next == 0) return false;
      offset = next;
    }
    a_.bind(errorExit_);
    a_.movz(0, 0);
    emitEpilogue();
    return a_.ok();
  }

 private:
  const Chunk& chunk_;
  a64::Assembler a_;
  a64::Label errorExit_;
  std::unordered_map<int, a64::Label> targets_;  // bytecode offset -> label
  std::unordered_set<int> fused_;  // offsets of fusable compare+branch triples

  int read16(int offset) const {
    return (chunk_.code[offset] << 8) | chunk_.code[offset + 1];
  }

  // Instruction length, or -1 for opcodes the template compiler can't emit.
  int supportedLength(int offset) const {
    uint8_t op = chunk_.code[offset];
    switch (op) {
      case OP_CLOSURE: {
        ObjFunction* target =
            asFunction(chunk_.constants[chunk_.code[offset + 1]]);
        return 2 + 2 * target->upvalueCount;
      }
      case OP_CONSTANT:
      case OP_GET_LOCAL:
      case OP_SET_LOCAL:
      case OP_CALL:
      case OP_GET_GLOBAL:
      case OP_SET_GLOBAL:
      case OP_DEFINE_GLOBAL:
      case OP_GET_UPVALUE:
      case OP_SET_UPVALUE:
      case OP_ARRAY:
        return 2;
      case OP_JUMP:
      case OP_JUMP_IF_FALSE:
      case OP_LOOP:
      case OP_CONSTANT_LONG:
        return 3;
      case OP_NIL:
      case OP_TRUE:
      case OP_FALSE:
      case OP_POP:
      case OP_EQUAL:
      case OP_GREATER:
      case OP_LESS:
      case OP_ADD:
      case OP_SUBTRACT:
      case OP_MULTIPLY:
      case OP_DIVIDE:
      case OP_MODULO:
      case OP_NOT:
      case OP_NEGATE:
      case OP_PRINT:
      case OP_RETURN:
      case OP_CLOSE_UPVALUE:
      case OP_INDEX_GET:
      case OP_INDEX_SET:
        return 1;
      default:
        return -1;
    }
  }

  int fusionTarget(int off) const { return off + 4 + read16(off + 2); }

  // A fusable triple: [LESS/GREATER][JUMP_IF_FALSE t][POP] where t is itself
  // a POP (the exit's condition pop). Fused code never materializes the
  // boolean, so no other jump may land inside the triple or on t.
  bool isFusionCandidate(int off) const {
    const std::vector<uint8_t>& code = chunk_.code;
    if (static_cast<size_t>(off) + 4 >= code.size()) return false;
    uint8_t op = code[off];
    if (op != OP_LESS && op != OP_GREATER && op != OP_EQUAL) return false;
    if (code[off + 1] != OP_JUMP_IF_FALSE || code[off + 4] != OP_POP) {
      return false;
    }
    int target = fusionTarget(off);
    return static_cast<size_t>(target) < code.size() &&
           code[target] == OP_POP;
  }

  // Validates every opcode is supported, collects branch targets, and finds
  // compare+branch triples that fuse into one native fcmp+b.cond.
  bool prescan() {
    const std::vector<uint8_t>& code = chunk_.code;
    // Pass 1: validate, gather fusion candidates, and register targets of
    // every jump EXCEPT a candidate's own JUMP_IF_FALSE (a fused triple
    // branches to target+1 instead; unfused ones get registered below).
    for (size_t offset = 0; offset < code.size();) {
      int off = static_cast<int>(offset);
      uint8_t op = code[offset];
      int length = supportedLength(off);
      if (length < 0) return false;
      if (isFusionCandidate(off)) {
        fused_.insert(off);
        offset += 5;
        continue;
      }
      int next = off + length;
      if (op == OP_JUMP || op == OP_JUMP_IF_FALSE) {
        targets_[next + read16(off + 1)];
      } else if (op == OP_LOOP) {
        targets_[next - read16(off + 1)];
      }
      offset = static_cast<size_t>(next);
    }

    // Pass 2: un-fuse any candidate whose interior or target is jumped to.
    // Un-fusing registers its normal target, which can invalidate another
    // candidate, so iterate to a fixpoint.
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto it = fused_.begin(); it != fused_.end();) {
        int off = *it;
        int target = fusionTarget(off);
        if (targets_.count(off + 1) != 0 || targets_.count(off + 4) != 0 ||
            targets_.count(target) != 0) {
          targets_[target];  // now branches like a normal JUMP_IF_FALSE
          it = fused_.erase(it);
          changed = true;
        } else {
          ++it;
        }
      }
    }
    for (int off : fused_) targets_[fusionTarget(off) + 1];
    return true;
  }

  void emitPrologue() {
    a_.stpPreX(29, 30, 31, -16);
    a_.stpPreX(19, 20, 31, -16);
    a_.stpPreX(21, 22, 31, -16);
    a_.movReg(kVm, 0);
    a_.movReg(kTopAddr, 1);
    a_.movReg(kSlots, 2);
  }

  void emitEpilogue() {
    a_.ldpPostX(21, 22, 31, 16);
    a_.ldpPostX(19, 20, 31, 16);
    a_.ldpPostX(29, 30, 31, 16);
    a_.ret();
  }

  // Pushes the 16-byte Value in x2(tag half)/x3(payload half).
  void emitPushPair() {
    a_.ldrX(0, kTopAddr, 0);
    a_.stpX(2, 3, 0, 0);
    a_.addImm(0, 0, 16);
    a_.strX(0, kTopAddr, 0);
  }

  void emitCallHelper(void* fn) {
    a_.movImm64(9, reinterpret_cast<uint64_t>(fn));
    a_.blr(9);
  }

  // helper(vm) — infallible.
  void emitHelper1(void* fn) {
    a_.movReg(0, kVm);
    emitCallHelper(fn);
  }

  // helper(vm, imm) — infallible.
  void emitHelper2(void* fn, uint64_t imm) {
    a_.movReg(0, kVm);
    a_.movImm64(1, imm);
    emitCallHelper(fn);
  }

  // helper(vm, bcOffset) -> status.
  void emitHelper2Checked(void* fn, int bcOffset) {
    a_.movReg(0, kVm);
    a_.movImm64(1, static_cast<uint64_t>(bcOffset));
    emitCallHelper(fn);
    a_.cbzW(0, errorExit_);
  }

  // helper(vm, imm, bcOffset) -> status.
  void emitHelper3Checked(void* fn, uint64_t imm, int bcOffset) {
    a_.movReg(0, kVm);
    a_.movImm64(1, imm);
    a_.movImm64(2, static_cast<uint64_t>(bcOffset));
    emitCallHelper(fn);
    a_.cbzW(0, errorExit_);
  }

  // Loads &slots[slot] into reg (slots are 16 bytes each).
  void emitSlotAddr(int reg, int slot) {
    a_.addImm(reg, kSlots, static_cast<uint32_t>(slot) * 16);
  }

  // Emits guards that both operands under stackTop are numbers, else jumps
  // to `slow`. Leaves stackTop in x0 and the doubles in d0/d1.
  void emitNumberGuards(a64::Label& slow) {
    a_.ldrX(0, kTopAddr, 0);
    a_.ldurbW(1, 0, -32);
    a_.cmpImmW(1, kTagNumber);
    a_.bCond(a64::NE, slow);
    a_.ldurbW(1, 0, -16);
    a_.cmpImmW(1, kTagNumber);
    a_.bCond(a64::NE, slow);
    a_.ldurD(0, 0, -24);
    a_.ldurD(1, 0, -8);
  }

  void emitShrinkOne() {  // stackTop -= 1 value (x0 holds old stackTop)
    a_.subImm(0, 0, 16);
    a_.strX(0, kTopAddr, 0);
  }

  void emitArithmetic(uint8_t op, int bcOffset) {
    a64::Label slow, done;
    emitNumberGuards(slow);
    switch (op) {
      case OP_ADD: a_.faddD(0, 0, 1); break;
      case OP_SUBTRACT: a_.fsubD(0, 0, 1); break;
      case OP_MULTIPLY: a_.fmulD(0, 0, 1); break;
      case OP_DIVIDE: a_.fdivD(0, 0, 1); break;
    }
    a_.sturD(0, 0, -24);  // overwrite a's payload; tag already NUMBER
    emitShrinkOne();
    a_.b(done);
    a_.bind(slow);
    emitHelper3Checked(reinterpret_cast<void*>(ember_jit_binary), op,
                       bcOffset);
    a_.bind(done);
  }

  // [CMP][JUMP_IF_FALSE][POP] as one unit: no boolean is materialized. Both
  // arms land past the target's condition-pop (prescan registered target+1).
  void emitFusedCompareBranch(uint8_t op, int offset) {
    int target = offset + 4 + read16(offset + 2);
    a64::Label& taken = targets_[target + 1];
    a64::Label slow, fall;
    emitNumberGuards(slow);
    a_.fcmpD(0, 1);
    a_.subImm(0, 0, 32);  // pop both operands
    a_.strX(0, kTopAddr, 0);
    // Branch when the comparison is false; the inverted conditions also
    // cover unordered (NaN compares false, and NaN == NaN is false),
    // matching interpreter truthiness.
    a64::Cond whenFalse = op == OP_LESS      ? a64::PL
                          : op == OP_GREATER ? a64::LE
                                             : a64::NE;
    a_.bCond(whenFalse, taken);
    a_.b(fall);
    a_.bind(slow);
    if (op == OP_EQUAL) {
      // Non-number equality is infallible; the helper pushes the boolean.
      emitHelper1(reinterpret_cast<void*>(ember_jit_equal));
    } else {
      emitHelper3Checked(reinterpret_cast<void*>(ember_jit_binary), op,
                         offset);
    }
    // The helper pushed the boolean; pop it and branch on the payload.
    a_.ldrX(0, kTopAddr, 0);
    a_.ldurbW(2, 0, -8);
    emitShrinkOne();
    a_.cbzW(2, taken);
    a_.bind(fall);
  }

  void emitComparison(uint8_t op, int bcOffset) {
    a64::Label slow, done;
    emitNumberGuards(slow);
    a_.fcmpD(0, 1);
    // a < b: MI (unordered clears N); a > b: GT (unordered breaks N==V).
    a_.csetX(2, op == OP_LESS ? a64::MI : a64::GT);
    a_.movz(3, 1);        // BOOL tag
    a_.sturX(3, 0, -32);
    a_.sturX(2, 0, -24);
    emitShrinkOne();
    a_.b(done);
    a_.bind(slow);
    emitHelper3Checked(reinterpret_cast<void*>(ember_jit_binary), op,
                       bcOffset);
    a_.bind(done);
  }

  // not: replace top of stack with a boolean, no helper (infallible).
  void emitNot() {
    a64::Label isTrue, isFalse, store;
    a_.ldrX(0, kTopAddr, 0);
    a_.ldurbW(1, 0, -16);
    a_.cbzW(1, isTrue);   // nil -> !nil is true
    a_.cmpImmW(1, 1);     // BOOL?
    a_.bCond(a64::NE, isFalse);  // any other type is truthy -> false
    a_.ldurbW(3, 0, -8);
    a_.cbzW(3, isTrue);   // boolean false -> true
    a_.bind(isFalse);
    a_.movz(3, 0);
    a_.b(store);
    a_.bind(isTrue);
    a_.movz(3, 1);
    a_.bind(store);
    a_.movz(2, 1);        // BOOL tag
    a_.sturX(2, 0, -16);
    a_.sturX(3, 0, -8);
  }

  // negate: in-place fneg on the number fast path.
  void emitNegate(int bcOffset) {
    a64::Label slow, done;
    a_.ldrX(0, kTopAddr, 0);
    a_.ldurbW(1, 0, -16);
    a_.cmpImmW(1, kTagNumber);
    a_.bCond(a64::NE, slow);
    a_.ldurD(0, 0, -8);
    a_.fnegD(0, 0);
    a_.sturD(0, 0, -8);
    a_.b(done);
    a_.bind(slow);
    emitHelper2Checked(reinterpret_cast<void*>(ember_jit_negate), bcOffset);
    a_.bind(done);
  }

  // Returns the offset of the next opcode, or 0 if this one is unsupported.
  size_t emitOp(int offset) {
    const std::vector<uint8_t>& code = chunk_.code;
    uint8_t op = code[offset];
    switch (op) {
      case OP_CONSTANT: {
        const Value* constant = &chunk_.constants[code[offset + 1]];
        a_.movImm64(1, reinterpret_cast<uint64_t>(constant));
        a_.ldpX(2, 3, 1, 0);
        emitPushPair();
        return offset + 2;
      }
      case OP_CONSTANT_LONG: {
        const Value* constant = &chunk_.constants[read16(offset + 1)];
        a_.movImm64(1, reinterpret_cast<uint64_t>(constant));
        a_.ldpX(2, 3, 1, 0);
        emitPushPair();
        return offset + 3;
      }
      case OP_NIL:
        a_.movz(2, 0);
        a_.movz(3, 0);
        emitPushPair();
        return offset + 1;
      case OP_TRUE:
      case OP_FALSE:
        a_.movz(2, 1);  // BOOL tag
        a_.movz(3, op == OP_TRUE ? 1 : 0);
        emitPushPair();
        return offset + 1;
      case OP_POP:
        a_.ldrX(0, kTopAddr, 0);
        emitShrinkOne();
        return offset + 1;
      case OP_GET_LOCAL: {
        emitSlotAddr(1, code[offset + 1]);
        a_.ldpX(2, 3, 1, 0);
        emitPushPair();
        return offset + 2;
      }
      case OP_SET_LOCAL: {
        a_.ldrX(0, kTopAddr, 0);
        a_.ldpX(2, 3, 0, -16);
        emitSlotAddr(1, code[offset + 1]);
        a_.stpX(2, 3, 1, 0);
        return offset + 2;
      }
      case OP_ADD:
      case OP_SUBTRACT:
      case OP_MULTIPLY:
      case OP_DIVIDE:
        emitArithmetic(op, offset);
        return offset + 1;
      case OP_MODULO: {
        // Number fast path calls fmod directly (d0/d1 are already the
        // argument registers); the full helper stays as the slow path.
        a64::Label slow, done;
        emitNumberGuards(slow);
        a_.movImm64(9, reinterpret_cast<uint64_t>(
                           static_cast<double (*)(double, double)>(fmod)));
        a_.blr(9);
        a_.ldrX(0, kTopAddr, 0);  // reload: fmod clobbers scratch registers
        a_.sturD(0, 0, -24);
        emitShrinkOne();
        a_.b(done);
        a_.bind(slow);
        emitHelper3Checked(reinterpret_cast<void*>(ember_jit_binary), op,
                           offset);
        a_.bind(done);
        return offset + 1;
      }
      case OP_GREATER:
      case OP_LESS:
      case OP_EQUAL:
        if (fused_.count(offset) != 0) {
          emitFusedCompareBranch(op, offset);
          return offset + 5;
        }
        if (op == OP_EQUAL) {
          emitHelper1(reinterpret_cast<void*>(ember_jit_equal));
        } else {
          emitComparison(op, offset);
        }
        return offset + 1;
      case OP_NOT:
        emitNot();
        return offset + 1;
      case OP_NEGATE:
        emitNegate(offset);
        return offset + 1;
      case OP_PRINT:
        emitHelper1(reinterpret_cast<void*>(ember_jit_print));
        return offset + 1;
      case OP_CALL:
        emitHelper3Checked(reinterpret_cast<void*>(ember_jit_call),
                           code[offset + 1], offset);
        return offset + 2;
      case OP_GET_GLOBAL:
      case OP_SET_GLOBAL:
      case OP_DEFINE_GLOBAL: {
        // The name is an interned ObjString in the constant pool; interned
        // strings referenced by a live function's constants can't be swept,
        // so embedding the pointer is safe.
        ObjString* name = asString(chunk_.constants[code[offset + 1]]);
        if (op == OP_DEFINE_GLOBAL) {
          a_.movReg(0, kVm);
          a_.movImm64(1, reinterpret_cast<uint64_t>(name));
          emitCallHelper(reinterpret_cast<void*>(ember_jit_define_global));
          return offset + 2;
        }
        // Inline cache: after the first resolution the site reads or writes
        // the map slot directly, skipping the hash lookup.
        Value** cell = newGlobalCacheCell();
        a64::Label slow, done;
        a_.movImm64(1, reinterpret_cast<uint64_t>(cell));
        a_.ldrX(4, 1, 0);
        a_.cbzX(4, slow);
        if (op == OP_GET_GLOBAL) {
          a_.ldpX(2, 3, 4, 0);
          emitPushPair();
        } else {
          a_.ldrX(0, kTopAddr, 0);
          a_.ldpX(2, 3, 0, -16);  // assignment leaves the value on the stack
          a_.stpX(2, 3, 4, 0);
        }
        a_.b(done);
        a_.bind(slow);
        a_.movReg(0, kVm);
        a_.movImm64(1, reinterpret_cast<uint64_t>(name));
        a_.movImm64(2, reinterpret_cast<uint64_t>(cell));
        a_.movImm64(3, static_cast<uint64_t>(offset));
        emitCallHelper(op == OP_GET_GLOBAL
                           ? reinterpret_cast<void*>(ember_jit_get_global_ic)
                           : reinterpret_cast<void*>(ember_jit_set_global_ic));
        a_.cbzW(0, errorExit_);
        a_.bind(done);
        return offset + 2;
      }
      case OP_CLOSURE: {
        ObjFunction* target = asFunction(chunk_.constants[code[offset + 1]]);
        // The (isLocal, index) pairs live in the caller's bytecode, which is
        // stable for the function's lifetime; pass a direct pointer.
        a_.movReg(0, kVm);
        a_.movImm64(1, reinterpret_cast<uint64_t>(target));
        a_.movImm64(2, reinterpret_cast<uint64_t>(code.data() + offset + 2));
        emitCallHelper(reinterpret_cast<void*>(ember_jit_closure));
        return offset + 2 + 2 * target->upvalueCount;
      }
      case OP_GET_UPVALUE:
        emitHelper2(reinterpret_cast<void*>(ember_jit_get_upvalue),
                    code[offset + 1]);
        return offset + 2;
      case OP_SET_UPVALUE:
        emitHelper2(reinterpret_cast<void*>(ember_jit_set_upvalue),
                    code[offset + 1]);
        return offset + 2;
      case OP_CLOSE_UPVALUE:
        emitHelper1(reinterpret_cast<void*>(ember_jit_close_upvalue));
        return offset + 1;
      case OP_ARRAY:
        emitHelper2(reinterpret_cast<void*>(ember_jit_array),
                    code[offset + 1]);
        return offset + 2;
      case OP_INDEX_GET:
        emitHelper2Checked(reinterpret_cast<void*>(ember_jit_index_get),
                           offset);
        return offset + 1;
      case OP_INDEX_SET:
        emitHelper2Checked(reinterpret_cast<void*>(ember_jit_index_set),
                           offset);
        return offset + 1;
      case OP_JUMP: {
        a_.b(targets_[offset + 3 + read16(offset + 1)]);
        return offset + 3;
      }
      case OP_LOOP: {
        a_.b(targets_[offset + 3 - read16(offset + 1)]);
        return offset + 3;
      }
      case OP_JUMP_IF_FALSE: {
        // Falsey = nil, or boolean false. Condition stays on the stack.
        a64::Label& target = targets_[offset + 3 + read16(offset + 1)];
        a64::Label truthy;
        a_.ldrX(0, kTopAddr, 0);
        a_.ldurbW(1, 0, -16);       // type tag of top of stack
        a_.cbzW(1, target);         // NIL -> falsey
        a_.cmpImmW(1, 1);           // BOOL?
        a_.bCond(a64::NE, truthy);  // any other type is truthy
        a_.ldurbW(2, 0, -8);        // boolean payload
        a_.cbzW(2, target);
        a_.bind(truthy);
        return offset + 3;
      }
      case OP_RETURN:
        emitHelper1(reinterpret_cast<void*>(ember_jit_return));
        a_.movz(0, 1);
        emitEpilogue();
        return offset + 1;
      default:
        return 0;  // unsupported: control flow, calls, globals (later PRs)
    }
  }
};

}  // namespace

namespace jit {

bool enabled() {
  // Default on where supported; EMBER_JIT=0 opts out.
  static const bool on = [] {
    const char* env = getenv("EMBER_JIT");
    return env == nullptr || env[0] != '0';
  }();
  return on;
}

uint32_t threshold() {
  static const uint32_t value = [] {
    const char* env = getenv("EMBER_JIT_THRESHOLD");
    if (env != nullptr) {
      long parsed = strtol(env, nullptr, 10);
      if (parsed >= 1) return static_cast<uint32_t>(parsed);
    }
    return kHotCallThreshold;
  }();
  return value;
}

void compile(ObjFunction* fn) {
  if (fn->jitState != JitState::NONE) return;
  bool logJit = getenv("EMBER_LOG_JIT") != nullptr;

  // Generous bound: the largest template is well under 64 bytes/op.
  size_t capacity = fn->chunk.code.size() * 96 + 256;
  auto buf = std::make_unique<CodeBuffer>(capacity);
  if (!buf->valid()) {
    fn->jitState = JitState::UNSUPPORTED;
    return;
  }

  TemplateCompiler compiler(fn, *buf);
  const char* name = fn->name ? fn->name->chars.c_str() : "<script>";
  if (!compiler.compile()) {
    fn->jitState = JitState::UNSUPPORTED;
    gStats.rejected++;
    if (logJit) {
      fprintf(stderr, "[jit] %s not compiled (unsupported bytecode)\n", name);
    }
    return;
  }
  buf->finalize();
  fn->jitEntry = reinterpret_cast<void*>(buf->entry<JitEntry>());
  fn->jitState = JitState::COMPILED;
  gStats.compiled++;
  gStats.bytecodeBytes += fn->chunk.code.size();
  gStats.nativeBytes += buf->size();
  if (logJit) {
    fprintf(stderr, "[jit] compiled %s (%zu bytecode -> %zu native bytes)\n",
            name, fn->chunk.code.size(), buf->size());
  }
  if (getenv("EMBER_JIT_DUMP") != nullptr) dumpCode(name, *buf);
  codeRegistry().push_back(std::move(buf));
}

void printStats() {
  if (gStats.compiled == 0 && gStats.rejected == 0) return;
  fprintf(stderr,
          "[jit] stats: %d compiled, %d rejected, %zu bytecode bytes -> "
          "%zu native bytes (%.1fx expansion)\n",
          gStats.compiled, gStats.rejected, gStats.bytecodeBytes,
          gStats.nativeBytes,
          gStats.bytecodeBytes
              ? static_cast<double>(gStats.nativeBytes) / gStats.bytecodeBytes
              : 0.0);
}

int execute(VM* vm, ObjFunction* fn) {
  JitEntry entry = reinterpret_cast<JitEntry>(fn->jitEntry);
  return entry(vm, JitRuntime::stackTopAddr(vm), JitRuntime::topFrameSlots(vm));
}

}  // namespace jit

#else  // !EMBER_JIT_SUPPORTED

namespace jit {
bool enabled() { return false; }
uint32_t threshold() { return kHotCallThreshold; }
void compile(ObjFunction* fn) { fn->jitState = JitState::UNSUPPORTED; }
int execute(VM*, ObjFunction*) { return 0; }
void printStats() {}
}  // namespace jit

#endif
