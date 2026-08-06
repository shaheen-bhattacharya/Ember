# Hacking on Ember

The practical companion to [DESIGN.md](DESIGN.md): how to build, test, and
make the two most common kinds of change without missing a step.

## Build and test

```sh
make            # optimized build (clang++, -O2)
make debug      # ASan + -O0; use while touching the VM, GC, or JIT
make test       # golden-output suite in tests/
make bench      # each benchmark, interpreter vs JIT, best of 3
```

CI runs the suite four ways; before pushing runtime changes, do the same:

```sh
./tests/run_tests.sh                             # default (JIT at threshold)
EMBER_JIT_THRESHOLD=1 ./tests/run_tests.sh       # everything tiers up immediately
EMBER_JIT=0 ./tests/run_tests.sh                 # interpreter only
EMBER_GC_STRESS=1 EMBER_JIT_THRESHOLD=1 ./tests/run_tests.sh  # collect on every allocation
```

The stress run is the one that catches rooting bugs: a value that is alive
but unreachable from the stack, frames, or globals will be swept mid-use.

## Golden tests

A test is a `.em` file in `tests/` whose expected output is annotated inline:

```
print 1 + 2;   // expect: 3
```

The runner diffs the program's combined stdout+stderr against the `expect`
lines in file order. Two things to know:

- Runtime errors print to stderr, which is unbuffered — in a test that mixes
  `print` output with a runtime error, the error text appears *first* in the
  captured output. Keep error-path tests in their own files (see
  `errors.em`).
- An `// expect:` with nothing after the space won't match; print a trailing
  `+ "!"` sentinel instead when asserting empty strings.

## Adding a native function

Natives live at the top of `src/vm.cpp`: a `static Value fooNative(int
argCount, Value* args)` plus one `defineNative("foo", fooNative)` line in the
VM constructor. Conventions:

- Validate arity and types first; answer `Value::nil()` for bad input rather
  than raising a runtime error.
- Mutating array natives return the array for chaining (`sort`, `reverse`);
  removal natives return the removed value (`pop`).
- **GC hazard:** if the native allocates a container and then allocates its
  contents (e.g. an array of strings), the container is unreachable from any
  root between those allocations. Pause collection across construction the
  way `split` does, and verify under `EMBER_GC_STRESS=1`.

Add a golden test covering good inputs, edge cases, and bad-input nils.

## Adding an opcode

More places than you'd guess — miss one and it fails at a distance:

1. `src/chunk.h` — the `OpCode` enum (append; order is ABI to the dispatch table).
2. `src/vm.cpp` — the computed-goto label table **and** its `static_assert`,
   plus the `VM_CASE` handler.
3. `src/debug.cpp` — a disassembler entry, or `--dump` and `EMBER_TRACE`
   will desynchronize on the unknown byte.
4. `src/jit/jit_compile.cpp` — `supportedLength()` and an `emitOp` case.
   The cheap route is a runtime helper (`emitHelper1` + a `JitRuntime`
   wrapper in `jit_runtime.{h,cpp}`); without any entry, every function
   containing the opcode silently stays on the interpreter.
5. `docs/BYTECODE.md` — the opcode table.

Verify the JIT path compiled rather than fell back:

```sh
EMBER_LOG_JIT=1 EMBER_JIT_THRESHOLD=1 ./ember your_test.em
```

## Seeing what's happening

```sh
./ember --dump file.em            # bytecode without running
EMBER_TRACE=1 ./ember file.em     # every instruction + stack
EMBER_PROFILE=1 ./ember file.em   # hotness, type feedback, tiers at exit
EMBER_JIT_DUMP=1 ./ember file.em  # emitted machine code, llvm-mc format
./ember --jit-selftest            # validate code emission on this machine
```
