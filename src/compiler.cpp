// Single-pass Pratt-parser compiler: source -> bytecode, no AST materialized.
// This is the tier-0 front end; the SSA-based optimizing tier will get its own
// IR (see docs/DESIGN.md).
#include "compiler.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "chunk.h"
#include "lexer.h"
#include "memory.h"
#include "object.h"

namespace {

enum Precedence {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // ! -
  PREC_CALL,        // ()
  PREC_PRIMARY,
};

struct Local {
  Token name;
  int depth = 0;  // -1 while declared but not yet initialized
  bool isCaptured = false;
};

// Compile-time record of a captured variable: where the runtime closure finds
// it — a local slot of the enclosing function, or an upvalue of the enclosing
// closure (for captures across more than one function boundary).
struct CompilerUpvalue {
  uint8_t index = 0;
  bool isLocal = false;
};

enum FunctionType { TYPE_FUNCTION, TYPE_SCRIPT };

// Per-function compile state; nested function declarations push a new one.
struct FunctionCompiler {
  FunctionCompiler* enclosing = nullptr;
  ObjFunction* function = nullptr;
  FunctionType type = TYPE_SCRIPT;
  Local locals[UINT8_MAX + 1];
  int localCount = 0;
  int scopeDepth = 0;
  CompilerUpvalue upvalues[UINT8_MAX + 1];
};

class Compiler;
using ParseFn = void (Compiler::*)(bool canAssign);

struct ParseRule {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
};

class Compiler {
 public:
  explicit Compiler(const std::string& source) : lexer_(source.c_str()) {}

  ObjFunction* compile() {
    FunctionCompiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    advance();
    while (!match(TOKEN_EOF)) {
      declaration();
    }
    ObjFunction* function = endCompiler();
    return hadError_ ? nullptr : function;
  }

 private:
  Lexer lexer_;
  Token current_;
  Token previous_;
  bool hadError_ = false;
  bool panicMode_ = false;
  FunctionCompiler* current_fc_ = nullptr;

  Chunk& currentChunk() { return current_fc_->function->chunk; }

  // ---- error handling ----

  void errorAt(const Token& token, const char* message) {
    if (panicMode_) return;
    panicMode_ = true;
    fprintf(stderr, "[line %d] Error", token.line);
    if (token.type == TOKEN_EOF) {
      fprintf(stderr, " at end");
    } else if (token.type != TOKEN_ERROR) {
      fprintf(stderr, " at '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
    hadError_ = true;
  }

  void error(const char* message) { errorAt(previous_, message); }
  void errorAtCurrent(const char* message) { errorAt(current_, message); }

  // ---- token plumbing ----

  void advance() {
    previous_ = current_;
    for (;;) {
      current_ = lexer_.scanToken();
      if (current_.type != TOKEN_ERROR) break;
      errorAtCurrent(current_.start);
    }
  }

  void consume(TokenType type, const char* message) {
    if (current_.type == type) {
      advance();
      return;
    }
    errorAtCurrent(message);
  }

  bool check(TokenType type) const { return current_.type == type; }

  bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
  }

  // ---- bytecode emission ----

  void emitByte(uint8_t byte) { currentChunk().write(byte, previous_.line); }

  void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
  }

  void emitLoop(int loopStart) {
    emitByte(OP_LOOP);
    int offset = static_cast<int>(currentChunk().code.size()) - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
  }

  int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return static_cast<int>(currentChunk().code.size()) - 2;
  }

  void patchJump(int offset) {
    int jump = static_cast<int>(currentChunk().code.size()) - offset - 2;
    if (jump > UINT16_MAX) error("Too much code to jump over.");
    currentChunk().code[offset] = (jump >> 8) & 0xff;
    currentChunk().code[offset + 1] = jump & 0xff;
  }

  void emitReturn() {
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
  }

  uint8_t makeConstant(const Value& value) {
    // Reuse an existing pool entry: repeated literals and names would
    // otherwise exhaust the 256-slot pool. Strings are interned, so
    // valuesEqual's pointer comparison deduplicates them too.
    Chunk& chunk = currentChunk();
    for (int i = 0; i < static_cast<int>(chunk.constants.size()); i++) {
      if (valuesEqual(chunk.constants[i], value)) {
        return static_cast<uint8_t>(i);
      }
    }
    int constant = currentChunk().addConstant(value);
    if (constant > UINT8_MAX) {
      error("Too many constants in one chunk.");
      return 0;
    }
    return static_cast<uint8_t>(constant);
  }

