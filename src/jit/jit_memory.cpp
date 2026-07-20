#include "jit_memory.h"

#if EMBER_JIT_SUPPORTED

#include <cstring>
#include <sys/mman.h>

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#endif

CodeBuffer::CodeBuffer(size_t capacity) : capacity_(capacity) {
#if defined(__APPLE__)
  void* mem = mmap(nullptr, capacity, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
#else
  void* mem = mmap(nullptr, capacity, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
  if (mem == MAP_FAILED) return;
  mem_ = static_cast<uint8_t*>(mem);
#if defined(__APPLE__)
  pthread_jit_write_protect_np(0);  // writable, not executable, this thread
#endif
}

CodeBuffer::~CodeBuffer() {
  if (mem_ != nullptr) munmap(mem_, capacity_);
}

void CodeBuffer::emit32(uint32_t word) {
  if (mem_ == nullptr || finalized_ || full()) {
    mem_ = nullptr;  // poison: compilation failed, caller checks valid()
    return;
  }
  memcpy(mem_ + size_, &word, sizeof(word));
  size_ += sizeof(word);
}

void CodeBuffer::patch32(size_t offset, uint32_t word) {
  if (mem_ == nullptr || finalized_ || offset + 4 > size_) return;
  memcpy(mem_ + offset, &word, sizeof(word));
}

void CodeBuffer::finalize() {
  if (mem_ == nullptr || finalized_) return;
  finalized_ = true;
#if defined(__APPLE__)
  pthread_jit_write_protect_np(1);  // executable, not writable
  sys_icache_invalidate(mem_, size_);
#else
  mprotect(mem_, capacity_, PROT_READ | PROT_EXEC);
  __builtin___clear_cache(reinterpret_cast<char*>(mem_),
                          reinterpret_cast<char*>(mem_ + size_));
#endif
}

#endif  // EMBER_JIT_SUPPORTED
