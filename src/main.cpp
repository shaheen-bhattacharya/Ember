#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "compiler.h"
#include "debug.h"
#include "jit/jit.h"
#include "memory.h"
#include "object.h"
#include "version.h"
#include "vm.h"

static std::string readFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// True while `source` can't be complete yet: an unclosed (, {, or [, an
// unterminated string, or an open block comment. A quick hand scan (not the
// lexer) because half-typed input isn't tokenizable.
static bool needsMoreInput(const std::string& source) {
  int depth = 0;
  int comment = 0;  // block comments nest
  bool inString = false;
  for (size_t i = 0; i < source.size(); i++) {
    char c = source[i];
    if (inString) {
      if (c == '\\') i++;  // skip the escaped character
      else if (c == '"') inString = false;
    } else if (comment > 0) {
      if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
        comment++;
        i++;
      } else if (c == '*' && i + 1 < source.size() && source[i + 1] == '/') {
        comment--;
        i++;
      }
    } else if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
      while (i < source.size() && source[i] != '\n') i++;
    } else if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
      comment = 1;
      i++;
    } else if (c == '"') {
      inString = true;
    } else if (c == '(' || c == '{' || c == '[') {
      depth++;
    } else if (c == ')' || c == '}' || c == ']') {
      depth--;  // over-closing goes negative: let the compiler report it
    }
  }
  return depth > 0 || comment > 0 || inString;
}

static void repl(VM& vm) {
  std::string line;
  printf("ember " EMBER_VERSION " — type an expression to see its value. "
         "Ctrl-D to exit.\n");
  for (;;) {
    printf("> ");
    if (!std::getline(std::cin, line)) {
      printf("\n");
      break;
    }
    // Keep reading while delimiters are open, so blocks can be typed across
    // lines. EOF mid-buffer runs what's there, then exits on the next loop.
    std::string source = line;
    while (needsMoreInput(source)) {
      printf(". ");
      if (!std::getline(std::cin, line)) break;
      source += "\n" + line;
    }
    // If the input parses as a bare expression, echo its value; otherwise
    // run it as a statement. The probe compile is silent.
    std::string wrapped = "print (" + source + ");";
    if (compileSource(wrapped, /*quietErrors=*/true) != nullptr) {
      vm.interpret(wrapped);
    } else {
      vm.interpret(source);
    }
  }
}

static int runFile(VM& vm, const char* path) {
  std::string source = readFile(path);
  InterpretResult result = vm.interpret(source);
  if (result == InterpretResult::COMPILE_ERROR) return 65;
  if (result == InterpretResult::RUNTIME_ERROR) return 70;
  return 0;
}

static int dumpFile(const char* path) {
  std::string source = readFile(path);
  ObjFunction* function = compileSource(source);
  if (function == nullptr) return 65;
  disassembleChunk(function->chunk, "<script>");
  return 0;
}

static void printUsage(FILE* out) {
  fprintf(out,
          "Usage: ember [options] [path]\n"
          "\n"
          "  (no arguments)   start the REPL\n"
          "  path             run an Ember script\n"
          "  --dump path      disassemble the compiled bytecode, don't run\n"
          "  -e, --eval code  run code given on the command line\n"
          "  --version        print the version\n"
          "  --help           show this help\n");
}

int main(int argc, char* argv[]) {
  VM vm;
  if (argc == 1) {
    repl(vm);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--version") {
    printf("ember " EMBER_VERSION "\n");
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--help") {
    printUsage(stdout);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--jit-selftest") {
    return jitSelftest() ? 0 : 1;
  }
  if (argc == 2) {
    return runFile(vm, argv[1]);
  }
  if (argc == 3 && std::string(argv[1]) == "--dump") {
    return dumpFile(argv[2]);
  }
  if (argc == 3 &&
      (std::string(argv[1]) == "-e" || std::string(argv[1]) == "--eval")) {
    InterpretResult result = vm.interpret(argv[2]);
    if (result == InterpretResult::COMPILE_ERROR) return 65;
    if (result == InterpretResult::RUNTIME_ERROR) return 70;
    return 0;
  }
  printUsage(stderr);
  return 64;
}