  void emitConstant(const Value& value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
  }

  // ---- compiler state ----

  void initCompiler(FunctionCompiler* compiler, FunctionType type) {
    compiler->enclosing = current_fc_;
    compiler->type = type;
    compiler->function = gHeap.allocate<ObjFunction>();
    current_fc_ = compiler;
    if (type != TYPE_SCRIPT) {
      current_fc_->function->name =
          copyString(std::string(previous_.start, previous_.length));
    }
    // Slot 0 holds the function being called; reserve it.
    Local* local = &current_fc_->locals[current_fc_->localCount++];
    local->depth = 0;
    local->name = Token{};
  }

  ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current_fc_->function;
    // One feedback cell per code byte; only opcode offsets are ever written.
    function->feedback.assign(function->chunk.code.size(), 0);
    current_fc_ = current_fc_->enclosing;
    return function;
  }

  void beginScope() { current_fc_->scopeDepth++; }

  void endScope() {
    current_fc_->scopeDepth--;
    while (current_fc_->localCount > 0 &&
           current_fc_->locals[current_fc_->localCount - 1].depth >
               current_fc_->scopeDepth) {
      // A captured local must outlive its stack slot: hoist it to the heap.
      if (current_fc_->locals[current_fc_->localCount - 1].isCaptured) {
        emitByte(OP_CLOSE_UPVALUE);
      } else {
        emitByte(OP_POP);
      }
      current_fc_->localCount--;
    }
  }

  // ---- variables ----

  static bool identifiersEqual(const Token& a, const Token& b) {
    if (a.length != b.length) return false;
    return memcmp(a.start, b.start, a.length) == 0;
  }

  uint8_t identifierConstant(const Token& name) {
    return makeConstant(
        Value::object(copyString(std::string(name.start, name.length))));
  }

  int resolveLocal(FunctionCompiler* compiler, const Token& name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
      Local* local = &compiler->locals[i];
      if (identifiersEqual(name, local->name)) {
        if (local->depth == -1) {
          error("Can't read local variable in its own initializer.");
        }
        return i;
      }
    }
    return -1;
  }

  int addUpvalue(FunctionCompiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;
    for (int i = 0; i < upvalueCount; i++) {
      const CompilerUpvalue& upvalue = compiler->upvalues[i];
      if (upvalue.index == index && upvalue.isLocal == isLocal) return i;
    }
    if (upvalueCount == UINT8_MAX + 1) {
      error("Too many closure variables in function.");
      return 0;
    }
    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
  }

  // Finds `name` in an enclosing function, threading it through every
  // intermediate closure so multi-level captures resolve at compile time.
  int resolveUpvalue(FunctionCompiler* compiler, const Token& name) {
    if (compiler->enclosing == nullptr) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
      compiler->enclosing->locals[local].isCaptured = true;
      return addUpvalue(compiler, static_cast<uint8_t>(local), true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
      return addUpvalue(compiler, static_cast<uint8_t>(upvalue), false);
    }
    return -1;
  }

  void addLocal(const Token& name) {
    if (current_fc_->localCount == UINT8_MAX + 1) {
      error("Too many local variables in function.");
      return;
    }
    Local* local = &current_fc_->locals[current_fc_->localCount++];
    local->name = name;
    local->depth = -1;
  }

  void declareVariable() {
    if (current_fc_->scopeDepth == 0) return;  // globals are late-bound
    const Token& name = previous_;
    for (int i = current_fc_->localCount - 1; i >= 0; i--) {
      Local* local = &current_fc_->locals[i];
      if (local->depth != -1 && local->depth < current_fc_->scopeDepth) break;
      if (identifiersEqual(name, local->name)) {
        error("Already a variable with this name in this scope.");
      }
    }
    addLocal(name);
  }

  uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    declareVariable();
    if (current_fc_->scopeDepth > 0) return 0;
    return identifierConstant(previous_);
  }

  void markInitialized() {
    if (current_fc_->scopeDepth == 0) return;
    current_fc_->locals[current_fc_->localCount - 1].depth =
        current_fc_->scopeDepth;
  }

  void defineVariable(uint8_t global) {
    if (current_fc_->scopeDepth > 0) {
      markInitialized();
      return;  // local lives in its stack slot; nothing to emit
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
  }

  void namedVariable(const Token& name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current_fc_, name);
    if (arg != -1) {
      getOp = OP_GET_LOCAL;
      setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current_fc_, name)) != -1) {
      getOp = OP_GET_UPVALUE;
      setOp = OP_SET_UPVALUE;
    } else {
      arg = identifierConstant(name);
      getOp = OP_GET_GLOBAL;
      setOp = OP_SET_GLOBAL;
    }
    if (canAssign && match(TOKEN_EQUAL)) {
      expression();
      emitBytes(setOp, static_cast<uint8_t>(arg));
    } else if (canAssign && (check(TOKEN_PLUS_EQUAL) || check(TOKEN_MINUS_EQUAL))) {
      // Desugar `x op= e` to `x = x op e`.
      bool isAdd = check(TOKEN_PLUS_EQUAL);
      advance();  // consume the compound operator
      emitBytes(getOp, static_cast<uint8_t>(arg));
      expression();
      emitByte(isAdd ? OP_ADD : OP_SUBTRACT);
      emitBytes(setOp, static_cast<uint8_t>(arg));
    } else {
      emitBytes(getOp, static_cast<uint8_t>(arg));
    }
  }

  // ---- expression parselets (Pratt) ----

  void expression() { parsePrecedence(PREC_ASSIGNMENT); }

  void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(previous_.type).prefix;
    if (prefixRule == nullptr) {
      error("Expect expression.");
      return;
    }
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    (this->*prefixRule)(canAssign);

    while (precedence <= getRule(current_.type).precedence) {
      advance();
      ParseFn infixRule = getRule(previous_.type).infix;
      (this->*infixRule)(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
      error("Invalid assignment target.");
    }
  }

  void grouping(bool) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  }

  void numberLit(bool) {
    double value = strtod(previous_.start, nullptr);
    emitConstant(Value::number(value));
  }

  void stringLit(bool) {
    emitConstant(Value::object(
        copyString(std::string(previous_.start + 1, previous_.length - 2))));
  }

  void literal(bool) {
    switch (previous_.type) {
      case TOKEN_FALSE: emitByte(OP_FALSE); break;
      case TOKEN_NIL: emitByte(OP_NIL); break;
      case TOKEN_TRUE: emitByte(OP_TRUE); break;
      default: return;  // unreachable
    }
  }

  void variable(bool canAssign) { namedVariable(previous_, canAssign); }

  void unary(bool) {
    TokenType operatorType = previous_.type;
    parsePrecedence(PREC_UNARY);
    switch (operatorType) {
      case TOKEN_BANG: emitByte(OP_NOT); break;
      case TOKEN_MINUS: emitByte(OP_NEGATE); break;
      default: return;  // unreachable
    }
  }

  void binary(bool) {
    TokenType operatorType = previous_.type;
    const ParseRule& rule = getRule(operatorType);
    parsePrecedence(static_cast<Precedence>(rule.precedence + 1));

    switch (operatorType) {
      case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
      case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
      case TOKEN_GREATER: emitByte(OP_GREATER); break;
      case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
      case TOKEN_LESS: emitByte(OP_LESS); break;
      case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
      case TOKEN_PLUS: emitByte(OP_ADD); break;
      case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
      case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
      case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
      case TOKEN_PERCENT: emitByte(OP_MODULO); break;
      default: return;  // unreachable
    }
  }

  void and_(bool) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
  }

  void or_(bool) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
  }

  uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
      do {
        expression();
        if (argCount == 255) error("Can't have more than 255 arguments.");
        argCount++;
      } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
  }

  void call(bool) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
  }

  const ParseRule& getRule(TokenType type) {
    static const ParseRule none = {nullptr, nullptr, PREC_NONE};
    switch (type) {
      case TOKEN_LEFT_PAREN: {
        static const ParseRule r = {&Compiler::grouping, &Compiler::call, PREC_CALL};
        return r;
      }
      case TOKEN_MINUS: {
        static const ParseRule r = {&Compiler::unary, &Compiler::binary, PREC_TERM};
        return r;
      }
      case TOKEN_PLUS: {
        static const ParseRule r = {nullptr, &Compiler::binary, PREC_TERM};
        return r;
      }
      case TOKEN_SLASH:
      case TOKEN_STAR:
      case TOKEN_PERCENT: {
        static const ParseRule r = {nullptr, &Compiler::binary, PREC_FACTOR};
        return r;
      }
      case TOKEN_BANG: {
        static const ParseRule r = {&Compiler::unary, nullptr, PREC_NONE};
        return r;
      }
      case TOKEN_BANG_EQUAL:
      case TOKEN_EQUAL_EQUAL: {
        static const ParseRule r = {nullptr, &Compiler::binary, PREC_EQUALITY};
        return r;
      }
      case TOKEN_GREATER:
      case TOKEN_GREATER_EQUAL:
      case TOKEN_LESS:
      case TOKEN_LESS_EQUAL: {
        static const ParseRule r = {nullptr, &Compiler::binary, PREC_COMPARISON};
        return r;
      }
      case TOKEN_IDENTIFIER: {
        static const ParseRule r = {&Compiler::variable, nullptr, PREC_NONE};
        return r;
      }
      case TOKEN_STRING: {
        static const ParseRule r = {&Compiler::stringLit, nullptr, PREC_NONE};
        return r;
      }
      case TOKEN_NUMBER: {
        static const ParseRule r = {&Compiler::numberLit, nullptr, PREC_NONE};
        return r;
      }
      case TOKEN_AND: {
        static const ParseRule r = {nullptr, &Compiler::and_, PREC_AND};
        return r;
      }
      case TOKEN_OR: {
        static const ParseRule r = {nullptr, &Compiler::or_, PREC_OR};
        return r;
      }
      case TOKEN_FALSE:
      case TOKEN_NIL:
      case TOKEN_TRUE: {
        static const ParseRule r = {&Compiler::literal, nullptr, PREC_NONE};
        return r;
      }
      default:
        return none;
    }
  }

  // ---- statements ----

  void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
      declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
  }

  void functionBody(FunctionType type) {
    FunctionCompiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
      do {
        current_fc_->function->arity++;
        if (current_fc_->function->arity > 255) {
          errorAtCurrent("Can't have more than 255 parameters.");
        }
        uint8_t constant = parseVariable("Expect parameter name.");
        defineVariable(constant);
      } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(Value::object(function)));
    for (int i = 0; i < function->upvalueCount; i++) {
      emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
      emitByte(compiler.upvalues[i].index);
    }
  }

  void funDeclaration() {
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();  // allow recursion: name is usable inside the body
    functionBody(TYPE_FUNCTION);
    defineVariable(global);
  }

  void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");
    if (match(TOKEN_EQUAL)) {
      expression();
    } else {
      emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global);
  }

  void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
  }

  void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);
    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
  }

  void whileStatement() {
    int loopStart = static_cast<int>(currentChunk().code.size());
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);
    patchJump(exitJump);
    emitByte(OP_POP);
  }

  void forStatement() {
    // Desugars to initializer + while loop with increment appended.
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
      // no initializer
    } else if (match(TOKEN_VAR)) {
      varDeclaration();
    } else {
      expressionStatement();
    }

    int loopStart = static_cast<int>(currentChunk().code.size());
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
      expression();
      consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
      exitJump = emitJump(OP_JUMP_IF_FALSE);
      emitByte(OP_POP);
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
      int bodyJump = emitJump(OP_JUMP);
      int incrementStart = static_cast<int>(currentChunk().code.size());
      expression();
      emitByte(OP_POP);
      consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
      emitLoop(loopStart);
      loopStart = incrementStart;
      patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1) {
      patchJump(exitJump);
      emitByte(OP_POP);
    }
    endScope();
  }

  void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
  }

  void returnStatement() {
    if (current_fc_->type == TYPE_SCRIPT) {
      error("Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON)) {
      emitReturn();
    } else {
      expression();
      consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
      emitByte(OP_RETURN);
    }
  }

  void synchronize() {
    panicMode_ = false;
    while (current_.type != TOKEN_EOF) {
      if (previous_.type == TOKEN_SEMICOLON) return;
      switch (current_.type) {
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
          return;
        default:
          break;
      }
      advance();
    }
  }

  void declaration() {
    if (match(TOKEN_FUN)) {
      funDeclaration();
    } else if (match(TOKEN_VAR)) {
      varDeclaration();
    } else {
      statement();
    }
    if (panicMode_) synchronize();
  }

  void statement() {
    if (match(TOKEN_PRINT)) {
      printStatement();
    } else if (match(TOKEN_IF)) {
      ifStatement();
    } else if (match(TOKEN_RETURN)) {
      returnStatement();
    } else if (match(TOKEN_WHILE)) {
      whileStatement();
    } else if (match(TOKEN_FOR)) {
      forStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
      beginScope();
      block();
      endScope();
    } else {
      expressionStatement();
    }
  }
};

}  // namespace

ObjFunction* compileSource(const std::string& source) {
  Compiler compiler(source);
  return compiler.compile();
}
