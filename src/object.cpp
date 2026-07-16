#include "object.h"

#include "memory.h"

ObjString* copyString(const std::string& s) {
  auto it = gHeap.strings.find(s);
  if (it != gHeap.strings.end()) return it->second;

  ObjString* str = gHeap.allocate<ObjString>(s);
  gHeap.strings[s] = str;
  return str;
}
