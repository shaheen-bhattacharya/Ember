#include "profile.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "chunk.h"
#include "memory.h"
#include "object.h"

namespace {

int instructionLength(const Chunk& chunk, int offset) {
  switch (chunk.code[offset]) {
    case OP_CONSTANT:
    case OP_GET_LOCAL:
    case OP_SET_LOCAL:
    case OP_GET_GLOBAL:
    case OP_DEFINE_GLOBAL:
    case OP_SET_GLOBAL:
    case OP_GET_UPVALUE:
    case OP_SET_UPVALUE:
    case OP_CALL:
      return 2;
    case OP_JUMP:
    case OP_JUMP_IF_FALSE:
    case OP_LOOP:
    case OP_CONSTANT_LONG:
      return 3;
    case OP_CLOSURE: {
      ObjFunction* fn = asFunction(chunk.constants[chunk.code[offset + 1]]);
      return 2 + 2 * fn->upvalueCount;
    }
    default:
      return 1;
  }
}

const char* opName(uint8_t op) {
  switch (op) {
    case OP_EQUAL: return "OP_EQUAL";
    case OP_GREATER: return "OP_GREATER";
    case OP_LESS: return "OP_LESS";
    case OP_ADD: return "OP_ADD";
    case OP_SUBTRACT: return "OP_SUBTRACT";
    case OP_MULTIPLY: return "OP_MULTIPLY";
    case OP_DIVIDE: return "OP_DIVIDE";
    case OP_MODULO: return "OP_MODULO";
    case OP_NEGATE: return "OP_NEGATE";
    default: return "?";
  }
}

std::string maskToString(uint8_t mask) {
  struct BitName {
    uint8_t bit;
    const char* name;
  };
  static const BitName bits[] = {
      {FB_NUMBER, "number"}, {FB_STRING, "string"},     {FB_BOOL, "bool"},
      {FB_NIL, "nil"},       {FB_CALLABLE, "callable"}, {FB_OTHER, "object"},
  };
  std::string out;
  for (const BitName& b : bits) {
    if (mask & b.bit) {
      if (!out.empty()) out += "|";
      out += b.name;
    }
  }
  return out;
}

const char* functionName(const ObjFunction* fn) {
  return fn->name ? fn->name->chars.c_str() : "<script>";
}

const char* targetName(const Obj* target) {
  if (target == nullptr) return "?";
  if (target->type == ObjType::NATIVE) return "<native fn>";
  return functionName(static_cast<const ObjFunction*>(target));
}

}  // namespace

void printProfile() {
  std::vector<ObjFunction*> functions;
  for (Obj* object = gHeap.objects; object != nullptr; object = object->next) {
    if (object->type != ObjType::FUNCTION) continue;
    ObjFunction* fn = static_cast<ObjFunction*>(object);
    if (fn->callCount > 0 || fn->backEdges > 0) functions.push_back(fn);
  }
  std::sort(functions.begin(), functions.end(),
            [](const ObjFunction* a, const ObjFunction* b) {
              return a->callCount > b->callCount;
            });

  fprintf(stderr, "== profile ==\n");
  for (ObjFunction* fn : functions) {
    fprintf(stderr, "%s: %u calls, %u back-edges%s\n", functionName(fn),
            fn->callCount, fn->backEdges, fn->hot ? ", HOT" : "");

    int codeSize = static_cast<int>(fn->chunk.code.size());
    for (int offset = 0; offset < codeSize;
         offset += instructionLength(fn->chunk, offset)) {
      uint8_t mask = fn->feedback[offset];
      if (mask == 0) continue;
      bool monomorphic = (mask & (mask - 1)) == 0;
      fprintf(stderr, "  @%04d %-12s %s (%s)\n", offset,
              opName(fn->chunk.code[offset]), maskToString(mask).c_str(),
              monomorphic ? "monomorphic" : "polymorphic");
    }

    std::vector<CallSiteFeedback> sites = fn->callSites;
    std::sort(sites.begin(), sites.end(),
              [](const CallSiteFeedback& a, const CallSiteFeedback& b) {
                return a.offset < b.offset;
              });
    for (const CallSiteFeedback& site : sites) {
      if (site.polymorphic) {
        fprintf(stderr, "  @%04d OP_CALL      polymorphic (%u calls)\n",
                site.offset, site.count);
      } else {
        fprintf(stderr, "  @%04d OP_CALL      -> %s (monomorphic, %u calls)\n",
                site.offset, targetName(site.target), site.count);
      }
    }
  }
}
