# Ember

A small dynamically-typed language and its runtime, built from scratch in C++17 — the first tier of a planned multi-tier JIT-compiled virtual machine (the same architecture as V8, JavaScriptCore, and the JVM).

**Current status: Tier 0 complete.** Source is lexed, parsed (single-pass Pratt parser, no AST), compiled to a compact stack-based bytecode, and executed by a bytecode interpreter with a precise mark-sweep garbage collector and interned strings.

## The language

```
fun fib(n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
}

var start = clock();
print "fib(30) = " + str(fib(30));
print "took " + str(clock() - start) + "s";
```

Dynamic typing over four value kinds (`nil`, booleans, 64-bit float numbers, heap objects), first-class functions and closures (with shared mutable captures, Lua-style upvalues), lexically-scoped locals, `if`/`else`, `while`, `for`, short-circuiting `and`/`or`, strings with `+` concatenation, and native functions (`clock`, `str`).

## Build and run

```sh
make            # builds ./ember (clang++, -O2)
make test       # runs the golden-output test suite in tests/
make bench      # runs each benchmark in bench/ three times
make debug      # ASan + -O0 build for hacking on the VM

./ember                 # REPL
./ember script.em       # run a file
./ember --dump script.em  # disassemble the compiled bytecode, don't run
EMBER_TRACE=1 ./ember script.em   # trace every instruction + stack
EMBER_LOG_GC=1 ./ember script.em  # log GC cycles
EMBER_PROFILE=1 ./ember script.em # dump hotness + type feedback at exit
EMBER_LOG_HOT=1 ./ember script.em # log when a function crosses the hot threshold
```

## Architecture

```
source ──lexer──> tokens ──compiler (Pratt)──> bytecode chunks ──VM──> output
                                                    │
                                          mark-sweep GC heap
                                       (interned strings, functions)
```

| Component | File | Notes |
|---|---|---|
| Lexer | `src/lexer.cpp` | On-demand tokenizer, zero-copy tokens into source |
| Compiler | `src/compiler.cpp` | Single-pass Pratt parser emitting bytecode directly |
| Bytecode | `src/chunk.h` | 31 opcodes, 16-bit jump offsets, per-byte line info |
| VM | `src/vm.cpp` | Stack machine, call frames as stack windows |
| GC | `src/memory.cpp` | Mark-sweep; roots = VM stack, frames, globals; weak intern table |
| Disassembler | `src/debug.cpp` | Powers `--dump` and `EMBER_TRACE` |
| Profiler | `src/profile.cpp` | Hotness, type feedback, call-site caches; powers `EMBER_PROFILE` |

Design details and the rationale for each decision are in [docs/DESIGN.md](docs/DESIGN.md).

## Roadmap

The point of this project is the full tiered-execution architecture:

- [x] **Tier 0 — bytecode interpreter** (this repo today)
- [x] Closures with upvalues (shared mutable capture, closed on scope exit)
- [x] Type feedback: hotness counters, per-site operand type recording, monomorphic call-site caches (`EMBER_PROFILE=1`)
- [ ] Classes if the fancy strikes
- [ ] **Tier 1 — baseline JIT**: template-compile bytecode to AArch64, `mmap(MAP_JIT)` code pages, interpreter↔JIT calling convention
- [ ] **Tier 2 — optimizing JIT**: SSA IR, type specialization from recorded feedback, inlining, DCE, linear-scan register allocation
- [ ] **Deoptimization**: bail from speculative JIT code back to the interpreter, reconstructing frames
- [ ] **Generational GC**: bump-allocated nursery, write barriers emitted by the JIT, stack maps for JIT frames
- [ ] NaN-boxed values

## Tests and benchmarks

`tests/` holds golden-output tests: each `.em` file annotates its expected output with `// expect:` comments and `tests/run_tests.sh` diffs actual against expected. `bench/` covers the three axes that will matter for the JIT tiers: call-heavy recursion (`fib.em`), tight numeric loops (`loop.em`), and allocation/GC pressure (`string_churn.em`). Tier-0 numbers are the baseline every future tier gets measured against.
