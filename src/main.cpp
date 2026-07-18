#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
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

static void repl(VM& vm) {
  std::string line;
  printf("ember 0.1 — tier 0 (bytecode interpreter). Ctrl-D to exit.\n");
  for (;;) {
    printf("> ");
    if (!std::getline(std::cin, line)) {
      printf("\n");
      break;
    }
    vm.interpret(line);
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
    printf("ember 0.1.0\n");
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--help") {
    printUsage(stdout);
    return 0;
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
