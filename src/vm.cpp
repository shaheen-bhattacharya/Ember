#include "vm.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "compiler.h"
#include "debug.h"
#include "jit/jit_compile.h"
#include "memory.h"
#include "profile.h"

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

static Value sqrtNative(int argCount, Value* args) {
  if (argCount != 1 || !args[0].isNumber()) return Value::nil();
  return Value::number(sqrt(args[0].as.number));
}

static Value absNative(int argCount, Value* args) {
  if (argCount != 1 || !args[0].isNumber()) return Value::nil();
  return Value::number(fabs(args[0].as.number));
}

static Value lenNative(int argCount, Value* args) {
  if (argCount != 1 || !args[0].isString()) return Value::nil();
  return Value::number(static_cast<double>(asString(args[0])->chars.size()));
}

static Value floorNative(int argCount, Value* args) {
  if (argCount != 1 || !args[0].isNumber()) return Value::nil();
  return Value::number(floor(args[0].as.number));
}

static Value ceilNative(int argCount, Value* args) {
  if (argCount != 1 || !args[0].isNumber()) return Value::nil();
  return Value::number(ceil(args[0].as.number));
}

static Value roundNative(int argCount, Value* args) {
  if (argCount != 1 || !args[0].isNumber()) return Value::nil();
  return Value::number(round(args[0].as.number));
}

static Value substrNative(int argCount, Value* args) {
  if (argCount != 3 || !args[0].isString() || !args[1].isNumber() ||
      !args[2].isNumber()) {
    return Value::nil();
  }
  double startD = args[1].as.number;
  double lenD = args[2].as.number;
  if (startD < 0 || lenD < 0) return Value::nil();
  const std::string& s = asString(args[0])->chars;
  size_t start = static_cast<size_t>(startD);
  if (start >= s.size()) return Value::object(copyString(""));
  return Value::object(
      copyString(s.substr(start, static_cast<size_t>(lenD))));
}

static Value minNative(int argCount, Value* args) {
  if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber()) {
    return Value::nil();
  }
  return Value::number(fmin(args[0].as.number, args[1].as.number));
}

static Value maxNative(int argCount, Value* args) {
  if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber()) {
    return Value::nil();
  }
  return Value::number(fmax(args[0].as.number, args[1].as.number));
}

// ---- profiling helpers ----

static inline uint8_t typeBit(const Value& v) {
  switch (v.type) {
    case ValueType::NIL: return FB_NIL;
    case ValueType::BOOL: return FB_BOOL;
    case ValueType::NUMBER: return FB_NUMBER;
    case ValueType::OBJ:
      switch (v.as.obj->type) {
        case ObjType::STRING: return FB_STRING;
        case ObjType::FUNCTION:
        case ObjType::NATIVE:
        case ObjType::CLOSURE: return FB_CALLABLE;
        default: return FB_OTHER;
      }
  }
  return FB_OTHER;
}

static void maybeMarkHot(ObjFunction* fn, bool logHot) {
  if (fn->hot) return;
  if (fn->callCount < kHotCallThreshold && fn->backEdges < kHotBackEdgeThreshold) {
    return;
  }
  fn->hot = true;
  if (logHot) {
    fprintf(stderr, "[hot] %s is hot (%u calls, %u back-edges)\n",
            fn->name ? fn->name->chars.c_str() : "<script>", fn->callCount,
            fn->backEdges);
  }
}

static void recordCallSite(ObjFunction* caller, int offset,
                           const Value& callee) {
  if (!callee.isObj()) return;
  // Cache the code identity, not the closure instance: every call to
  // makeCounter() yields a fresh closure, but the JIT cares about the target.
  Obj* target = callee.as.obj->type == ObjType::CLOSURE
                    ? static_cast<Obj*>(asClosure(callee)->function)
                    : callee.as.obj;
  for (CallSiteFeedback& site : caller->callSites) {
    if (site.offset == offset) {
      site.count++;
      if (!site.polymorphic && site.target != target) {
        site.polymorphic = true;
        site.target = nullptr;
      }
      return;
    }
  }
  caller->callSites.push_back({offset, target, 1, false});
}

