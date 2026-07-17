#include "object.h"

#include "memory.h"

ObjString* copyString(const std::string& s) {
  auto it = gHeap.strings.find(s);
  if (it != gHeap.strings.end()) return it->second;

  ObjString* str = gHeap.allocate<ObjString>(s);
  // Charge the character payload to the GC budget too, not just the header;
  // sweep subtracts gcSize, so the books stay balanced.
  str->gcSize += str->chars.size();
  gHeap.bytesAllocated += str->chars.size();
  gHeap.strings[s] = str;
  return str;
}
