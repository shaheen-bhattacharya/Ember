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
  ObjFunction* function;
  uint8_t* ip;
  Value* slots;  // window into the VM value stack for this frame's locals
};

class VM {
 public:
  VM();
  ~VM();

  InterpretResult interpret(const std::string& source);
  void markRoots();  // called by the GC

 private:
  static constexpr int FRAMES_MAX = 256;
  static constexpr int STACK_MAX = FRAMES_MAX * 256;

  CallFrame frames_[FRAMES_MAX];
  int frameCount_ = 0;
  Value stack_[STACK_MAX];
  Value* stackTop_ = stack_;
  std::unordered_map<ObjString*, Value> globals_;
  bool traceExecution_ = false;

  InterpretResult run();
  void resetStack();
  void push(const Value& value);
  Value pop();
  const Value& peek(int distance) const;
  bool call(ObjFunction* function, int argCount);
  bool callValue(const Value& callee, int argCount);
  void concatenate();
  void defineNative(const char* name, NativeFn function);
  void runtimeError(const char* format, ...);
};
