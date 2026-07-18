# Ember design notes

## Tier 0 decisions and their rationale

**Stack-based bytecode.** Tier 0 optimizes for simplicity and debuggability: a
stack machine needs no register allocation in the compiler, and the
disassembly maps 1:1 to source structure. The cost is more dispatch per useful
operation than a register machine (Lua-style). That cost is deliberate — it
makes the tier-1 baseline JIT speedup measurable and honest.

**Single-pass compiler, no AST.** The Pratt parser emits bytecode as it
parses. This is the fastest possible compile pipeline (one token of
lookahead, no tree allocation) and is exactly what a baseline JIT tier wants:
compile latency is part of a JIT's cost model. The optimizing tier will build
its own SSA IR *from bytecode*, not from source — same as V8's Maglev/Turbofan
and the JVM's C2.

**Call frames are stack windows.** A `CallFrame` is `{function, ip, slots}`
where `slots` points into the single contiguous value stack. Locals are just
`slots[i]` — no separate environment objects, no hashing on the hot path.
Frame setup is ~4 stores, which is why `fib` (call-heavy) is a fair benchmark.

**Tagged-union values (16 bytes).** `{type tag, union{bool, double, Obj*}}`.
NaN-boxing (packing everything into 8 bytes inside a canonical NaN) roughly
halves stack traffic and is planned, but the tagged union keeps every value
readable in a debugger while the VM's semantics are still settling.

**Mark-sweep GC, collection only at allocation points.** All heap objects
live on one intrusive linked list. Roots are the value stack, call-frame
functions, and the globals table; reachability flows through function constant
pools (a function keeps its nested functions and string constants alive). The
collector runs when allocated bytes cross a threshold that doubles after each
collection. Two correctness subtleties worth knowing:

1. Collection happens *before* the triggering allocation, never after —
   otherwise the brand-new object could be swept before its caller roots it.
2. GC is disabled during compilation. Compile-time objects (functions, string
   constants) are only reachable *after* compilation finishes, via the script
   function; collecting mid-compile would sweep them.

**Interned strings.** Every string, including concatenation results, is
deduplicated through a global table, so string equality is pointer equality.
The intern table is *weak*: it is not a GC root, and dead entries are purged
before the sweep so future interning can't return a dangling pointer.

**Globals are late-bound, locals are compile-time slots.** Referencing an
undefined global is a runtime error (enabling mutual recursion between
top-level functions); locals resolve to stack slot indices at compile time and
cost an array index at runtime.

**Closures via upvalues (the Lua design).** Every function value at runtime is
an `ObjClosure`: compiled code plus an array of `ObjUpvalue*` captures. An
upvalue starts *open* — a pointer into the live stack slot — so reads and
writes through the closure and through the original variable observe each
other. When the variable's slot is about to die (scope exit emits
`OP_CLOSE_UPVALUE`; return closes everything at or above the frame base), the
value migrates into the upvalue object and the pointer repoints at itself.
The VM keeps open upvalues in a list sorted by stack address so two closures
capturing the same variable share one upvalue, which is what makes mutation
through one closure visible through the other. Capture is resolved entirely at
compile time: `resolveUpvalue` threads a capture through every intermediate
function, so the runtime never searches scopes.

## What tier 1 (baseline JIT) will need from this code

- Bytecode is already position-independent within a chunk and uses explicit
  16-bit relative jumps — easy to template-compile.
- Call frames are already laid out like native frames (args contiguous below
  a frame pointer analog), so the JIT calling convention can mirror `slots`.
- The interpreter loop is the deopt target: any JIT bailout re-enters `run()`
  with a reconstructed `CallFrame`.
- Type feedback and hotness counters (below) are recorded; what remains is the
  tier-up trigger itself — swapping a hot function's entry point to JIT code.

## Profiling (the interpreter as tier-0 profiler)

The interpreter records, always-on, the three signals the JIT tiers consume:

- **Hotness**: per-function call and loop-back-edge counters. Crossing a
  threshold (1,000 calls or 10,000 back-edges) marks the function `hot` —
  the future tier-up trigger. `EMBER_LOG_HOT=1` logs the moment it happens.
- **Operand type feedback**: every arithmetic/comparison/negate site ORs a
  bitmask of its observed operand types into a per-function side table indexed
  by bytecode offset (one byte per code byte, no bytecode format changes). A
  site with one bit set is monomorphic — e.g. `OP_ADD number (monomorphic)` —
  and is exactly the site the optimizing tier can specialize to an unboxed
  float add guarded by a type check.
- **Call-site caches**: each `OP_CALL` site remembers its first callee's
  *code* identity (the `ObjFunction`, not the closure instance, since a
  factory can produce a fresh closure per call over the same code). A second
  distinct callee degrades the site to polymorphic. Monomorphic sites are
  inlining candidates.

`EMBER_PROFILE=1` dumps the whole profile at VM teardown, per function, sorted
by call count. Recording costs roughly 20–40% on the benchmarks (fib
0.10s→0.13s, tight loop 0.38s→0.53s) — the standard price of a profiling
interpreter tier, paid only until hot code tiers up.

## Known limitations (deliberate, roadmap items)

- No classes/objects beyond strings, functions, and closures.
- Constant pool capped at 256 entries per chunk (no `OP_CONSTANT_LONG` yet).
- The value stack (65,536 slots) is guarded at call boundaries (each call
  reserves 256 slots of headroom) but not per push, so a single expression
  with pathological temporary depth inside one frame can still overflow.
  Frame count is checked too (256).
