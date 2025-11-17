#ifndef SIS_PARSER_H
#define SIS_PARSER_H

#include "Lexer.h"

namespace par {

  typedef struct Atom {
    lex::Token* token;

    virtual ~Atom() {
      delete token;
      token = nullptr;
    }
  } Atom;

  typedef struct Expression: Atom {
    Atom* left = nullptr;
    Atom* middle = nullptr;
    Atom* right = nullptr;

    ~Expression() {
      if (left != nullptr) delete left; left = nullptr;
      if (middle != nullptr) delete middle; middle = nullptr;
      if (right != nullptr) delete right; right = nullptr;
    }

  } Expr;

  class Parser {
    public:
      Parser() {}
      ~Parser() {
        lexer = nullptr;
      }

      void parse(lex::Lexer* lexer);

    private:
      lex::Lexer* lexer;
  };

}

#endif // SIS_PARSE_H
