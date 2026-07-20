#pragma once

// Runs the JIT's self-checks (executable memory round-trip, instruction
// encodings). Returns true on success, and true with a notice on platforms
// where the JIT is unsupported.
bool jitSelftest();
