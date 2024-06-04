#include <tree_sitter/parser.h>

enum TokenType {
  BLOCK_COMMENT_TEXT,
  DOC_COMMENT_TEXT,
  REAL_LITERAL,
};

void *tree_sitter_c3_external_scanner_create() { return NULL; }
void tree_sitter_c3_external_scanner_destroy(void *p) {}
void tree_sitter_c3_external_scanner_reset(void *p) {}
unsigned tree_sitter_c3_external_scanner_serialize(void *p, char *buffer) { return 0; }
void tree_sitter_c3_external_scanner_deserialize(void *p, const char *b, unsigned n) {}

static bool scan_block_comment(TSLexer *lexer, bool allow_eof) {
  for (int stack = 0;;) {
    if (lexer->eof(lexer)) {
      lexer->mark_end(lexer);
      return allow_eof;
    }

    int32_t c = lexer->lookahead;

    if (c == '/') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '*') {
        lexer->advance(lexer, false);
        stack += 1;
      }
    } else if (c == '*') {
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);
      if (lexer->lookahead == '/') {
        lexer->advance(lexer, false);
        stack -= 1;
        if (stack == -1) {
          return true;
        }
      }
    } else {
      lexer->advance(lexer, false);
    }
  }
  return false;
}

static bool is_whitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static bool is_digit(int32_t c) {
  return c >= '0' && c <= '9';
}

static bool is_hex_digit(int32_t c) {
  return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || is_digit(c);
}

static bool scan_realtype(TSLexer *lexer) {
  if (lexer->lookahead != 'f') {
    return false;
  }

  lexer->advance(lexer, false);
  lexer->mark_end(lexer);

  int32_t c1 = lexer->lookahead;
  lexer->advance(lexer, false);

  if (c1 == '8') {
    lexer->mark_end(lexer);
    return true;
  }

  int32_t c2 = lexer->lookahead;
  lexer->advance(lexer, false);

  if ((c1 == '1' && c2 == '6') || (c1 == '3' && c2 == '2') || (c1 == '6' && c2 == '4')) {
    lexer->mark_end(lexer);
    return true;
  }

  int32_t c3 = lexer->lookahead;
  lexer->advance(lexer, false);

  if (c1 == '1' && c2 == '2' && c3 == '8') {
    lexer->mark_end(lexer);
    return true;
  }

  return true;
}

static bool scan_digits(TSLexer *lexer) {
  if (!is_digit(lexer->lookahead)) {
    return false;
  }

  lexer->advance(lexer, false);
  while (is_digit(lexer->lookahead)) {
    lexer->advance(lexer, false);
  }
  return true;
}

static bool scan_int(TSLexer *lexer) {
  if (!is_digit(lexer->lookahead)) {
    return false;
  }

  lexer->advance(lexer, false);
  while (is_digit(lexer->lookahead) || lexer->lookahead == '_') {
    lexer->advance(lexer, false);
  }
  return true;
}

static bool scan_hexint(TSLexer *lexer) {
  if (!is_hex_digit(lexer->lookahead)) {
    return false;
  }

  lexer->advance(lexer, false);
  while (is_hex_digit(lexer->lookahead) || lexer->lookahead == '_') {
    lexer->advance(lexer, false);
  }
  return true;
}

static bool scan_real_literal(TSLexer *lexer) {
  int32_t c = lexer->lookahead;
  if (!is_digit(c)) {
    return false;
  }
  lexer->advance(lexer, false);

  bool is_hex = false;
  if (c == '0') {
    if (lexer->lookahead == 'x' || lexer->lookahead == 'X') {
      lexer->advance(lexer, false);
      is_hex = true;
    }
  }

  if (is_hex) {
    bool has_fraction = false;
    bool has_precision = false;

    if (!scan_hexint(lexer)) {
      return false;
    }

    if (lexer->lookahead == '.') {
      has_fraction = true;
      lexer->advance(lexer, false);

      if (lexer->lookahead == '.') {
        return false;
      }

      scan_hexint(lexer);
    }

    if (lexer->lookahead == 'p' || lexer->lookahead == 'P') {
      has_precision = true;
      lexer->advance(lexer, false);

      if (lexer->lookahead == '+' || lexer->lookahead == '-') {
        lexer->advance(lexer, false);
      }

      if (!scan_digits(lexer)) {
        return false;
      }
    }

    if (!has_fraction && !has_precision) {
      return false;
    }

  } else {
    while (is_digit(lexer->lookahead) || lexer->lookahead == '_') {
      lexer->advance(lexer, false);
    }

    if (scan_realtype(lexer)) {
      return true;
    }

    bool has_fraction = false;
    bool has_exponent = false;

    if (lexer->lookahead == '.') {
      has_fraction = true;
      lexer->advance(lexer, false);

      if (lexer->lookahead == '.') {
        return false;
      }

      scan_int(lexer);
    }

    if (lexer->lookahead == 'e' || lexer->lookahead == 'E') {
      has_exponent = true;
      lexer->advance(lexer, false);

      if (lexer->lookahead == '+' || lexer->lookahead == '-') {
        lexer->advance(lexer, false);
      }

      if (!scan_digits(lexer)) {
        return false;
      }
    }

    if (!has_fraction && !has_exponent) {
      return false;
    }
  }

  scan_realtype(lexer);
  return true;
}

bool tree_sitter_c3_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  // Allow block comments ending at EOF, but not doc comments.
  if (valid_symbols[BLOCK_COMMENT_TEXT] && scan_block_comment(lexer, true)) {
    lexer->result_symbol = BLOCK_COMMENT_TEXT;
    return true;
  }
  if (valid_symbols[DOC_COMMENT_TEXT] && scan_block_comment(lexer, false)) {
    lexer->result_symbol = DOC_COMMENT_TEXT;
    return true;
  }

  while (is_whitespace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }

  if (valid_symbols[REAL_LITERAL] && scan_real_literal(lexer)) {
    lexer->result_symbol = REAL_LITERAL;
    return true;
  }

  return false;
}
