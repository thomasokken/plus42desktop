/*****************************************************************************
 * Plus42 -- an enhanced HP-42S calculator simulator
 * Copyright (C) 2004-2022  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#ifndef CORE_PARSER_H
#define CORE_PARSER_H 1


#include <map>
#include <string>
#include <vector>

#include "core_globals.h"
#include "core_variables.h"

////////////////////////////////
/////  class declarations  /////
////////////////////////////////

class GeneratorContext;
class For;

class Evaluator {

    protected:

    int tpos;

    public:

    Evaluator(int pos) : tpos(pos) {}
    virtual ~Evaluator() {}
    virtual bool isBool() { return false; }
    virtual bool makeLvalue() { return false; }
    virtual std::string name() { return ""; }
    virtual std::string eqnName() { return ""; }
    virtual std::vector<std::string> *eqnParamNames() { return NULL; }
    virtual std::string getText() { return ""; }
    virtual void getSides(const std::string &name, Evaluator **lhs, Evaluator **rhs);
    virtual Evaluator *clone(For *f) = 0;

    int pos() { return tpos; }

    virtual Evaluator *invert(const std::string &name, Evaluator *rhs);
    virtual void generateCode(GeneratorContext *ctx) = 0;
    virtual void generateAssignmentCode(GeneratorContext *ctx) {} /* For lvalues */
    virtual void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) = 0;
    virtual int howMany(const std::string &name) = 0;

    void addIfNew(const std::string &name, std::vector<std::string> *vars, std::vector<std::string> *locals);
};

class CodeMap {
    private:
    char *data;
    int size;
    int capacity;
    int4 current_pos;
    int4 current_line;

    void addByte(int b);
    void write(int4 n);
    int4 read(int *index);

    public:
    CodeMap() : data(NULL), size(0), capacity(0), current_pos(-1), current_line(0) {}
    CodeMap(char *data, int size) : data(data), size(size) {}
    ~CodeMap();
    void add(int4 pos, int4 line);
    int4 lookup(int4 line);
    char *getData() { return data; }
    int getSize() { return size; }
};

class Lexer;
struct prgm_struct;

class Parser {

    private:

    std::string text;
    Lexer *lex;
    std::string pb;
    int pbpos;
    int context;
    std::vector<For *> forStack;

    static Evaluator *parse2(std::string expr, bool *compatMode, int *errpos);

    public:

    static Evaluator *parse(std::string expr, bool *compatMode, int *errpos);
    static void generateCode(Evaluator *ev, prgm_struct *prgm, CodeMap *map);

    private:

    Parser(Lexer *lex);
    ~Parser();
    Evaluator *parseExpr(int context);
    Evaluator *parseExpr2();
    Evaluator *parseAnd();
    Evaluator *parseNot();
    Evaluator *parseComparison();
    Evaluator *parseNumExpr();
    Evaluator *parseTerm();
    Evaluator *parseFactor();
    Evaluator *parseThing();
    std::vector<Evaluator *> *parseExprList(int min_args, int max_args, int mode);
    bool isIdentifier(const std::string &s);
    bool nextToken(std::string *tok, int *tpos);
    void pushback(std::string o, int p);
    static bool isOperator(const std::string &s);
};

void get_varmenu_row_for_eqn(vartype *eqn, int need_eval, int *rows, int *row, char ktext[6][7], int klen[6]);
vartype *isolate(vartype *eqn, const char *name, int length);
bool has_parameters(equation_data *eqdata);
std::vector<std::string> get_parameters(equation_data *eqdata);
std::vector<std::string> get_mvars(const char *name, int namelen);
bool is_equation(vartype *v);
void num_parameters(vartype *v, int *black, int *total);

#endif
