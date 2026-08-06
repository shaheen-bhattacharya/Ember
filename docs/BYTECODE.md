# Ember bytecode reference

Stack-based, one-byte opcodes, operands inline in the code stream. Constants
live in a per-chunk pool; opcodes with a one-byte operand reach the first 256
entries, and literals past that use `OP_CONSTANT_LONG`. Jumps use unsigned
16-bit big-endian offsets relative to the instruction after the operand. Use
`./ember --dump file.em` to see any script's compiled form.

| Opcode | Operands | Stack effect | Notes |
|---|---|---|---|
| `OP_CONSTANT` | pool index | push | |
| `OP_NIL` / `OP_TRUE` / `OP_FALSE` | — | push | |
| `OP_POP` | — | pop | statement result disposal |
| `OP_GET_LOCAL` | slot | push | slot is frame-relative |
| `OP_SET_LOCAL` | slot | peek | assignment is an expression |
| `OP_GET_GLOBAL` | name pool index | push | runtime error if undefined |
| `OP_DEFINE_GLOBAL` | name pool index | pop | |
| `OP_SET_GLOBAL` | name pool index | peek | runtime error if undefined |
| `OP_GET_UPVALUE` | upvalue index | push | reads through `location` |
| `OP_SET_UPVALUE` | upvalue index | peek | writes through `location` |
| `OP_EQUAL` | — | pop 2, push | any types; interned-pointer equality for strings |
| `OP_GREATER` / `OP_LESS` | — | pop 2, push | two numbers, or two strings (lexicographic) |
| `OP_ADD` | — | pop 2, push | numbers add, strings concatenate |
| `OP_SUBTRACT` / `OP_MULTIPLY` / `OP_DIVIDE` / `OP_MODULO` | — | pop 2, push | numbers only; `%` is `fmod` |
| `OP_NOT` | — | pop, push | truthiness: `nil`/`false` are falsey |
| `OP_NEGATE` | — | pop, push | number only |
| `OP_PRINT` | — | pop | writes value + newline to stdout |
| `OP_JUMP` | 16-bit offset | — | unconditional forward |
| `OP_JUMP_IF_FALSE` | 16-bit offset | peek | condition stays on the stack |
| `OP_LOOP` | 16-bit offset | — | backward jump; counts a back-edge for hotness |
| `OP_CALL` | arg count | replaces callee+args with result | records call-site feedback |
| `OP_CLOSURE` | fn pool index, then (isLocal, index) per upvalue | push | wraps a function with its captures |
| `OP_CLOSE_UPVALUE` | — | pop | hoists the captured top-of-stack local to the heap |
| `OP_RETURN` | — | pop result; unwind frame | closes the frame's open upvalues |
| `OP_CONSTANT_LONG` | 16-bit pool index | push | for pools past 256 entries |
| `OP_ARRAY` | element count | pop N, push array | elements were pushed left to right |
| `OP_INDEX_GET` | — | pop index + target, push element | arrays and strings |
| `OP_INDEX_SET` | — | pop value + index + array, push value | assignment is an expression |

`>=`, `<=`, and `!=` have no opcodes: the compiler emits `OP_LESS`+`OP_NOT`,
`OP_GREATER`+`OP_NOT`, and `OP_EQUAL`+`OP_NOT` respectively. `and`/`or` compile
to jump patterns over `OP_JUMP_IF_FALSE`/`OP_JUMP`, which is what gives them
short-circuit evaluation.
