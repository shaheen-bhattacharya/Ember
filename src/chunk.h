#pragma once

#include <cstdint>
#include <vector>

#include "value.h"

// Bytecode opcodes for the stack-based tier-0 VM.
enum OpCode : uint8_t {
  OP_CONSTANT,       // operand: constant index -> push constants[idx]
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,      // operand: stack slot
  OP_SET_LOCAL,      // operand: stack slot
  OP_GET_GLOBAL,     // operand: constant index of name string
  OP_DEFINE_GLOBAL,  // operand: constant index of name string
  OP_SET_GLOBAL,     // operand: constant index of name string
  OP_GET_UPVALUE,    // operand: upvalue index in the current closure
  OP_SET_UPVALUE,    // operand: upvalue index in the current closure
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MODULO,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,           // operand: 16-bit forward offset
  OP_JUMP_IF_FALSE,  // operand: 16-bit forward offset
  OP_LOOP,           // operand: 16-bit backward offset
  OP_CALL,           // operand: arg count
  OP_CLOSURE,        // operands: fn constant index, then (isLocal, index) per upvalue
  OP_CLOSE_UPVALUE,  // hoist the captured top-of-stack local into the heap
  OP_RETURN,
  OP_CONSTANT_LONG,  // operand: 16-bit constant index, for pools past 256
  OP_ARRAY,          // operand: element count -> pop N elements, push array
  OP_INDEX_GET,      // pop index + target, push element (array or string)
  OP_INDEX_SET,      // pop value + index + array, set, push value
};

// A chunk of bytecode plus its constant pool and line info (parallel to code,
// one entry per byte, used for runtime error reporting).
struct Chunk {
  std::vector<uint8_t> code;
  std::vector<int> lines;
  std::vector<Value> constants;

  void write(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
  }

  int addConstant(const Value& value) {
    constants.push_back(value);
    return static_cast<int>(constants.size()) - 1;
  }
};
