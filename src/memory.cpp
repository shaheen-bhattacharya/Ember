#include "memory.h"

#include <cstdio>

#include "object.h"
#include "vm.h"

Heap gHeap;

void Heap::markObject(Obj* object) {
  if (object == nullptr || object->marked) return;
  object->marked = true;

  switch (object->type) {
    case ObjType::STRING:
    case ObjType::NATIVE:
      break;  // no outgoing references
    case ObjType::FUNCTION: {
      ObjFunction* fn = static_cast<ObjFunction*>(object);
      markObject(fn->name);
      for (const Value& constant : fn->chunk.constants) {
        markValue(constant);
      }
      break;
    }
    case ObjType::CLOSURE: {
      ObjClosure* closure = static_cast<ObjClosure*>(object);
      markObject(closure->function);
      for (ObjUpvalue* upvalue : closure->upvalues) {
        markObject(upvalue);
      }
      break;
    }
    case ObjType::UPVALUE:
      markValue(static_cast<ObjUpvalue*>(object)->closed);
      break;
    case ObjType::ARRAY:
      for (const Value& item : static_cast<ObjArray*>(object)->items) {
        markValue(item);
      }
      break;
  }
}

void Heap::markValue(const Value& value) {
  if (value.isObj()) markObject(value.as.obj);
}

void Heap::collectGarbage() {
  size_t before = bytesAllocated;

  // Mark phase: roots live in the VM (stack, frames, globals).
  if (vm != nullptr) vm->markRoots();

  // The intern table holds strings weakly: drop entries that are about to die
  // so future interning can't hand out a dangling pointer.
  for (auto it = strings.begin(); it != strings.end();) {
    if (!it->second->marked) {
      it = strings.erase(it);
    } else {
      ++it;
    }
  }

  // Sweep phase: free everything unmarked, unmark survivors.
  Obj** link = &objects;
  while (*link != nullptr) {
    Obj* object = *link;
    if (object->marked) {
      object->marked = false;
      link = &object->next;
    } else {
      *link = object->next;
      bytesAllocated -= object->gcSize;
      delete object;
    }
  }

  nextGC = bytesAllocated * 2;
  if (nextGC < 1024 * 1024) nextGC = 1024 * 1024;

  if (logGC) {
    fprintf(stderr, "[gc] collected %zu bytes (%zu -> %zu), next at %zu\n",
            before - bytesAllocated, before, bytesAllocated, nextGC);
  }
}

void Heap::freeObjects() {
  Obj* object = objects;
  while (object != nullptr) {
    Obj* next = object->next;
    delete object;
    object = next;
  }
  objects = nullptr;
  strings.clear();
  bytesAllocated = 0;
}
