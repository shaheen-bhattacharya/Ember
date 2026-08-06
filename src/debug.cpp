#include "debug.h"

#include <cstdio>

#include "object.h"

static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(const char* name, const Chunk& chunk, int offset) {
  uint8_t slot = chunk.code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int constantInstruction(const char* name, const Chunk& chunk,
                               int offset) {
  uint8_t constant = chunk.code[offset + 1];
  printf("%-16s %4d '%s'\n", name, constant,
         valueToString(chunk.constants[constant]).c_str());
  return offset + 2;
}

static int jumpInstruction(const char* name, int sign, const Chunk& chunk,
                           int offset) {
  uint16_t jump = static_cast<uint16_t>(chunk.code[offset + 1] << 8);
  jump |= chunk.code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

int disassembleInstruction(const Chunk& chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk.lines[offset] == chunk.lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk.lines[offset]);
  }

  uint8_t instruction = chunk.code[offset];
  switch (instruction) {
    case OP_CONSTANT: return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_LONG: {
      uint16_t constant = static_cast<uint16_t>(
          (chunk.code[offset + 1] << 8) | chunk.code[offset + 2]);
      printf("%-16s %4d '%s'\n", "OP_CONSTANT_LONG", constant,
             valueToString(chunk.constants[constant]).c_str());
      return offset + 3;
    }
    case OP_NIL: return simpleInstruction("OP_NIL", offset);
    case OP_TRUE: return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE: return simpleInstruction("OP_FALSE", offset);
    case OP_POP: return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL: return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL: return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_SET_LOCAL_POP: return byteInstruction("OP_SET_LOCAL_POP", chunk, offset);
    case OP_GET_GLOBAL: return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL: return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL: return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE: return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE: return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_EQUAL: return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER: return simpleInstruction("OP_GREATER", offset);
    case OP_LESS: return simpleInstruction("OP_LESS", offset);
    case OP_ADD: return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT: return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY: return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE: return simpleInstruction("OP_DIVIDE", offset);
    case OP_MODULO: return simpleInstruction("OP_MODULO", offset);
    case OP_NOT: return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE: return simpleInstruction("OP_NEGATE", offset);
    case OP_PRINT: return simpleInstruction("OP_PRINT", offset);
    case OP_JUMP: return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP: return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL: return byteInstruction("OP_CALL", chunk, offset);
    case OP_ARRAY: return byteInstruction("OP_ARRAY", chunk, offset);
    case OP_INDEX_GET: return simpleInstruction("OP_INDEX_GET", offset);
    case OP_INDEX_SET: return simpleInstruction("OP_INDEX_SET", offset);
    case OP_CLOSURE: {
      int off = offset + 1;
      uint8_t constant = chunk.code[off++];
      printf("%-16s %4d %s\n", "OP_CLOSURE", constant,
             valueToString(chunk.constants[constant]).c_str());
      ObjFunction* function = asFunction(chunk.constants[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk.code[off++];
        int index = chunk.code[off++];
        printf("%04d      |                     %s %d\n", off - 2,
               isLocal ? "local" : "upvalue", index);
      }
      return off;
    }
    case OP_CLOSE_UPVALUE: return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}

void disassembleChunk(const Chunk& chunk, const char* name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < static_cast<int>(chunk.code.size());) {
    offset = disassembleInstruction(chunk, offset);
  }
  // Recurse into nested function constants so `--dump` shows everything.
  for (const Value& constant : chunk.constants) {
    if (constant.isFunction()) {
      ObjFunction* fn = asFunction(constant);
      disassembleChunk(fn->chunk,
                       fn->name ? fn->name->chars.c_str() : "<anonymous>");
    }
  }
}
