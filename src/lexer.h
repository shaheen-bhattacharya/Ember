#pragma once

enum TokenType {
  // Single-character tokens.
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
  TOKEN_COMMA, TOKEN_MINUS, TOKEN_PLUS, TOKEN_SEMICOLON,
  TOKEN_SLASH, TOKEN_STAR, TOKEN_PERCENT,
  // One or two character tokens.
  TOKEN_BANG, TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
  TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL,
  // Literals.
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
  // Keywords.
  TOKEN_AND, TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR,
  TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR, TOKEN_PRINT, TOKEN_RETURN,
  TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

  TOKEN_ERROR, TOKEN_EOF,
};

struct Token {
  TokenType type = TOKEN_EOF;
  const char* start = nullptr;
  int length = 0;
  int line = 0;
};

class Lexer {
 public:
  explicit Lexer(const char* source) : start_(source), current_(source) {
    // Skip a leading shebang so scripts can be directly executable.
    if (current_[0] == '#' && current_[1] == '!') {
      while (*current_ != '\n' && *current_ != '\0') current_++;
    }
  }
  Token scanToken();

 private:
  const char* start_;
  const char* current_;
  int line_ = 1;

  bool isAtEnd() const { return *current_ == '\0'; }
  char advance() { return *current_++; }
  char peek() const { return *current_; }
  char peekNext() const { return isAtEnd() ? '\0' : current_[1]; }
  bool match(char expected);
  void skipWhitespace();
  Token makeToken(TokenType type) const;
  Token errorToken(const char* message) const;
  Token string();
  Token number();
  Token identifier();
  TokenType identifierType() const;
};
