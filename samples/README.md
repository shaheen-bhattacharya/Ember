# Ember sample programs

Small but complete programs written in Ember, exercising the whole language:
arrays, closures, recursion, `break`/`continue`, compound assignment, and the
native library.

| Program | What it shows |
|---|---|
| [`life.em`](life.em) | Conway's Game of Life on a wrapping grid — nested arrays, grid math with `%`, a glider crawling across four printed generations |
| [`sort.em`](sort.em) | In-place quicksort with a partition helper, a deterministic LCG input generator, and a sortedness check — recursion over array slices |
| [`wordfreq.em`](wordfreq.em) | Word frequency counting via `split` → `sort` → run-length counting — the string/array toolkit end to end |
| [`nqueens.em`](nqueens.em) | N-Queens backtracking — an array as an explicit stack of column choices, diagonal math with `abs`, counts checked against the known sequence |
| [`cipher.em`](cipher.em) | Caesar cipher encode/decode/crack — `chr`/`ord` arithmetic, ROT13 round-trip, and a frequency-scored brute-force that recovers the shift |

Run any of them with:

```sh
make && ./ember samples/life.em
```

They also make decent workloads: `EMBER_PROFILE=1` shows which functions go
hot, and `EMBER_LOG_JIT=1` shows what compiles (array-heavy functions
currently stay on the interpreter — array opcodes are on the JIT roadmap).