// ---- VM ----

VM::VM() {
  resetStack();
  gHeap.vm = this;
  traceExecution_ = getenv("EMBER_TRACE") != nullptr;
  logHot_ = getenv("EMBER_LOG_HOT") != nullptr;
  gHeap.logGC = getenv("EMBER_LOG_GC") != nullptr;
  defineNative("clock", clockNative);
  defineNative("str", strNative);
  defineNative("sqrt", sqrtNative);
  defineNative("abs", absNative);
  defineNative("floor", floorNative);
  defineNative("ceil", ceilNative);
  defineNative("round", roundNative);
  defineNative("len", lenNative);
  defineNative("substr", substrNative);
  defineNative("min", minNative);
  defineNative("max", maxNative);
}

VM::~VM() {
  if (getenv("EMBER_PROFILE") != nullptr) printProfile();
  gHeap.vm = nullptr;
  gHeap.gcEnabled = false;
  gHeap.freeObjects();
}

void VM::resetStack() {
  stackTop_ = stack_;
  frameCount_ = 0;
  openUpvalues_ = nullptr;
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
    ObjFunction* function = frame->closure->function;
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
    gHeap.markObject(frames_[i].closure);
  }
  for (ObjUpvalue* upvalue = openUpvalues_; upvalue != nullptr;
       upvalue = upvalue->nextOpen) {
    gHeap.markObject(upvalue);
  }
  for (auto& entry : globals_) {
    gHeap.markObject(entry.first);
    gHeap.markValue(entry.second);
  }
}

bool VM::call(ObjClosure* closure, int argCount) {
  ObjFunction* function = closure->function;
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.", function->arity,
                 argCount);
    return false;
  }
  if (frameCount_ == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }
  // Reserve headroom for the frame's worst case (256 locals) so pushes inside
  // the callee can't run off the end of the value stack.
  if ((stackTop_ - stack_) + (UINT8_MAX + 1) > STACK_MAX) {
    runtimeError("Value stack overflow.");
    return false;
  }
  function->callCount++;
  maybeMarkHot(function, logHot_);
  CallFrame* frame = &frames_[frameCount_++];
  frame->closure = closure;
  frame->ip = function->chunk.code.data();
  frame->slots = stackTop_ - argCount - 1;
  return true;
}

