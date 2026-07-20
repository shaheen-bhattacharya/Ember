#include "jit_compile.h"

#include <cstdio>
#include <cstdlib>

#include "../object.h"
#include "jit_memory.h"
#include "jit_runtime.h"

#if EMBER_JIT_SUPPORTED

#include <memory>
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
    emitPrologue();
    const std::vector<uint8_t>& code = chunk_.code;
    for (size_t offset = 0; offset < code.size();) {
      size_t next = emitOp(static_cast<int>(offset));
      if (next == 0) return false;  // unsupported opcode
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
      case OP_MODULO:
        emitHelper3Checked(reinterpret_cast<void*>(ember_jit_binary), op,
                           offset);
        return offset + 1;
      case OP_GREATER:
      case OP_LESS:
        emitComparison(op, offset);
        return offset + 1;
      case OP_EQUAL:
        emitHelper1(reinterpret_cast<void*>(ember_jit_equal));
        return offset + 1;
      case OP_NOT:
        emitHelper1(reinterpret_cast<void*>(ember_jit_not));
        return offset + 1;
      case OP_NEGATE:
        emitHelper2Checked(reinterpret_cast<void*>(ember_jit_negate), offset);
        return offset + 1;
      case OP_PRINT:
        emitHelper1(reinterpret_cast<void*>(ember_jit_print));
        return offset + 1;
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
  static const bool on = [] {
    const char* env = getenv("EMBER_JIT");
    return env != nullptr && env[0] == '1';
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
  if (!compiler.compile()) {
    fn->jitState = JitState::UNSUPPORTED;
    if (logJit) {
      fprintf(stderr, "[jit] %s not compiled (unsupported bytecode)\n",
              fn->name ? fn->name->chars.c_str() : "<script>");
    }
    return;
  }
  buf->finalize();
  fn->jitEntry = reinterpret_cast<void*>(buf->entry<JitEntry>());
  fn->jitState = JitState::COMPILED;
  if (logJit) {
    fprintf(stderr, "[jit] compiled %s (%zu bytecode -> %zu native bytes)\n",
            fn->name ? fn->name->chars.c_str() : "<script>",
            fn->chunk.code.size(), buf->size());
  }
  codeRegistry().push_back(std::move(buf));
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
}  // namespace jit

#endif
