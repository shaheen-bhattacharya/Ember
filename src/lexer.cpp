#include "lexer.h"

#include <cstring>

bool Lexer::match(char expected) {
  if (isAtEnd() || *current_ != expected) return false;
  current_++;
  return true;
}

void Lexer::skipWhitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        line_++;
        advance();
        break;
      case '/':
        if (peekNext() == '/') {
          while (peek() != '\n' && !isAtEnd()) advance();
        } else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

Token Lexer::makeToken(TokenType type) const {
  Token token;
  token.type = type;
  token.start = start_;
  token.length = static_cast<int>(current_ - start_);
  token.line = line_;
  return token;
}

Token Lexer::errorToken(const char* message) const {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = static_cast<int>(strlen(message));
  token.line = line_;
  return token;
}

Token Lexer::string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') line_++;
    advance();
  }
  if (isAtEnd()) return errorToken("Unterminated string.");
  advance();  // closing quote
  return makeToken(TOKEN_STRING);
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }
static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

Token Lexer::number() {
  // Underscores are digit separators: allowed only between two digits.
  while (isDigit(peek()) || (peek() == '_' && isDigit(peekNext()))) advance();
  if (peek() == '.' && isDigit(peekNext())) {
    advance();  // consume the '.'
    while (isDigit(peek()) || (peek() == '_' && isDigit(peekNext()))) {
      advance();
    }
  }
  return makeToken(TOKEN_NUMBER);
}

TokenType Lexer::identifierType() const {
  struct Keyword {
    const char* word;
    TokenType type;
  };
  static const Keyword keywords[] = {
      {"and", TOKEN_AND},       {"break", TOKEN_BREAK},
      {"continue", TOKEN_CONTINUE},
      {"else", TOKEN_ELSE},   {"false", TOKEN_FALSE},
      {"for", TOKEN_FOR},       {"fun", TOKEN_FUN},     {"if", TOKEN_IF},
      {"nil", TOKEN_NIL},       {"or", TOKEN_OR},       {"print", TOKEN_PRINT},
      {"return", TOKEN_RETURN}, {"true", TOKEN_TRUE},   {"var", TOKEN_VAR},
      {"while", TOKEN_WHILE},
  };
  int length = static_cast<int>(current_ - start_);
  for (const Keyword& kw : keywords) {
    int kwLen = static_cast<int>(strlen(kw.word));
    if (kwLen == length && memcmp(start_, kw.word, length) == 0) {
      return kw.type;
    }
  }
  return TOKEN_IDENTIFIER;
}

Token Lexer::identifier() {
  while (isAlpha(peek()) || isDigit(peek())) advance();
  return makeToken(identifierType());
}

Token Lexer::scanToken() {
  skipWhitespace();
  start_ = current_;
  if (isAtEnd()) return makeToken(TOKEN_EOF);

  char c = advance();
  if (isAlpha(c)) return identifier();
  if (isDigit(c)) return number();

  switch (c) {
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': return makeToken(TOKEN_RIGHT_BRACE);
    case ',': return makeToken(TOKEN_COMMA);
    case '-': return makeToken(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
    case '+': return makeToken(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case '/': return makeToken(TOKEN_SLASH);
    case '*': return makeToken(TOKEN_STAR);
    case '%': return makeToken(TOKEN_PERCENT);
    case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string();
  }
  return errorToken("Unexpected character.");
}