bool VM::callValue(const Value& callee, int argCount) {
  if (callee.isObj()) {
    switch (callee.as.obj->type) {
      case ObjType::CLOSURE: {
        ObjClosure* closure = asClosure(callee);
        ObjFunction* fn = closure->function;
        if (jit::enabled()) {
          if (fn->jitState == JitState::NONE &&
              fn->callCount + 1 >= jit::threshold()) {
            jit::compile(fn);
          }
          if (fn->jitState == JitState::COMPILED) {
            // Tier 1: run native code for the whole activation. The entry
            // pops the frame and leaves the result on the stack, so the
            // interpreter resumes in the caller transparently.
            if (!call(closure, argCount)) return false;
            return jit::execute(this, fn) != 0;
          }
        }
        return call(closure, argCount);
      }
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
  runtimeError("Can only call functions; got a %s.", typeName(callee));
  return false;
}

// Returns the upvalue for a stack slot, reusing an existing open one so every
// closure over the same variable shares the same storage.
ObjUpvalue* VM::captureUpvalue(Value* local) {
  ObjUpvalue* prev = nullptr;
  ObjUpvalue* upvalue = openUpvalues_;
  while (upvalue != nullptr && upvalue->location > local) {
    prev = upvalue;
    upvalue = upvalue->nextOpen;
  }
  if (upvalue != nullptr && upvalue->location == local) return upvalue;

  ObjUpvalue* created = gHeap.allocate<ObjUpvalue>(local);
  created->nextOpen = upvalue;
  if (prev == nullptr) {
    openUpvalues_ = created;
  } else {
    prev->nextOpen = created;
  }
  return created;
}

// Closes every open upvalue at or above `last`: the stack slots are about to
// be popped, so the values move into the upvalue objects themselves.
void VM::closeUpvalues(Value* last) {
  while (openUpvalues_ != nullptr && openUpvalues_->location >= last) {
    ObjUpvalue* upvalue = openUpvalues_;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    openUpvalues_ = upvalue->nextOpen;
  }
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
  ObjClosure* closure = gHeap.allocate<ObjClosure>(function);
  pop();
  push(Value::object(closure));
  call(closure, 0);
  gHeap.gcEnabled = true;
  return run(0);
}

InterpretResult VM::run(int stopDepth) {
  CallFrame* frame = &frames_[frameCount_ - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
  (frame->ip += 2,   \
   static_cast<uint16_t>((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
  (frame->closure->function->chunk.constants[READ_BYTE()])
#define READ_STRING() (static_cast<ObjString*>(READ_CONSTANT().as.obj))
// Records the observed operand types at the current opcode's offset. Valid
// only in cases whose operands are still on the stack and whose bytecode
// operands are unread (ip is one past the opcode byte).
#define RECORD_TYPES(mask)                                         \
  do {                                                             \
    ObjFunction* fbFn = frame->closure->function;                  \
    fbFn->feedback[frame->ip - fbFn->chunk.code.data() - 1] |=     \
        (mask);                                                    \
  } while (false)

#define BINARY_OP(op)                                     \
  do {                                                    \
    RECORD_TYPES(typeBit(peek(0)) | typeBit(peek(1)));    \
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
          frame->closure->function->chunk,
          static_cast<int>(frame->ip -
                           frame->closure->function->chunk.code.data()));
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
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_EQUAL: {
        RECORD_TYPES(typeBit(peek(0)) | typeBit(peek(1)));
        Value b = pop();
        Value a = pop();
        push(Value::boolean(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER: {
        RECORD_TYPES(typeBit(peek(0)) | typeBit(peek(1)));
        if (peek(0).isString() && peek(1).isString()) {
          ObjString* b = asString(pop());
          ObjString* a = asString(pop());
          push(Value::boolean(a->chars > b->chars));
        } else if (peek(0).isNumber() && peek(1).isNumber()) {
          double b = pop().as.number;
          double a = pop().as.number;
          push(Value::boolean(a > b));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return InterpretResult::RUNTIME_ERROR;
        }
        break;
      }
      case OP_LESS: {
        RECORD_TYPES(typeBit(peek(0)) | typeBit(peek(1)));
        if (peek(0).isString() && peek(1).isString()) {
          ObjString* b = asString(pop());
          ObjString* a = asString(pop());
          push(Value::boolean(a->chars < b->chars));
        } else if (peek(0).isNumber() && peek(1).isNumber()) {
          double b = pop().as.number;
          double a = pop().as.number;
          push(Value::boolean(a < b));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return InterpretResult::RUNTIME_ERROR;
        }
        break;
      }
      case OP_ADD: {
        RECORD_TYPES(typeBit(peek(0)) | typeBit(peek(1)));
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
        RECORD_TYPES(typeBit(peek(0)));
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
        ObjFunction* fn = frame->closure->function;
        fn->backEdges++;
        maybeMarkHot(fn, logHot_);
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        ObjFunction* caller = frame->closure->function;
        recordCallSite(
            caller,
            static_cast<int>(frame->ip - caller->chunk.code.data()) - 2,
            peek(argCount));
        if (!callValue(peek(argCount), argCount)) {
          return InterpretResult::RUNTIME_ERROR;
        }
        frame = &frames_[frameCount_ - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction* function = asFunction(READ_CONSTANT());
        ObjClosure* closure = gHeap.allocate<ObjClosure>(function);
        // Push before capturing: captureUpvalue allocates, and the closure
        // must be rooted if that allocation triggers a collection.
        push(Value::object(closure));
        for (int i = 0; i < function->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(stackTop_ - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        frameCount_--;
        if (frameCount_ == 0) {
          pop();  // the script function itself
          return InterpretResult::OK;
        }
        stackTop_ = frame->slots;
        push(result);
        if (frameCount_ == stopDepth) return InterpretResult::OK;
        frame = &frames_[frameCount_ - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef RECORD_TYPES
#undef BINARY_OP
}
