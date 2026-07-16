#include "vm.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "compiler.h"
#include "debug.h"
#include "memory.h"

// ---- native functions ----

static Value clockNative(int, Value*) {
  using namespace std::chrono;
  double seconds =
      duration_cast<duration<double>>(steady_clock::now().time_since_epoch())
          .count();
  return Value::number(seconds);
}

static Value strNative(int argCount, Value* args) {
  if (argCount != 1) return Value::object(copyString(""));
  return Value::object(copyString(valueToString(args[0])));
}

// ---- VM ----

VM::VM() {
  resetStack();
  gHeap.vm = this;
  traceExecution_ = getenv("EMBER_TRACE") != nullptr;
  gHeap.logGC = getenv("EMBER_LOG_GC") != nullptr;
  defineNative("clock", clockNative);
  defineNative("str", strNative);
}

VM::~VM() {
  gHeap.vm = nullptr;
  gHeap.gcEnabled = false;
  gHeap.freeObjects();
}

void VM::resetStack() {
  stackTop_ = stack_;
  frameCount_ = 0;
}

void VM::push(const Value& value) { *stackTop_++ = value; }

Value VM::pop() { return *--stackTop_; }

const Value& VM::peek(int distance) const { return stackTop_[-1 - distance]; }

void VM::runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // Stack trace, innermost frame first.
  for (int i = frameCount_ - 1; i >= 0; i--) {
    CallFrame* frame = &frames_[i];
    ObjFunction* function = frame->function;
    size_t instruction = frame->ip - function->chunk.code.data() - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
    if (function->name == nullptr) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars.c_str());
    }
  }
  resetStack();
}

void VM::defineNative(const char* name, NativeFn function) {
  ObjString* nameStr = copyString(name);
  ObjNative* native = gHeap.allocate<ObjNative>(function);
  globals_[nameStr] = Value::object(native);
}

void VM::markRoots() {
  for (Value* slot = stack_; slot < stackTop_; slot++) {
    gHeap.markValue(*slot);
  }
  for (int i = 0; i < frameCount_; i++) {
    gHeap.markObject(frames_[i].function);
  }
  for (auto& entry : globals_) {
    gHeap.markObject(entry.first);
    gHeap.markValue(entry.second);
  }
}

bool VM::call(ObjFunction* function, int argCount) {
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.", function->arity,
                 argCount);
    return false;
  }
  if (frameCount_ == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }
  CallFrame* frame = &frames_[frameCount_++];
  frame->function = function;
  frame->ip = function->chunk.code.data();
  frame->slots = stackTop_ - argCount - 1;
  return true;
}

bool VM::callValue(const Value& callee, int argCount) {
  if (callee.isObj()) {
    switch (callee.as.obj->type) {
      case ObjType::FUNCTION:
        return call(asFunction(callee), argCount);
      case ObjType::NATIVE: {
        NativeFn native = asNative(callee)->function;
        Value result = native(argCount, stackTop_ - argCount);
        stackTop_ -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break;
    }
  }
  runtimeError("Can only call functions.");
  return false;
}

void VM::concatenate() {
  ObjString* b = asString(peek(0));
  ObjString* a = asString(peek(1));
  ObjString* result = copyString(a->chars + b->chars);
  pop();
  pop();
  push(Value::object(result));
}

InterpretResult VM::interpret(const std::string& source) {
  gHeap.gcEnabled = false;  // compile-time objects aren't rooted yet
  ObjFunction* function = compileSource(source);
  if (function == nullptr) return InterpretResult::COMPILE_ERROR;

  push(Value::object(function));
  call(function, 0);
  gHeap.gcEnabled = true;
  return run();
}

InterpretResult VM::run() {
  CallFrame* frame = &frames_[frameCount_ - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
  (frame->ip += 2,   \
   static_cast<uint16_t>((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants[READ_BYTE()])
#define READ_STRING() (static_cast<ObjString*>(READ_CONSTANT().as.obj))
#define BINARY_OP(op)                                     \
  do {                                                    \
    if (!peek(0).isNumber() || !peek(1).isNumber()) {     \
      runtimeError("Operands must be numbers.");          \
      return InterpretResult::RUNTIME_ERROR;              \
    }                                                     \
    double b = pop().as.number;                           \
    double a = pop().as.number;                           \
    push(op);                                             \
  } while (false)

  for (;;) {
    if (traceExecution_) {
      fprintf(stderr, "          ");
      for (Value* slot = stack_; slot < stackTop_; slot++) {
        fprintf(stderr, "[ %s ]", valueToString(*slot).c_str());
      }
      fprintf(stderr, "\n");
      disassembleInstruction(
          frame->function->chunk,
          static_cast<int>(frame->ip - frame->function->chunk.code.data()));
    }

    uint8_t instruction = READ_BYTE();
    switch (instruction) {
      case OP_CONSTANT:
        push(READ_CONSTANT());
        break;
      case OP_NIL:
        push(Value::nil());
        break;
      case OP_TRUE:
        push(Value::boolean(true));
        break;
      case OP_FALSE:
        push(Value::boolean(false));
        break;
      case OP_POP:
        pop();
        break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        auto it = globals_.find(name);
        if (it == globals_.end()) {
          runtimeError("Undefined variable '%s'.", name->chars.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        push(it->second);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        globals_[name] = peek(0);
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        auto it = globals_.find(name);
        if (it == globals_.end()) {
          runtimeError("Undefined variable '%s'.", name->chars.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        it->second = peek(0);
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(Value::boolean(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:
        BINARY_OP(Value::boolean(a > b));
        break;
      case OP_LESS:
        BINARY_OP(Value::boolean(a < b));
        break;
      case OP_ADD: {
        if (peek(0).isString() && peek(1).isString()) {
          concatenate();
        } else if (peek(0).isNumber() && peek(1).isNumber()) {
          double b = pop().as.number;
          double a = pop().as.number;
          push(Value::number(a + b));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return InterpretResult::RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT:
        BINARY_OP(Value::number(a - b));
        break;
      case OP_MULTIPLY:
        BINARY_OP(Value::number(a * b));
        break;
      case OP_DIVIDE:
        BINARY_OP(Value::number(a / b));
        break;
      case OP_MODULO:
        BINARY_OP(Value::number(fmod(a, b)));
        break;
      case OP_NOT:
        push(Value::boolean(isFalsey(pop())));
        break;
      case OP_NEGATE:
        if (!peek(0).isNumber()) {
          runtimeError("Operand must be a number.");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(Value::number(-pop().as.number));
        break;
      case OP_PRINT:
        printf("%s\n", valueToString(pop()).c_str());
        break;
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return InterpretResult::RUNTIME_ERROR;
        }
        frame = &frames_[frameCount_ - 1];
        break;
      }
      case OP_RETURN: {
        Value result = pop();
        frameCount_--;
        if (frameCount_ == 0) {
          pop();  // the script function itself
          return InterpretResult::OK;
        }
        stackTop_ = frame->slots;
        push(result);
        frame = &frames_[frameCount_ - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}
