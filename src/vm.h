#pragma once

#include <string>
#include <unordered_map>

#include "object.h"
#include "value.h"

enum class InterpretResult {
  OK,
  COMPILE_ERROR,
  RUNTIME_ERROR,
};

struct CallFrame {
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;  // window into the VM value stack for this frame's locals
};

class VM {
 public:
  VM();
  ~VM();

  InterpretResult interpret(const std::string& source);
  void markRoots();  // called by the GC
  // Populates the `args` global with the command-line arguments that
  // followed the script path (empty for the REPL and -e).
  void defineScriptArgs(int count, const char* const* values);

 private:
  // The baseline JIT's runtime helpers operate directly on VM state.
  friend struct JitRuntime;
  static constexpr int FRAMES_MAX = 256;
  static constexpr int STACK_MAX = FRAMES_MAX * 256;

  CallFrame frames_[FRAMES_MAX];
  int frameCount_ = 0;
  Value stack_[STACK_MAX];
  Value* stackTop_ = stack_;
  std::unordered_map<ObjString*, Value> globals_;
  ObjUpvalue* openUpvalues_ = nullptr;  // sorted by stack slot, outermost first
  bool traceExecution_ = false;
  bool logHot_ = false;

  // Runs bytecode until the frame count drops to stopDepth (0 = whole
  // program). A non-zero stopDepth lets JIT code interpret one callee and get
  // control back when it returns.
  InterpretResult run(int stopDepth);
  void resetStack();
  void push(const Value& value);
  Value pop();
  const Value& peek(int distance) const;
  bool call(ObjClosure* closure, int argCount);
  bool callValue(const Value& callee, int argCount);
  // Shared by the interpreter loop and the JIT helpers.
  void makeArray(int count);
  bool indexGet();
  bool indexSet();
  ObjUpvalue* captureUpvalue(Value* local);
  void closeUpvalues(Value* last);
  void concatenate();
  void defineNative(const char* name, NativeFn function);
  void runtimeError(const char* format, ...);
};
