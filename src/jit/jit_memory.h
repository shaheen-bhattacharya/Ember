#pragma once

#include <cstddef>
#include <cstdint>

// Tier-1 baseline JIT targets AArch64 only; every other architecture keeps
// running the tier-0 interpreter.
#if defined(__aarch64__) || defined(_M_ARM64)
#define EMBER_JIT_SUPPORTED 1
#else
#define EMBER_JIT_SUPPORTED 0
#endif

#if EMBER_JIT_SUPPORTED

// A block of executable memory with W^X discipline: writable while code is
// being emitted, then flipped to executable by finalize() (which also
// invalidates the instruction cache). On macOS this uses MAP_JIT +
// pthread_jit_write_protect_np; elsewhere mmap + mprotect.
class CodeBuffer {
 public:
  explicit CodeBuffer(size_t capacity);
  ~CodeBuffer();

  CodeBuffer(const CodeBuffer&) = delete;
  CodeBuffer& operator=(const CodeBuffer&) = delete;

  bool valid() const { return mem_ != nullptr; }
  bool full() const { return size_ + 4 > capacity_; }
  size_t size() const { return size_; }
  const uint8_t* base() const { return mem_; }

  void emit32(uint32_t word);
  void patch32(size_t offset, uint32_t word);
  void finalize();

  template <typename F>
  F entry() const {
    return reinterpret_cast<F>(reinterpret_cast<void*>(mem_));
  }

 private:
  uint8_t* mem_ = nullptr;
  size_t capacity_ = 0;
  size_t size_ = 0;
  bool finalized_ = false;
};

#endif  // EMBER_JIT_SUPPORTED
