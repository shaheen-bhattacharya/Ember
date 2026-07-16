#pragma once

#include <string>

struct ObjFunction;

// Compiles Ember source to a top-level "script" function containing bytecode.
// Returns nullptr if there were any compile errors (reported to stderr).
ObjFunction* compileSource(const std::string& source);
