// Single-pass Pratt-parser compiler: source -> bytecode, no AST materialized.
// This is the tier-0 front end; the SSA-based optimizing tier will get its own
// IR (see docs/DESIGN.md).
#include "compiler.h"

#include <algorithm>
#include <cmath>
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

// Innermost-enclosing-loop state for break/continue. `continueTarget` is the
// bytecode offset a continue jumps back to (condition, or increment for a
// for-loop); `scopeDepth` is the depth outside the loop body so jumps can
// discard body locals first.
struct LoopContext {
  LoopContext* enclosing = nullptr;
  int continueTarget = 0;
  int scopeDepth = 0;
  std::vector<int> breakJumps;
};

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
  explicit Compiler(const std::string& source, bool quietErrors)
      : source_(source), lexer_(source.c_str()), quietErrors_(quietErrors) {}

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
  const std::string& source_;
  Lexer lexer_;
  Token current_;
  Token previous_;
  bool hadError_ = false;
  bool panicMode_ = false;
  bool quietErrors_ = false;
  FunctionCompiler* current_fc_ = nullptr;
  LoopContext* currentLoop_ = nullptr;

  // Peephole state for constant folding: the last two number-constant loads
  // at the very tail of the current chunk. Any other emission invalidates.
  struct TailConst {
    int start = -1;  // byte offset of the load instruction, -1 = none
    int length = 0;
    double value = 0;
  };
  TailConst lastConst_;
  TailConst prevConst_;

  Chunk& currentChunk() { return current_fc_->function->chunk; }

  // ---- error handling ----

  void errorAt(const Token& token, const char* message) {
    if (panicMode_) return;
    panicMode_ = true;
    hadError_ = true;
    if (quietErrors_) return;
    fprintf(stderr, "[line %d] Error", token.line);
    if (token.type == TOKEN_EOF) {
      fprintf(stderr, " at end");
    } else if (token.type != TOKEN_ERROR) {
      fprintf(stderr, " at '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\n", message);
    showSourceLine(token);
    hadError_ = true;
  }

  // Prints the offending line with a caret under the token, when the token
  // points into the source (error tokens carry a message string instead).
  void showSourceLine(const Token& token) {
    const char* begin = source_.data();
    const char* end = begin + source_.size();
    const char* at = token.start;
    if (at == nullptr || at < begin || at > end) return;

    const char* lineStart = at;
    while (lineStart > begin && lineStart[-1] != '\n') lineStart--;
    const char* lineEnd = at;
    while (lineEnd < end && *lineEnd != '\n') lineEnd++;

    fprintf(stderr, "  %4d | %.*s\n", token.line,
            static_cast<int>(lineEnd - lineStart), lineStart);
    fprintf(stderr, "       | %*s^\n", static_cast<int>(at - lineStart), "");
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

  void emitByte(uint8_t byte) {
    lastConst_ = TailConst{};
    prevConst_ = TailConst{};
    currentChunk().write(byte, previous_.line);
  }

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

  int findOrAddConstant(const Value& value, int maxIndex) {
    // Reuse an existing pool entry: repeated literals and names would
    // otherwise exhaust the pool. Strings are interned, so valuesEqual's
    // pointer comparison deduplicates them too. Only hits within maxIndex
    // count: single-byte-operand opcodes can't reach further entries.
    Chunk& chunk = currentChunk();
    int limit = static_cast<int>(chunk.constants.size());
    for (int i = 0; i < limit && i <= maxIndex; i++) {
      if (valuesEqual(chunk.constants[i], value)) return i;
    }
    return chunk.addConstant(value);
  }

  // For opcodes with a one-byte constant operand (names, functions).
  uint8_t makeConstant(const Value& value) {
    int constant = findOrAddConstant(value, UINT8_MAX);
    if (constant > UINT8_MAX) {
      error("Too many constants in one chunk.");
      return 0;
    }
    return static_cast<uint8_t>(constant);
  }

  // Literals get the wide encoding when the pool grows past 256.
  void emitConstant(const Value& value) {
    TailConst shifted = lastConst_;  // emitByte clears tracking; restore after
    int start = static_cast<int>(currentChunk().code.size());
    int constant = findOrAddConstant(value, UINT16_MAX);
    if (constant > UINT16_MAX) {
      error("Too many constants in one chunk.");
      return;
    }
    if (constant <= UINT8_MAX) {
      emitBytes(OP_CONSTANT, static_cast<uint8_t>(constant));
    } else {
      emitByte(OP_CONSTANT_LONG);
      emitByte((constant >> 8) & 0xff);
      emitByte(constant & 0xff);
    }
    if (value.isNumber()) {
      prevConst_ = shifted;
      lastConst_.start = start;
      lastConst_.length = static_cast<int>(currentChunk().code.size()) - start;
      lastConst_.value = value.as.number;
    }
  }

  // If the chunk tail is exactly [CONST a][CONST b], replace it with the
  // folded result. Recorded jump targets can only point at the first
  // instruction's start, which stays an instruction boundary.
  bool tryFoldBinary(TokenType op) {
    int codeSize = static_cast<int>(currentChunk().code.size());
    if (lastConst_.start < 0 || prevConst_.start < 0) return false;
    if (lastConst_.start + lastConst_.length != codeSize) return false;
    if (prevConst_.start + prevConst_.length != lastConst_.start) return false;

    double a = prevConst_.value;
    double b = lastConst_.value;
    double result;
    switch (op) {
      case TOKEN_PLUS: result = a + b; break;
      case TOKEN_MINUS: result = a - b; break;
      case TOKEN_STAR: result = a * b; break;
      case TOKEN_SLASH: result = a / b; break;
      case TOKEN_PERCENT: result = fmod(a, b); break;
      default: return false;
    }
    truncateTo(prevConst_.start);
    emitConstant(Value::number(result));
    return true;
  }

  bool tryFoldNegate() {
    int codeSize = static_cast<int>(currentChunk().code.size());
    if (lastConst_.start < 0 ||
        lastConst_.start + lastConst_.length != codeSize) {
      return false;
    }
    double value = lastConst_.value;
    // Only the last constant is rewritten; the one before it (if adjacent)
    // is still valid, so keep it foldable: `4 * -2` folds all the way.
    TailConst keepPrev = prevConst_;
    truncateTo(lastConst_.start);
    emitConstant(Value::number(-value));
    if (keepPrev.start >= 0 &&
        keepPrev.start + keepPrev.length == lastConst_.start) {
      prevConst_ = keepPrev;
    }
    return true;
  }

  void truncateTo(int size) {
    Chunk& chunk = currentChunk();
    chunk.code.resize(size);
    chunk.lines.resize(size);
    lastConst_ = TailConst{};
    prevConst_ = TailConst{};
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
    } else if (canAssign && (check(TOKEN_PLUS_EQUAL) || check(TOKEN_MINUS_EQUAL) ||
                             check(TOKEN_STAR_EQUAL) || check(TOKEN_SLASH_EQUAL) ||
                             check(TOKEN_PERCENT_EQUAL))) {
      // Desugar `x op= e` to `x = x op e`.
      TokenType op = current_.type;
      advance();  // consume the compound operator
      emitBytes(getOp, static_cast<uint8_t>(arg));
      expression();
      switch (op) {
        case TOKEN_PLUS_EQUAL: emitByte(OP_ADD); break;
        case TOKEN_MINUS_EQUAL: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR_EQUAL: emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH_EQUAL: emitByte(OP_DIVIDE); break;
        default: emitByte(OP_MODULO); break;
      }
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
    // Strip digit-separator underscores before parsing.
    std::string text(previous_.start, previous_.length);
    text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
    double value = strtod(text.c_str(), nullptr);
    emitConstant(Value::number(value));
  }

  void stringLit(bool) {
    // Decode escape sequences between the quotes.
    std::string chars;
    const char* src = previous_.start + 1;
    int length = previous_.length - 2;
    for (int i = 0; i < length; i++) {
      char c = src[i];
      if (c == '\\' && i + 1 < length) {
        i++;
        switch (src[i]) {
          case 'n': chars += '\n'; break;
          case 't': chars += '\t'; break;
          case 'r': chars += '\r'; break;
          case '"': chars += '"'; break;
          case '\\': chars += '\\'; break;
          case '0': chars += '\0'; break;
          default:
            error("Unknown escape sequence in string.");
            return;
        }
      } else {
        chars += c;
      }
    }
    emitConstant(Value::object(copyString(chars)));
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
      case TOKEN_MINUS:
        if (!tryFoldNegate()) emitByte(OP_NEGATE);
        break;
      default: return;  // unreachable
    }
  }

  void binary(bool) {
    TokenType operatorType = previous_.type;
    const ParseRule& rule = getRule(operatorType);
    parsePrecedence(static_cast<Precedence>(rule.precedence + 1));

    if (tryFoldBinary(operatorType)) return;

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

  void arrayLiteral(bool) {
    int count = 0;
    if (!check(TOKEN_RIGHT_BRACKET)) {
      do {
        expression();
        if (count == 255) error("Can't have more than 255 array elements.");
        count++;
      } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after array elements.");
    emitBytes(OP_ARRAY, static_cast<uint8_t>(count));
  }

  void index(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");
    if (canAssign && match(TOKEN_EQUAL)) {
      expression();
      emitByte(OP_INDEX_SET);
    } else {
      emitByte(OP_INDEX_GET);
    }
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
      case TOKEN_LEFT_BRACKET: {
        static const ParseRule r = {&Compiler::arrayLiteral, &Compiler::index,
                                    PREC_CALL};
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
    // A loop in the enclosing function is not breakable from inside this one.
    LoopContext* enclosingLoop = currentLoop_;
    currentLoop_ = nullptr;

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

    currentLoop_ = enclosingLoop;
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

    LoopContext loop;
    loop.enclosing = currentLoop_;
    loop.continueTarget = loopStart;
    loop.scopeDepth = current_fc_->scopeDepth;
    currentLoop_ = &loop;
    statement();
    currentLoop_ = loop.enclosing;

    emitLoop(loopStart);
    patchJump(exitJump);
    emitByte(OP_POP);
    // Breaks land here: past the loop and past the exit path's condition pop.
    for (int jump : loop.breakJumps) patchJump(jump);
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

    LoopContext loop;
    loop.enclosing = currentLoop_;
    loop.continueTarget = loopStart;  // increment start when one exists
    loop.scopeDepth = current_fc_->scopeDepth;
    currentLoop_ = &loop;
    statement();
    currentLoop_ = loop.enclosing;

    emitLoop(loopStart);

    if (exitJump != -1) {
      patchJump(exitJump);
      emitByte(OP_POP);
    }
    for (int jump : loop.breakJumps) patchJump(jump);
    endScope();
  }

  // Emits pops for locals that live inside the current loop's body, without
  // changing compile-time state — the jump leaves the scopes, the compiler
  // doesn't.
  void discardLoopLocals() {
    for (int i = current_fc_->localCount - 1;
         i >= 0 && current_fc_->locals[i].depth > currentLoop_->scopeDepth;
         i--) {
      emitByte(current_fc_->locals[i].isCaptured ? OP_CLOSE_UPVALUE : OP_POP);
    }
  }

  void breakStatement() {
    if (currentLoop_ == nullptr) {
      error("Can't use 'break' outside of a loop.");
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");
    if (currentLoop_ == nullptr) return;
    discardLoopLocals();
    currentLoop_->breakJumps.push_back(emitJump(OP_JUMP));
  }

  void continueStatement() {
    if (currentLoop_ == nullptr) {
      error("Can't use 'continue' outside of a loop.");
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
    if (currentLoop_ == nullptr) return;
    discardLoopLocals();
    // Jump back to the condition (while) or the increment clause (for).
    emitLoop(currentLoop_->continueTarget);
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
    } else if (match(TOKEN_BREAK)) {
      breakStatement();
    } else if (match(TOKEN_CONTINUE)) {
      continueStatement();
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

ObjFunction* compileSource(const std::string& source, bool quietErrors) {
  Compiler compiler(source, quietErrors);
  return compiler.compile();
}
