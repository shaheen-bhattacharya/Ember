# Ember design notes

## Tier 0 decisions and their rationale

**Stack-based bytecode.** Tier 0 optimizes for simplicity and debuggability: a
stack machine needs no register allocation in the compiler, and the
disassembly maps 1:1 to source structure. The cost is more dispatch per useful
operation than a register machine (Lua-style). That cost is deliberate — it
makes the tier-1 baseline JIT speedup measurable and honest.

**Computed-goto dispatch.** Where the compiler supports labels-as-values,
every opcode ends with its own indirect branch instead of funneling through
one switch: the branch predictor learns per-opcode-pair patterns. Worth ~30%
on dispatch-bound loops (0.48s → 0.33s) and 25% on call-heavy recursion; the
bodies are written once and a `static_assert` pins dispatch-table order to
the opcode enum.

**Global inline caches (tier 1).** Each compiled global-access site caches
the resolved `unordered_map` node address after its first lookup and then
loads/stores it directly. Sound because node addresses survive rehashes and
globals can't be deleted. Recursion resolves the callee through a global, so
this took fib from 0.061s to 0.036s.

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

## Tier 1: the baseline template JIT (src/jit/)

When a function's call count crosses `EMBER_JIT_THRESHOLD` (default: the
profiler's hot threshold), its bytecode is template-compiled to AArch64 and
subsequent calls run the native entry instead of the dispatch loop.

**Execution model.** Emitted code keeps the VM's value stack as the single
source of truth: x19 holds the VM, x20 points at `stackTop_`, x21 at the
frame's slot base. There is no register allocation — every op loads and
stores the simulated stack, exactly like the interpreter minus dispatch.
That's the deliberate baseline trade-off: correctness and compile speed now,
an SSA tier later.

**Fast paths and helpers.** Arithmetic and comparisons inline the
number-number case (tag guards, unboxed `fadd`/`fcmp` on doubles); everything
else — string concat, globals, equality, print, errors — calls C-ABI helpers
(`ember_jit_*`) that reuse interpreter semantics. Because stackTop is written
back before every helper call, every GC safepoint sees a coherent stack, and
helpers sync the frame ip so error stack traces are line-accurate through JIT
frames.

**Calls.** JIT→JIT calls invoke the callee's native entry directly (C-stack
recursion, bounded by the 256-frame cap). JIT→interpreter uses
`run(stopDepth)` to interpret exactly one activation. Tier-up also triggers
from JIT call sites, so a function called only from compiled code still gets
compiled.

**W^X.** Code buffers are `MAP_JIT` pages on macOS, toggled with
`pthread_jit_write_protect_np` and flushed with `sys_icache_invalidate`
(mprotect + `__builtin___clear_cache` elsewhere). Compiled code lives in a
process-lifetime registry; functions are GC'd, code is not (code GC is a
non-goal for the baseline).

**Closures.** Closure creation, upvalue reads/writes, and upvalue closing all
lower to helpers (the closure is rooted on the stack before capture, since
capture allocates), and return closes the frame's open upvalues — so every
opcode is now compilable and no function is interpreter-bound by shape.
Upvalue access stays helper-bound; inlining it is tier-2 work.

**Compare+branch fusion.** A `LESS/GREATER; JUMP_IF_FALSE; POP` triple whose
branch target is the exit's condition-pop compiles to one `fcmp` plus an
inverted `b.cond` — no boolean is materialized. A fixpoint pass un-fuses any
triple that another jump lands inside of, and the inverted conditions (PL/LE)
include the unordered case so NaN comparisons stay falsey.

**Diagnostics.** `EMBER_LOG_JIT=1` logs each compilation and prints totals at
exit; `EMBER_JIT_DUMP=1` hex-dumps emitted code for `llvm-mc --disassemble`;
`EMBER_GC_STRESS=1` collects on every allocation, which is the harshest test
of the JIT's rooting discipline (CI runs it with the JIT forced hot).

Top-level script code never tiers (it runs once). Measured on Apple Silicon:
tight numeric loops 4.6×, call-heavy recursion 2.3×, closure-heavy calls
1.2×, allocation-bound workloads ~1× (the GC, not dispatch, is the
bottleneck there — as expected).

## Known limitations (deliberate, roadmap items)

- No classes/objects beyond strings, functions, and closures.
- Literal constant pools grow to 65,536 entries (`OP_CONSTANT_LONG`); opcodes
  with one-byte constant operands (global names, function references) still
  cap at 256 per chunk.
- The value stack (65,536 slots) is guarded at call boundaries (each call
  reserves 256 slots of headroom) but not per push, so a single expression
  with pathological temporary depth inside one frame can still overflow.
  Frame count is checked too (256).
