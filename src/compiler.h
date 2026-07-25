#pragma once

#include <string>

struct ObjFunction;

// Compiles Ember source to a top-level "script" function containing bytecode.
// Returns nullptr if there were any compile errors (reported to stderr,
// unless quietErrors — used by the REPL to probe whether a line parses as an
// expression).
ObjFunction* compileSource(const std::string& source,
                           bool quietErrors = false);
