/*****************************************************************************
 * Plus42 -- an enhanced HP-42S calculator simulator
 * Copyright (C) 2004-2025  Thomas Okken
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <sstream>

#include "core_helpers.h"
#include "core_parser.h"
#include "core_tables.h"
#include "core_variables.h"

//////////////////////////////
/////  GeneratorContext  /////
//////////////////////////////

struct Line {
    int pos;
    int cmd;
    arg_struct arg;
    char *buf;
    Line(int pos, int cmd) : pos(pos), cmd(cmd), buf(NULL) {
        arg.type = ARGTYPE_NONE;
    }
    Line(int pos, phloat d) : pos(pos), cmd(CMD_NUMBER), buf(NULL) {
        arg.type = ARGTYPE_DOUBLE;
        arg.val_d = d;
    }
    Line(int pos, int cmd, char s, bool ind) : pos(pos), cmd(cmd), buf(NULL) {
        arg.type = ind ? ARGTYPE_IND_STK : ARGTYPE_STK;
        arg.val.stk = s;
    }
    Line(int pos, int cmd, int n, bool ind) : pos(pos), cmd(cmd), buf(NULL) {
        arg.type = ind ? ARGTYPE_IND_NUM : ARGTYPE_NUM;
        arg.val.num = n;
    }
    Line(int pos, int cmd, std::string s, bool ind) : pos(pos), cmd(cmd), buf(NULL) {
        if (cmd == CMD_XSTR) {
            int len = (int) s.length();
            if (len > 65535)
                len = 65535;
            buf = (char *) malloc(len);
            memcpy(buf, s.c_str(), len);
            arg.type = ARGTYPE_XSTR;
            arg.val.xstr = buf;
            arg.length = len;
        } else {
            int len = (int) s.length();
            if (len > 7)
                len = 7;
            arg.type = ind ? ARGTYPE_IND_STR : ARGTYPE_STR;
            memcpy(arg.val.text, s.c_str(), len);
            arg.length = len;
        }
    }
    ~Line() {
        free(buf);
    }
};

CodeMap::~CodeMap() {
    free(data);
}

void CodeMap::addByte(int b) {
    if (size == -1)
        return;
    if (size + 1 > capacity) {
        int newcapacity = capacity + 64;
        char *newdata = (char *) realloc(data, newcapacity);
        if (newdata == NULL) {
            free(data);
            data = NULL;
            size = -1;
            return;
        }
        data = newdata;
        capacity = newcapacity;
    }
    data[size++] = b;
}

void CodeMap::write(int4 n) {
    uint4 u = n;
    while (true) {
        if (u <= 127) {
            addByte(u);
            return;
        }
        addByte((u & 127) | 128);
        u >>= 7;
    }
}

int4 CodeMap::read(int *index) {
    if (*index >= size)
        return -2;
    uint4 u = 0;
    int offset = 0;
    int b;
    do {
        b = data[(*index)++];
        u |= (b & 127) << offset;
        offset += 7;
    } while ((b & 128) != 0);
    return u;
}

void CodeMap::add(int4 pos, int4 line) {
    if (pos != current_pos) {
        if (line > current_line) {
            write(current_pos);
            write(line - current_line);
        }
        current_pos = pos;
        current_line = line;
    }
}

int4 CodeMap::lookup(int4 line) {
    int index = 0;
    int4 cline = 0;
    while (true) {
        int4 pos = read(&index);
        if (pos == -2)
            return -1;
        cline += read(&index);
        if (line < cline)
            return pos;
    }
}

class GeneratorContext {
    private:

    std::vector<Line *> *lines;
    std::vector<std::vector<Line *> *> stack;
    std::vector<std::vector<Line *> *> queue;
    int lbl;
    int assertTwoRealsLbl;

    public:

    GeneratorContext() {
        lines = new std::vector<Line *>;
        lbl = 0;
        assertTwoRealsLbl = -1;
        addLine(0, CMD_FSTART);
    }

    ~GeneratorContext() {
        for (int i = 0; i < lines->size(); i++)
            delete (*lines)[i];
        delete lines;
    }

    void addLine(int pos, int cmd) {
        lines->push_back(new Line(pos, cmd));
    }

    void addLine(int pos, phloat d) {
        lines->push_back(new Line(pos, d));
    }

    void addLine(int pos, int cmd, char s, bool ind = false) {
        lines->push_back(new Line(pos, cmd, s, ind));
    }

    void addLine(int pos, int cmd, int n, bool ind = false) {
        lines->push_back(new Line(pos, cmd, n, ind));
    }

    void addLine(int pos, int cmd, std::string s, bool ind = false) {
        lines->push_back(new Line(pos, cmd, s, ind));
    }

    int nextLabel() {
        return ++lbl;
    }

    void pushSubroutine() {
        stack.push_back(lines);
        lines = new std::vector<Line *>;
    }

    void popSubroutine() {
        queue.push_back(lines);
        lines = stack.back();
        stack.pop_back();
    }

    void addAssertTwoReals(int pos) {
        if (assertTwoRealsLbl == -1) {
            assertTwoRealsLbl = nextLabel();
            int lbl1 = nextLabel();
            int lbl2 = nextLabel();
            pushSubroutine();
            addLine(-1, CMD_LBL, assertTwoRealsLbl);
            addLine(-1, CMD_REAL_T);
            addLine(-1, CMD_GTOL, lbl1);
            addLine(-1, CMD_RTNERR, 4);
            addLine(-1, CMD_LBL, lbl1);
            addLine(-1, CMD_SWAP);
            addLine(-1, CMD_REAL_T);
            addLine(-1, CMD_GTOL, lbl2);
            addLine(-1, CMD_SWAP);
            addLine(-1, CMD_RTNERR, 4);
            addLine(-1, CMD_LBL, lbl2);
            addLine(-1, CMD_SWAP);
            popSubroutine();
        }
        addLine(pos, CMD_XEQL, assertTwoRealsLbl);
    }

    void store(prgm_struct *prgm, CodeMap *map) {
        prgm->lclbl_invalid = false;
        // Tack all the subroutines onto the main code
        for (int i = 0; i < queue.size(); i++) {
            addLine(-1, CMD_RTN);
            std::vector<Line *> *l = queue[i];
            lines->insert(lines->end(), l->begin(), l->end());
            delete l;
        }
        queue.clear();
        // First, resolve labels
        std::map<int, int> label2line;
        int lineno = 1;
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_LBL)
                label2line[line->arg.val.num] = lineno;
            else if (line->cmd == CMD_N_PLUS_U)
                lineno--;
            else
                lineno++;
        }
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_GTOL || line->cmd == CMD_XEQL)
                line->arg.val.num = label2line[line->arg.val.num];
        }
        // Label resolution done
        pgm_index saved_prgm = current_prgm;
        current_prgm.set(eq_dir->id, prgm->eq_data->eqn_index);
        prgm->text = NULL;
        prgm->size = 0;
        prgm->capacity = 0;
        // Temporarily turn off PRGM mode. This is because
        // store_command() usually refuses to insert commands
        // in programs above prgms_count, in order to prevent
        // users from editing generated code.
        char saved_prgm_mode = flags.f.prgm_mode;
        flags.f.prgm_mode = false;
        bool prev_printer_exists = flags.f.printer_exists;
        flags.f.printer_exists = false;
        bool prev_loading_state = loading_state;
        loading_state = true;
        // First, the end. Doing this before anything else prevents the program count from being bumped.
        arg_struct arg;
        arg.type = ARGTYPE_NONE;
        store_command(0, CMD_END, &arg, NULL);
        // Then, the rest...
        int4 pc = -1;
        lineno = 0;
        int skipcount = 0;
        for (int i = 0; i < lines->size(); i++) {
            Line *line = (*lines)[i];
            if (line->cmd == CMD_LBL)
                continue;
            store_command_after(&pc, line->cmd, &line->arg, NULL);
            if (skipcount == 0) {
                lineno++;
                if (map != NULL)
                    map->add(line->pos, lineno);
            } else {
                skipcount--;
            }
            if (line->cmd == CMD_N_PLUS_U)
                skipcount = 2;
        }
        if (map != NULL) {
            // Make END map to start of eqn
            map->add(0, ++lineno);
            // Sentinel. Should be redundant.
            map->add(-2, ((uint4) -1) >> 1);
        }
        current_prgm = saved_prgm;
        flags.f.prgm_mode = saved_prgm_mode;
        flags.f.printer_exists = prev_printer_exists;
        loading_state = prev_loading_state;
    }
};

//////////////////////////////////////////////
/////  Boilerplate Evaluator subclasses  /////
//////////////////////////////////////////////

class UnaryEvaluator : public Evaluator {

    protected:

    Evaluator *ev;
    bool invertible;

    public:

    UnaryEvaluator(int pos, Evaluator *ev, bool invertible) : Evaluator(pos), ev(ev), invertible(invertible) {}

    ~UnaryEvaluator() {
        delete ev;
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        int n = ev->howMany(name);
        return n == 0 ? 0 : invertible ? n : -1;
    }
};

class UnaryFunction : public UnaryEvaluator {

    private:

    int cmd;

    public:

    UnaryFunction(int pos, Evaluator *ev, int cmd) : UnaryEvaluator(pos, ev, false), cmd(cmd) {}

    Evaluator *clone(For *f) {
        return new UnaryFunction(tpos, ev->clone(f), cmd);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(tpos, cmd);
    }
};

class InvertibleUnaryFunction : public UnaryEvaluator {

    private:

    int cmd, inv_cmd;

    public:

    InvertibleUnaryFunction(int pos, Evaluator *ev, int cmd, int inv_cmd) : UnaryEvaluator(pos, ev, true), cmd(cmd), inv_cmd(inv_cmd) {}

    Evaluator *clone(For *f) {
        return new InvertibleUnaryFunction(tpos, ev->clone(f), cmd, inv_cmd);
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs) {
        return ev->invert(name, new InvertibleUnaryFunction(0, rhs, inv_cmd, cmd));
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(tpos, cmd);
    }
};

class BinaryEvaluator : public Evaluator {

    protected:

    Evaluator *left, *right;
    bool invertible;
    bool swapArgs;

    public:

    BinaryEvaluator(int pos, Evaluator *left, Evaluator *right, bool invertible)
        : Evaluator(pos), left(left), right(right), invertible(invertible), swapArgs(false) {}
    BinaryEvaluator(int pos, Evaluator *left, Evaluator *right, bool invertible, bool swapArgs)
        : Evaluator(pos), left(left), right(right), invertible(invertible), swapArgs(swapArgs) {}

    ~BinaryEvaluator() {
        delete left;
        delete right;
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        left->collectVariables(vars, locals);
        if (right != NULL)
            right->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        int a = left->howMany(name);
        if (a == -1)
            return -1;
        int b = right == NULL ? 0 : right->howMany(name);
        if (b == -1)
            return -1;
        int c = a + b;
        return c == 0 ? 0 : invertible ? c : -1;
    }
};

class BinaryFunction : public BinaryEvaluator {

    private:

    int cmd;

    public:

    BinaryFunction(int pos, Evaluator *left, Evaluator *right, int cmd) : BinaryEvaluator(pos, left, right, false), cmd(cmd) {}

    Evaluator *clone(For *f) {
        return new BinaryFunction(tpos, left->clone(f), right->clone(f), cmd);
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, cmd);
    }
};

class Subexpression : public Evaluator {

    private:

    Evaluator *ev;
    std::string text;

    public:

    Subexpression(int pos, Evaluator *ev, std::string text) : Evaluator(pos), ev(ev), text(text) {}

    ~Subexpression() {
        delete ev;
    }

    Evaluator *clone(For *) {
        return new Subexpression(tpos, ev->clone(NULL), text);
    }

    std::string getText() {
        return text;
    }

    void generateCode(GeneratorContext *ctx) {
        // this is handled by Integ
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        return ev->howMany(name) == 0 ? 0 : -1;
    }
};

class Subroutine : public Evaluator {

    private:

    Evaluator *ev;
    int lbl;
    Subroutine *primary;

    public:

    Subroutine(Evaluator *ev) : Evaluator(-1), ev(ev), lbl(-1), primary(NULL) {}
    Subroutine(Subroutine *primary) : Evaluator(-1), primary(primary) {}

    ~Subroutine() {
        if (primary == NULL)
            delete ev;
    }

    Evaluator *clone(For *f) {
        // only instantiated by isolate()
        return NULL;
    }

    void generateCode(GeneratorContext *ctx) {
        Subroutine *primary = this->primary == NULL ? this : this->primary;
        if (primary->lbl == -1) {
            primary->lbl = ctx->nextLabel();
            ctx->pushSubroutine();
            ctx->addLine(tpos, CMD_LBL, primary->lbl);
            primary->ev->generateCode(ctx);
            ctx->popSubroutine();
        }
        ctx->addLine(tpos, CMD_XEQL, primary->lbl);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // only instantiated by isolate()
    }

    int howMany(const std::string &name) {
        // only instantiated by isolate()
        return 0;
    }
};

class RecallFunction : public Evaluator {

    private:

    int cmd;

    public:

    RecallFunction(int pos, int cmd) : Evaluator(pos), cmd(cmd) {}

    Evaluator *clone(For *) {
        return new RecallFunction(tpos, cmd);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, cmd);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return 0;
    }
};

class RecallOneOfTwoFunction : public Evaluator {

    private:

    int cmd;
    bool pick_x;

    public:

    RecallOneOfTwoFunction(int pos, int cmd, bool pick_x) : Evaluator(pos), cmd(cmd), pick_x(pick_x) {}

    Evaluator *clone(For *) {
        return new RecallOneOfTwoFunction(tpos, cmd, pick_x);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, cmd);
        if (pick_x)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_DROP);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return 0;
    }
};


///////////////////
/////  Abort  /////
///////////////////

class Abort : public Evaluator {

    public:

    Abort(int pos) : Evaluator(pos) {}

    Evaluator *clone(For *f) {
        return new Abort(tpos);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_RAISE, ERR_INVALID_DATA);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return -1;
    }
};

/////////////////
/////  And  /////
/////////////////

class And : public BinaryEvaluator {

    public:

    And(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new And(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_AND);
    }
};

///////////////////
/////  Angle  /////
///////////////////

class Angle : public BinaryEvaluator {

    public:

    Angle(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Angle(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(tpos, CMD_CPX_T);
            ctx->addLine(tpos, CMD_GTOL, lbl1);
            ctx->addLine(tpos, (phloat) 0);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_TO_POL);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, CMD_GTOL, lbl2);
            ctx->addLine(tpos, CMD_LBL, lbl1);
            ctx->addLine(tpos, CMD_PCOMPLX);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_TO_POL);
            ctx->addLine(tpos, CMD_DROP);
        }
    }
};

////////////////////
/////  Append  /////
////////////////////

class Append : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Append(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}

    ~Append() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Append(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        (*evs)[0]->generateCode(ctx);
        for (int i = 1; i < evs->size(); i++) {
            (*evs)[i]->generateCode(ctx);
            ctx->addLine(tpos, CMD_APPEND);
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

///////////////////
/////  Array  /////
///////////////////

class Array : public Evaluator {

    private:

    std::vector<std::vector<Evaluator *> > data;
    bool trans;

public:

    Array(int pos, std::vector<std::vector<Evaluator *> > data, bool trans) : Evaluator(pos), data(data), trans(trans) {}
    Array(int pos, std::vector<std::vector<Evaluator *> > data, bool trans, int dummy) : Evaluator(pos), trans(trans) {
        for (int i = 0; i < data.size(); i++) {
            std::vector<Evaluator *> row;
            for (int j = 0; j < data[i].size(); j++)
                row.push_back(data[i][j]->clone(NULL));
            this->data.push_back(row);
        }
    }

    ~Array() {
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                delete data[i][j];
    }

    Evaluator *clone(For *) {
        return new Array(tpos, data, trans, true);
    }

    void generateCode(GeneratorContext *ctx) {
        int rows = (int) data.size();
        int cols = 0;
        for (int i = 0; i < data.size(); i++) {
            int c = (int) data[i].size();
            if (cols < c)
                cols = c;
        }
        int lbl = ctx->nextLabel();
        ctx->addLine(tpos, CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(tpos, CMD_LBL, lbl);
        ctx->addLine(tpos, (phloat) rows);
        ctx->addLine(tpos, (phloat) cols);
        ctx->addLine(tpos, CMD_NEWMAT);
        ctx->addLine(tpos, CMD_LSTO, std::string("_TMPMAT"));
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, CMD_INDEX, std::string("_TMPMAT"));
        for (int i = 0; i < rows; i++) {
            int c = (int) data[i].size();
            for (int j = 0; j < c; j++) {
                data[i][j]->generateCode(ctx);
                ctx->addLine(data[i][j]->pos(), CMD_STOEL);
                ctx->addLine(tpos, CMD_DROP);
                if (j < c - 1)
                    ctx->addLine(tpos, CMD_J_ADD);
            }
            int gap = cols - c + 1;
            if (i < rows - 1)
                if (gap > 2) {
                    ctx->addLine(tpos, (phloat) (i + 2));
                    ctx->addLine(tpos, (phloat) 1);
                    ctx->addLine(tpos, CMD_STOIJ);
                    ctx->addLine(tpos, CMD_DROPN, 2);
                } else {
                    while (gap-- > 0)
                        ctx->addLine(tpos, CMD_J_ADD);
                }
        }
        ctx->addLine(tpos, CMD_RCL, std::string("_TMPMAT"));
        if (trans)
            ctx->addLine(tpos, CMD_TRANS);
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                data[i][j]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                if (data[i][j]->howMany(name) != 0)
                    return -1;
        return 0;
    }
};

//////////////////
/////  Badd  /////
//////////////////

class Badd : public BinaryEvaluator {

    public:

    Badd(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Badd(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_BASEADD);
    }
};

//////////////////
/////  Band  /////
//////////////////

class Band : public BinaryEvaluator {

    public:

    Band(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Band(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_AND);
    }
};

//////////////////
/////  Bdiv  /////
//////////////////

class Bdiv : public BinaryEvaluator {

    public:

    Bdiv(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Bdiv(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Bdiv(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_BASEDIV);
    }
};

//////////////////
/////  Bmul  /////
//////////////////

class Bmul : public BinaryEvaluator {

    public:

    Bmul(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Bmul(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_BASEMUL);
    }
};

/////////////////
/////  Bor  /////
/////////////////

class Bor : public BinaryEvaluator {

    public:

    Bor(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Bor(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_OR);
    }
};

///////////////////
/////  Break  /////
///////////////////

class Break : public Evaluator {

    private:

    For *f;

    public:

    Break(int pos, For *f) : Evaluator(pos), f(f) {}

    Evaluator *clone(For *f) {
        return new Break(tpos, f);
    }

    void generateCode(GeneratorContext *ctx);

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return 0;
    }
};

//////////////////
/////  Bsub  /////
//////////////////

class Bsub : public BinaryEvaluator {

    public:

    Bsub(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Bsub(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Bsub(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_BASESUB);
    }
};

//////////////////
/////  Bxor  /////
//////////////////

class Bxor : public BinaryEvaluator {

    public:

    Bxor(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Bxor(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_XOR);
    }
};

//////////////////
/////  Call  /////
//////////////////

class Call : public Evaluator {

    private:

    std::string name;
    std::vector<Evaluator *> *evs;

    public:

    Call(int pos, std::string name, std::vector<Evaluator *> *evs) : Evaluator(pos), name(name), evs(evs) {}

    ~Call() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Call(tpos, name, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        // Wrapping the equation call in another subroutine,
        // so ->PAR can create locals for the parameters without
        // stepping on any alread-existing locals with the
        // same name.
        int lbl = ctx->nextLabel();
        ctx->addLine(tpos, CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(tpos, CMD_LBL, lbl);
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->generateCode(ctx);
        ctx->addLine(tpos, (phloat) (int) evs->size());
        ctx->addLine(tpos, CMD_XSTR, name);
        ctx->addLine(tpos, CMD_GETEQN);
        ctx->addLine(tpos, CMD_TO_PAR);
        ctx->addLine(tpos, CMD_EVALN, 'L');
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

//////////////////
/////  Comb  /////
//////////////////

class Comb : public BinaryEvaluator {

    public:

    Comb(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Comb(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_COMB);
    }
};

///////////////////////
/////  CompareEQ  /////
///////////////////////

class CompareEQ : public BinaryEvaluator {

    public:

    CompareEQ(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareEQ(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_EQ);
    }
};

///////////////////////
/////  CompareNE  /////
///////////////////////

class CompareNE : public BinaryEvaluator {

    public:

    CompareNE(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareNE(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_NE);
    }
};

///////////////////////
/////  CompareLT  /////
///////////////////////

class CompareLT : public BinaryEvaluator {

    public:

    CompareLT(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareLT(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_LT);
    }
};

///////////////////////
/////  CompareLE  /////
///////////////////////

class CompareLE : public BinaryEvaluator {

    public:

    CompareLE(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareLE(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_LE);
    }
};

///////////////////////
/////  CompareGT  /////
///////////////////////

class CompareGT : public BinaryEvaluator {

    public:

    CompareGT(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareGT(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_GT);
    }
};

///////////////////////
/////  CompareGE  /////
///////////////////////

class CompareGE : public BinaryEvaluator {

    public:

    CompareGE(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new CompareGE(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_GE);
    }
};

//////////////////////
/////  Continue  /////
//////////////////////

class Continue : public Evaluator {

    private:

    For *f;

    public:

    Continue(int pos, For *f) : Evaluator(pos), f(f) {}

    Evaluator *clone(For *f) {
        return new Continue(tpos, f);
    }

    void generateCode(GeneratorContext *ctx);

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return 0;
    }
};

///////////////////
/////  Cross  /////
///////////////////

class Cross : public BinaryEvaluator {

    public:

    Cross(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Cross(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_CROSS);
    }
};

//////////////////
/////  Date  /////
//////////////////

class Date : public BinaryEvaluator {

    public:

    Date(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Date(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Date(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_DATE_PLUS);
    }
};

///////////////////
/////  Ddays  /////
///////////////////

class Ddays : public Evaluator {

    private:

    Evaluator *date1;
    Evaluator *date2;
    Evaluator *cal;

    public:

    Ddays(int pos, Evaluator *date1, Evaluator *date2, Evaluator *cal) : Evaluator(pos), date1(date1), date2(date2), cal(cal) {}

    Evaluator *clone(For *f) {
        return new Ddays(tpos, date1->clone(f), date2->clone(f), cal->clone(f));
    }

    ~Ddays() {
        delete date1;
        delete date2;
        delete cal;
    }

    void generateCode(GeneratorContext *ctx) {
        date1->generateCode(ctx);
        date2->generateCode(ctx);
        cal->generateCode(ctx);
        ctx->addLine(tpos, CMD_DDAYSC);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        date1->collectVariables(vars, locals);
        date2->collectVariables(vars, locals);
        cal->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        if (date1->howMany(name) != 0
                || date2->howMany(name) != 0
                || cal->howMany(name) != 0)
            return -1;
        else
            return 0;
    }
};

////////////////////////
/////  Difference  /////
////////////////////////

class Difference : public BinaryEvaluator {

    public:

    Difference(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Difference(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Difference(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_SUB);
    }
};

/////////////////
/////  Dot  /////
/////////////////

class Dot : public BinaryEvaluator {

    public:

    Dot(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Dot(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_DOT);
    }
};

/////////////////
/////  Ell  /////
/////////////////

class Ell : public Evaluator {

    private:

    std::string name;
    Evaluator *left, *right;
    bool compatMode;

    public:

    Ell(int pos, std::string name, Evaluator *right, bool compatMode) : Evaluator(pos), name(name), left(NULL), right(right), compatMode(compatMode) {}
    Ell(int pos, Evaluator *left, Evaluator *right, bool compatMode) : Evaluator(pos), name(""), left(left), right(right), compatMode(compatMode) {}

    ~Ell() {
        delete left;
        delete right;
    }

    Evaluator *clone(For *f) {
        if (name != "")
            return new Ell(tpos, name, right->clone(f), compatMode);
        else
            return new Ell(tpos, left->clone(f), right->clone(f), compatMode);
    }

    std::string name2() { return name; }

    void generateCode(GeneratorContext *ctx) {
        if (name != "") {
            right->generateCode(ctx);
            ctx->addLine(tpos, compatMode ? CMD_GSTO : CMD_STO, name);
        } else {
            left->generateCode(ctx);
            right->generateCode(ctx);
            left->generateAssignmentCode(ctx);
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        if (left != NULL)
            left->collectVariables(vars, locals);
        right->collectVariables(vars, locals);
    }

    int howMany(const std::string &nam) {
        if (left != NULL && left->howMany(nam) != 0)
            return -1;
        return right->howMany(nam) == 0 ? 0 : -1;
    }
};

//////////////////////
/////  Equation  /////
//////////////////////

class Equation : public BinaryEvaluator {

    public:

    Equation(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Equation(tpos, left->clone(f), right->clone(f));
    }

    void getSides(const std::string &name, Evaluator **lhs, Evaluator **rhs) {
        if (left->howMany(name) == 1) {
            *lhs = left;
            *rhs = right;
        } else {
            *lhs = right;
            *rhs = left;
        }
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_SUB);
    }
};

/////////////////
/////  Ess  /////
/////////////////

class Ess : public Evaluator {

    private:

    std::string name;

    public:

    Ess(int pos, std::string name) : Evaluator(pos), name(name) {}

    Evaluator *clone(For *f) {
        return new Ess(tpos, name);
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_XSTR, name);
        ctx->addLine(tpos, CMD_SVAR);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &nam) {
        return 0;
    }
};

//////////////////////
/////  FlowItem  /////
//////////////////////

class FlowItem : public UnaryEvaluator {

    private:

    std::string name;
    int col;

    public:

    FlowItem(int pos, std::string name, Evaluator *ev, int col) : UnaryEvaluator(pos, ev, false), name(name), col(col) {}

    Evaluator *clone(For *f) {
        return new FlowItem(tpos, name, ev->clone(f), col);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_XSTR, name);
        ev->generateCode(ctx);
        ctx->addLine(tpos, (phloat) 1);
        ctx->addLine(tpos, CMD_ADD);
        ctx->addLine(tpos, (phloat) col);
        if (col == 1) {
            ctx->addLine(tpos, CMD_GETITEM);
        } else {
            ctx->addLine(tpos, CMD_SF, 25);
            ctx->addLine(tpos, CMD_GETITEM);
            ctx->addLine(tpos, CMD_FSC_T, 25);
            int lbl = ctx->nextLabel();
            ctx->addLine(tpos, CMD_GTOL, lbl);
            // #T() on a 1-column CFLO list should return 1 for all valid row indexes.
            // We verify the validity of the row index by retrying the GETITEM with
            // column 1 instead of 2, and since we're not using flag 25 this time,
            // a Dimension Error will be raised if the row index is out of range.
            // If no error is raised, we proceed by discarding the result of GETITEM,
            // and returning LASTx instead, which at this point will contain 1.
            ctx->addLine(tpos, CMD_SIGN);
            ctx->addLine(tpos, CMD_GETITEM);
            ctx->addLine(tpos, CMD_CLX);
            ctx->addLine(tpos, CMD_LASTX);
            ctx->addLine(tpos, CMD_LBL, lbl);
        }
    }

    int howMany(const std::string &nam) {
        if (nam == name || ev->howMany(nam) != 0)
            return -1;
        else
            return 0;
    }
};

/////////////////
/////  For  /////
/////////////////

class For : public Evaluator {

    private:

    Evaluator *init, *cond, *next;
    std::vector<Evaluator *> *evs;
    int breakLbl, continueLbl;

    public:

    For(int pos) : Evaluator(pos), init(NULL), cond(NULL), next(NULL), evs(NULL) {}

    ~For() {
        delete init;
        delete cond;
        delete next;
        if (evs != NULL) {
            for (int i = 0; i < evs->size(); i++)
                delete (*evs)[i];
            delete evs;
        }
    }

    Evaluator *clone(For *) {
        For *f = new For(tpos);
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        f->finish_init(init->clone(f), cond->clone(f), next->clone(f), evs2);
        return f;
    }

    void finish_init(Evaluator *init, Evaluator *cond, Evaluator *next, std::vector<Evaluator *> *evs) {
        this->init = init;
        this->cond = cond;
        this->next = next;
        this->evs = evs;
    }

    int getBreak() {
        return breakLbl;
    }

    int getContinue() {
        return continueLbl;
    }

    void generateCode(GeneratorContext *ctx) {
        breakLbl = ctx->nextLabel();
        continueLbl = ctx->nextLabel();
        int top = ctx->nextLabel();
        int test = ctx->nextLabel();
        init->generateCode(ctx);
        ctx->addLine(tpos, CMD_GTOL, test);
        ctx->addLine(tpos, CMD_LBL, top);
        for (int i = 0; i < evs->size(); i++) {
            (*evs)[i]->generateCode(ctx);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
        }
        ctx->addLine(tpos, CMD_LBL, continueLbl);
        next->generateCode(ctx);
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, CMD_LBL, test);
        cond->generateCode(ctx);
        ctx->addLine(tpos, CMD_IF_T);
        ctx->addLine(tpos, CMD_GTOL, top);
        ctx->addLine(tpos, CMD_LBL, breakLbl);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        init->collectVariables(vars, locals);
        cond->collectVariables(vars, locals);
        next->collectVariables(vars, locals);
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        if (init->howMany(name) != 0
                || cond->howMany(name) != 0
                || next->howMany(name) != 0)
            return -1;
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Gee  /////
/////////////////

class Gee : public Evaluator {

    private:

    std::string name;
    bool compatMode;

    public:

    Gee(int pos, std::string name, bool compatMode) : Evaluator(pos), name(name), compatMode(compatMode) {}

    Evaluator *clone(For *f) {
        return new Gee(tpos, name, compatMode);
    }

    std::string name2() { return name; }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, compatMode ? CMD_GRCL : CMD_RCL, name);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return 0;
    }
};

////////////////////////
/////  HeadOrTail  /////
////////////////////////

class HeadOrTail : public UnaryEvaluator {

    private:

    bool head;

    public:

    HeadOrTail(int pos, Evaluator *ev, bool head) : UnaryEvaluator(pos, ev, false), head(head) {}

    Evaluator *clone(For *f) {
        return new HeadOrTail(tpos, ev->clone(f), head);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_HEAD, 'X');
        if (head) {
            int lbl = ctx->nextLabel();
            ctx->addLine(tpos, CMD_SKIP);
            ctx->addLine(tpos, CMD_GTOL, lbl);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, CMD_LBL, lbl);
        } else {
            ctx->addLine(tpos, CMD_DROP);
        }
    }
};

////////////////////
/////  Hmsadd  /////
////////////////////

class Hmsadd : public BinaryEvaluator {

    public:

    Hmsadd(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Hmsadd(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_HMSADD);
    }
};

////////////////////
/////  Hmssub  /////
////////////////////

class Hmssub : public BinaryEvaluator {

    public:

    Hmssub(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Hmssub(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Hmssub(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_HMSSUB);
    }
};

//////////////////
/////  Idiv  /////
//////////////////

class Idiv : public BinaryEvaluator {

    public:

    Idiv(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Idiv(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_DIV);
        ctx->addLine(tpos, CMD_IP);
    }
};

////////////////
/////  If  /////
////////////////

class If : public Evaluator {

    private:

    Evaluator *condition, *trueEv, *falseEv;

    public:

    If(int pos, Evaluator *condition, Evaluator *trueEv, Evaluator *falseEv)
            : Evaluator(pos), condition(condition), trueEv(trueEv), falseEv(falseEv) {}

    ~If() {
        delete condition;
        delete trueEv;
        delete falseEv;
    }

    Evaluator *clone(For *f) {
        return new If(tpos, condition->clone(f), trueEv->clone(f), falseEv->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        condition->generateCode(ctx);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(tpos, CMD_IF_T);
        ctx->addLine(tpos, CMD_GTOL, lbl1);
        falseEv->generateCode(ctx);
        ctx->addLine(tpos, CMD_GTOL, lbl2);
        ctx->addLine(tpos, CMD_LBL, lbl1);
        trueEv->generateCode(ctx);
        ctx->addLine(tpos, CMD_LBL, lbl2);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        condition->collectVariables(vars, locals);
        trueEv->collectVariables(vars, locals);
        falseEv->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        if (condition->howMany(name) != 0)
            return -1;
        int t = trueEv->howMany(name);
        int f = falseEv->howMany(name);
        if (t == 1 || f == 1)
            return 1;
        else if (t == -1 || f == -1)
            return -1;
        else
            return 0;
    }
};

/////////////////
/////  Int  /////
/////////////////

class Int : public UnaryEvaluator {

    public:

    Int(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone(For *f) {
        return new Int(tpos, ev->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_IP);
        ctx->addLine(tpos, CMD_X_EQ_NN, 'L');
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(tpos, CMD_GTOL, lbl1);
        ctx->addLine(tpos, CMD_0_LT_NN, 'L');
        ctx->addLine(tpos, CMD_GTOL, lbl1);
        ctx->addLine(tpos, CMD_UNIT_T);
        ctx->addLine(tpos, CMD_GTOL, lbl2);
        ctx->addLine(tpos, (phloat) 1);
        ctx->addLine(tpos, CMD_SUB);
        ctx->addLine(tpos, CMD_GTOL, lbl1);
        ctx->addLine(tpos, CMD_LBL, lbl2);
        ctx->addLine(tpos, (phloat) 1);
        ctx->addLine(tpos, CMD_RCL, 'Y');
        ctx->addLine(tpos, CMD_TO_UNIT);
        ctx->addLine(tpos, CMD_SUB);
        ctx->addLine(tpos, CMD_LBL, lbl1);
    }
};

///////////////////
/////  Integ  /////
///////////////////

class Integ : public Evaluator {

    private:

    Evaluator *expr;
    std::string integ_var;
    Evaluator *llim;
    Evaluator *ulim;
    Evaluator *acc;

    public:

    Integ(int tpos, Evaluator *expr, std::string integ_var, Evaluator *llim, Evaluator *ulim, Evaluator *acc)
        : Evaluator(tpos), expr(expr), integ_var(integ_var), llim(llim), ulim(ulim), acc(acc) {}

    ~Integ() {
        delete expr;
        delete llim;
        delete ulim;
        delete acc;
    }

    Evaluator *clone(For *f) {
        return new Integ(tpos, expr->clone(f), integ_var, llim->clone(f), ulim->clone(f), acc == NULL ? NULL : acc->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        llim->generateCode(ctx);
        ulim->generateCode(ctx);
        if (acc != NULL)
            acc->generateCode(ctx);
        ctx->addLine(tpos, (phloat) 0);
        int lbl = ctx->nextLabel();
        ctx->addLine(tpos, CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(tpos, CMD_LBL, lbl);
        ctx->addLine(tpos, CMD_LSTO, integ_var);
        ctx->addLine(tpos, CMD_DROP);
        if (acc != NULL) {
            ctx->addLine(tpos, CMD_LSTO, std::string("ACC"));
            ctx->addLine(tpos, CMD_DROP);
        }
        ctx->addLine(tpos, CMD_LSTO, std::string("ULIM"));
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, CMD_LSTO, std::string("LLIM"));
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, CMD_XSTR, expr->getText());
        ctx->addLine(tpos, CMD_PARSE);
        ctx->addLine(tpos, CMD_EQNINT, 'X');
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, CMD_INTEG, integ_var);
        ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_DROP);
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        locals->push_back(integ_var);
        expr->collectVariables(vars, locals);
        locals->pop_back();
        llim->collectVariables(vars, locals);
        ulim->collectVariables(vars, locals);
        if (acc != NULL)
            acc->collectVariables(vars, locals);
    }

    int howMany(const std::string &nam) {
        if (nam != integ_var) {
            if (expr->howMany(nam) != 0)
                return -1;
        }
        if (llim->howMany(nam) != 0
                || ulim->howMany(nam) != 0
                || acc != NULL && acc->howMany(nam) != 0)
            return -1;
        return 0;
    }
};

//////////////////
/////  Item  /////
//////////////////

class Item : public Evaluator {

    private:

    std::string name;
    Evaluator *ev1, *ev2;
    bool lvalue;

    public:

    Item(int pos, std::string name, Evaluator *ev1, Evaluator *ev2) : Evaluator(pos), name(name), ev1(ev1), ev2(ev2), lvalue(false) {}

    ~Item() {
        delete ev1;
        delete ev2;
    }

    Evaluator *clone(For *f) {
        Evaluator *ret = new Item(tpos, name, ev1->clone(f), ev2 == NULL ? NULL : ev2->clone(f));
        if (lvalue)
            ret->makeLvalue();
        return ret;
    }

    bool makeLvalue() {
        lvalue = true;
        return true;
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_XSTR, name);
        ev1->generateCode(ctx);
        if (ev2 != NULL)
            ev2->generateCode(ctx);
        if (!lvalue)
            ctx->addLine(tpos, CMD_GETITEM);
    }

    void generateAssignmentCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_PUTITEM);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        ev1->collectVariables(vars, locals);
        if (ev2 != NULL)
            ev2->collectVariables(vars, locals);
    }

    int howMany(const std::string &nam) {
        if (nam == name || ev1->howMany(nam) != 0 || ev2 != NULL && ev2->howMany(nam) != 0)
            return -1;
        else
            return 0;
    }
};

//////////////////
/////  List  /////
//////////////////

class List : public Evaluator {

    private:

    std::vector<Evaluator *> data;

    public:

    List(int pos, std::vector<Evaluator *> data) : Evaluator(pos), data(data) {}

    ~List() {
        for (int i = 0; i < data.size(); i++)
            delete data[i];
    }

    Evaluator *clone(For *) {
        std::vector<Evaluator *> data_copy;
        for (int i = 0; i < data.size(); i++)
            data_copy.push_back(data[i]->clone(NULL));
        return new List(tpos, data_copy);
    }

    void generateCode(GeneratorContext *ctx) {
        if (data.size() == 0) {
            ctx->addLine(tpos, CMD_NEWLIST);
        } else {
            for (int i = 0; i < data.size(); i++)
                data[i]->generateCode(ctx);
            ctx->addLine(tpos, (phloat) (int) data.size());
            ctx->addLine(tpos, CMD_TO_LIST);
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < data.size(); i++)
            data[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < data.size(); i++)
            if (data[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////////
/////  Literal  /////
/////////////////////

class Literal : public Evaluator {

    private:

    phloat value;

    public:

    Literal(int pos, phloat value) : Evaluator(pos), value(value) {}

    Evaluator *clone(For *f) {
        return new Literal(tpos, value);
    }

    bool isLiteral() { return true; }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, value);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return 0;
    }
};

//////////////////////
/////  LocalEll  /////
//////////////////////

class LocalEll : public Evaluator {

    private:

    std::string name;
    Evaluator *value;
    std::vector<Evaluator *> *evs;

    public:

    LocalEll(int pos, std::string name, Evaluator *value, std::vector<Evaluator *> *evs) : Evaluator(pos), name(name), value(value), evs(evs) {}

    ~LocalEll() {
        delete value;
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new LocalEll(tpos, name, value->clone(f), evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        int lbl = ctx->nextLabel();
        ctx->addLine(tpos, CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(tpos, CMD_LBL, lbl);
        value->generateCode(ctx);
        ctx->addLine(tpos, CMD_LSTO, name);
        for (int i = 0; i < evs->size(); i++) {
            ctx->addLine(tpos, CMD_DROP);
            (*evs)[i]->generateCode(ctx);
        }
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        value->collectVariables(vars, locals);
        locals->push_back(name);
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
        locals->pop_back();
    }

    int howMany(const std::string &nam) {
        if (value->howMany(nam) != 0)
            return -1;
        if (nam != name) {
            for (int i = 0; i < evs->size(); i++)
                if ((*evs)[i]->howMany(name) != 0)
                    return -1;
        }
        return 0;
    }
};

/////////////////
/////  Max  /////
/////////////////

class Max : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Max(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}

    ~Max() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Max(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        if (evs->size() == 0) {
            ctx->addLine(tpos, NEG_HUGE_PHLOAT);
        } else {
            (*evs)[0]->generateCode(ctx);
            for (int i = 1; i < evs->size(); i++) {
                (*evs)[i]->generateCode(ctx);
                ctx->addLine(tpos, CMD_X_GT_Y);
                ctx->addLine(tpos, CMD_SWAP);
                ctx->addLine(tpos, CMD_DROP);
            }
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Min  /////
/////////////////

class Min : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;

    public:

    Min(int pos, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs) {}

    ~Min() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Min(tpos, evs2);
    }

    void generateCode(GeneratorContext *ctx) {
        if (evs->size() == 0) {
            ctx->addLine(tpos, POS_HUGE_PHLOAT);
        } else {
            (*evs)[0]->generateCode(ctx);
            for (int i = 1; i < evs->size(); i++) {
                (*evs)[i]->generateCode(ctx);
                ctx->addLine(tpos, CMD_X_LT_Y);
                ctx->addLine(tpos, CMD_SWAP);
                ctx->addLine(tpos, CMD_DROP);
            }
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Mod  /////
/////////////////

class Mod : public BinaryEvaluator {

    public:

    Mod(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Mod(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_MOD);
    }
};

/////////////////////
/////  NameTag  /////
/////////////////////

class NameTag : public UnaryEvaluator {

    private:

    std::string name;
    std::vector<std::string> *params;

    public:

    /* Note that this class declares itself to be invertible. It isn't, really,
     * but it doesn't matter, since the NameTag will always be removed before
     * the parse tree is inverted. We're doing this just so that howMany(name)
     * will return ev->howMany(name) without touching it.
     */
    NameTag(int pos, std::string name, std::vector<std::string> *params, Evaluator *ev)
            : UnaryEvaluator(pos, ev, true), name(name), params(params) {}

    ~NameTag() {
        delete params;
    }

    Evaluator *clone(For *f) {
        return new NameTag(tpos, name, new std::vector<std::string>(*params), ev->clone(f));
    }

    void getSides(const std::string &name, Evaluator **lhs, Evaluator **rhs) {
        ev->getSides(name, lhs, rhs);
    }

    std::string eqnName() {
        return name;
    }

    std::vector<std::string> *eqnParamNames() {
        return params;
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        /* Force parameters to be at the head of the list */
        if (params != NULL)
            for (int i = 0; i < params->size(); i++)
                addIfNew((*params)[i], vars, locals);
        if (ev != NULL)
            ev->collectVariables(vars, locals);
    }
};

//////////////////////
/////  Negative  /////
//////////////////////

class Negative : public UnaryEvaluator {

    public:

    Negative(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, true) {}

    Evaluator *clone(For *f) {
        return new Negative(tpos, ev->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs) {
        return ev->invert(name, new Negative(0, rhs));
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_CHS);
    }
};

////////////////////
/////  Newmat  /////
////////////////////

class Newmat : public BinaryEvaluator {

    public:

    Newmat(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Newmat(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_NEWMAT);
    }
};

/////////////////
/////  Not  /////
/////////////////

class Not : public UnaryEvaluator {

    public:

    Not(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone(For *f) {
        return new Not(tpos, ev->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_NOT);
    }
};

////////////////
/////  Or  /////
////////////////

class Or : public BinaryEvaluator {

    public:

    Or(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Or(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_OR);
    }
};

/////////////////////
/////  Pcomplx  /////
/////////////////////

class Pcomplx : public BinaryEvaluator {

    public:

    Pcomplx(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Pcomplx(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addAssertTwoReals(tpos);
        ctx->addLine(tpos, CMD_PCOMPLX);
    }
};

//////////////////
/////  Perm  /////
//////////////////

class Perm : public BinaryEvaluator {

    public:

    Perm(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Perm(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_PERM);
    }
};

/////////////////////////
/////  PosOrSubstr  /////
/////////////////////////

class PosOrSubstr : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;
    bool do_pos;

    public:

    PosOrSubstr(int pos, std::vector<Evaluator *> *evs, bool do_pos) : Evaluator(pos), evs(evs), do_pos(do_pos) {}

    ~PosOrSubstr() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new PosOrSubstr(tpos, evs2, do_pos);
    }

    void generateCode(GeneratorContext *ctx) {
        (*evs)[0]->generateCode(ctx);
        (*evs)[1]->generateCode(ctx);
        if (evs->size() == 3) {
            ctx->addLine(tpos, CMD_REAL_T);
            ctx->addLine(tpos, CMD_SKIP);
            ctx->addLine(tpos, CMD_RAISE, ERR_INVALID_DATA);
            (*evs)[2]->generateCode(ctx);
        }
        ctx->addLine(tpos, do_pos ? CMD_POS : CMD_SUBSTR);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

///////////////////
/////  Power  /////
///////////////////

class Power : public BinaryEvaluator {

    public:

    Power(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Power(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Power(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_Y_POW_X);
    }
};

/////////////////////
/////  Product  /////
/////////////////////

class Product : public BinaryEvaluator {

    public:

    Product(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Product(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_MUL);
    }
};

//////////////////////
/////  Quotient  /////
//////////////////////

class Quotient : public BinaryEvaluator {

    public:

    Quotient(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}
    Quotient(int pos, Evaluator *left, Evaluator *right, bool swapArgs) : BinaryEvaluator(pos, left, right, true, swapArgs) {}

    Evaluator *clone(For *f) {
        return new Quotient(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        if (swapArgs)
            ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_DIV);
    }
};

////////////////////
/////  Radius  /////
////////////////////

class Radius : public BinaryEvaluator {

    public:

    Radius(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Radius(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(tpos, CMD_CPX_T);
            ctx->addLine(tpos, CMD_GTOL, lbl1);
            ctx->addLine(tpos, (phloat) 0);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_TO_POL);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, CMD_GTOL, lbl2);
            ctx->addLine(tpos, CMD_LBL, lbl1);
            ctx->addLine(tpos, CMD_PCOMPLX);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_TO_POL);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
        }
    }
};

/////////////////////
/////  Rcomplx  /////
/////////////////////

class Rcomplx : public BinaryEvaluator {

    public:

    Rcomplx(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Rcomplx(tpos, left->clone(f), right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addAssertTwoReals(tpos);
        ctx->addLine(tpos, CMD_RCOMPLX);
    }
};

//////////////////////
/////  Register  /////
//////////////////////

class Register : public Evaluator {

    private:

    int index; // L=0, X=1, Y=2, Z=3, T=4
    Evaluator *ev;

    public:

    Register(int pos, int index) : Evaluator(pos), index(index), ev(NULL) {}
    Register(int pos, Evaluator *ev) : Evaluator(pos), ev(ev) {}

    ~Register() {
        delete ev;
    }

    Evaluator *clone(For *f) {
        if (ev == NULL)
            return new Register(tpos, index);
        else
            return new Register(tpos, ev);
    }

    void generateCode(GeneratorContext *ctx) {
        if (ev == NULL)
            ctx->addLine(tpos, (phloat) index);
        else
            ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_FSTACK);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        if (ev != NULL)
            ev->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        if (ev == NULL)
            return 0;
        else
            return ev->howMany(name) == 0 ? 0 : -1;
    }
};

/////////////////
/////  Rnd  /////
/////////////////

class Rnd : public BinaryEvaluator {

    private:

    bool trunc;

    public:

    Rnd(int pos, Evaluator *left, Evaluator *right, bool trunc) : BinaryEvaluator(pos, left, right, false), trunc(trunc) {}

    Evaluator *clone(For *f) {
        return new Rnd(tpos, left->clone(f), right->clone(f), trunc);
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_RCLFLAG);
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_X_LT_0);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(tpos, CMD_GTOL, lbl1);
        ctx->addLine(tpos, CMD_FIX, 'X', true);
        ctx->addLine(tpos, CMD_GTOL, lbl2);
        ctx->addLine(tpos, CMD_LBL, lbl1);
        ctx->addLine(tpos, (phloat) -1);
        ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, CMD_SUB);
        ctx->addLine(tpos, CMD_SCI, 'X', true);
        ctx->addLine(tpos, CMD_LBL, lbl2);
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, trunc ? CMD_TRUNC : CMD_RND);
        ctx->addLine(tpos, CMD_SWAP);
        ctx->addLine(tpos, (phloat) 36.41);
        ctx->addLine(tpos, CMD_STOFLAG);
        ctx->addLine(tpos, CMD_DROPN, 2);
    }
};

/////////////////
/////  Seq  /////
/////////////////

class Seq : public Evaluator {

    private:

    std::vector<Evaluator *> *evs;
    bool view;
    bool compatMode;

    public:

    Seq(int pos, bool view, bool compatMode, std::vector<Evaluator *> *evs) : Evaluator(pos), evs(evs), view(view), compatMode(compatMode) {}

    ~Seq() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Seq(tpos, view, compatMode, evs2);
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size() - 1; i++)
            evs2->push_back((*evs)[i]->clone(NULL));
        evs2->push_back(rhs);
        return evs->back()->invert(name, new Seq(0, view, compatMode, evs2));
    }

    void generateCode(GeneratorContext *ctx) {
        int sz = (int) evs->size();
        for (int i = 0; i < sz; i++) {
            Evaluator *ev = (*evs)[i];
            bool isLast = i == sz - 1;
            bool generate = isLast || ev->name() == "";
            if (generate)
                ev->generateCode(ctx);
            if (view) {
                std::string name = ev->name2();
                if (name != "") {
                    if (compatMode && ev->name() == "") {
                        // L() using GSTO or G() using GRCL; if we just use VIEW
                        // here, we could end up viewing a local instead of the
                        // intended global. In order to print the correct value,
                        // which is now in level 1, and print it with the NAME=
                        // label, we create a local with the same name for VIEW
                        // to pick up.
                        int lbl = ctx->nextLabel();
                        ctx->pushSubroutine();
                        ctx->addLine(tpos, CMD_LBL, lbl);
                        ctx->addLine(tpos, CMD_LSTO, name);
                        ctx->addLine(tpos, CMD_VIEW, name);
                        ctx->popSubroutine();
                        ctx->addLine(tpos, CMD_XEQL, lbl);
                    } else {
                        ctx->addLine(tpos, CMD_VIEW, name);
                    }
                } else if (ev->isString()) {
                    ctx->addLine(tpos, CMD_XVIEW);
                } else {
                    int lbl1 = ctx->nextLabel();
                    int lbl2 = ctx->nextLabel();
                    ctx->addLine(tpos, CMD_STR_T);
                    ctx->addLine(tpos, CMD_GTOL, lbl1);
                    ctx->addLine(tpos, CMD_VIEW, 'X');
                    ctx->addLine(tpos, CMD_GTOL, lbl2);
                    ctx->addLine(tpos, CMD_LBL, lbl1);
                    ctx->addLine(tpos, CMD_XVIEW);
                    ctx->addLine(tpos, CMD_LBL, lbl2);
                }
            }
            if (generate && !isLast)
                ctx->addLine(tpos, CMD_DROP);
        }
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < evs->size() - 1; i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return evs->back()->howMany(name);
    }
};

/////////////////
/////  Sgn  /////
/////////////////

class Sgn : public UnaryEvaluator {

    public:

    Sgn(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone(For *f) {
        return new Sgn(tpos, ev->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_UNIT_T);
        ctx->addLine(tpos, CMD_UVAL);
        ctx->addLine(tpos, CMD_REAL_T);
        ctx->addLine(tpos, CMD_X_NE_0);
        ctx->addLine(tpos, CMD_SIGN);
    }
};

///////////////////
/////  Sigma  /////
///////////////////

class Sigma : public Evaluator {

    private:

    std::string name;
    Evaluator *from;
    Evaluator *to;
    Evaluator *step;
    Evaluator *ev;

    public:

    Sigma(int pos, std::string name, Evaluator *from, Evaluator *to, Evaluator *step, Evaluator *ev)
        : Evaluator(pos), name(name), from(from), to(to), step(step), ev(ev) {}

    ~Sigma() {
        delete from;
        delete to;
        delete step;
        delete ev;
    }

    Evaluator *clone(For *f) {
        return new Sigma(tpos, name, from->clone(NULL), to->clone(NULL), step->clone(NULL), ev->clone(NULL));
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, (phloat) 0);
        to->generateCode(ctx);
        step->generateCode(ctx);
        ctx->addLine(tpos, CMD_X_EQ_0);
        ctx->addLine(tpos, CMD_RAISE, ERR_INVALID_DATA);
        from->generateCode(ctx);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        int lbl3 = ctx->nextLabel();
        int lbl4 = ctx->nextLabel();
        int lbl5 = ctx->nextLabel();
        ctx->addLine(tpos, CMD_XEQL, lbl1);
        ctx->pushSubroutine();
        ctx->addLine(tpos, CMD_LBL, lbl1);
        ctx->addLine(tpos, CMD_LSTO, name);
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, CMD_GTOL, lbl3);
        ctx->addLine(tpos, CMD_LBL, lbl2);
        ctx->addLine(tpos, CMD_RDNN, 3);
        ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_ADD);
        ctx->addLine(tpos, CMD_RDNN, 3);
        ctx->addLine(tpos, CMD_STO_ADD, name);
        ctx->addLine(tpos, CMD_LBL, lbl3);
        ctx->addLine(tpos, CMD_X_LT_0);
        ctx->addLine(tpos, CMD_GTOL, lbl4);
        ctx->addLine(tpos, CMD_RDNN, 3);
        ctx->addLine(tpos, CMD_X_GE_NN, name);
        ctx->addLine(tpos, CMD_GTOL, lbl2);
        ctx->addLine(tpos, CMD_GTOL, lbl5);
        ctx->addLine(tpos, CMD_LBL, lbl4);
        ctx->addLine(tpos, CMD_RDNN, 3);
        ctx->addLine(tpos, CMD_X_LE_NN, name);
        ctx->addLine(tpos, CMD_GTOL, lbl2);
        ctx->addLine(tpos, CMD_LBL, lbl5);
        ctx->addLine(tpos, CMD_RUPN, 3);
        ctx->addLine(tpos, CMD_DROPN, 2);
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        from->collectVariables(vars, locals);
        to->collectVariables(vars, locals);
        step->collectVariables(vars, locals);
        locals->push_back(name);
        ev->collectVariables(vars, locals);
        locals->pop_back();
    }

    int howMany(const std::string &nam) {
        if (from->howMany(nam) != 0
                || to->howMany(nam) != 0
                || step->howMany(nam) != 0)
            return -1;
        if (nam != name) {
            if (ev->howMany(nam) != 0)
                return -1;
        }
        return 0;
    }
};

//////////////////
/////  Size  /////
//////////////////

class Size : public UnaryEvaluator {

    private:

    char mode;

    public:

    Size(int pos, Evaluator *ev, char mode) : UnaryEvaluator(pos, ev, false), mode(mode) {}

    Evaluator *clone(For *f) {
        return new Size(tpos, ev->clone(f), mode);
    }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        if (mode == 'S') {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(tpos, CMD_LIST_T);
            ctx->addLine(tpos, CMD_GTOL, lbl1);
            ctx->addLine(tpos, CMD_DIM_T);
            ctx->addLine(tpos, CMD_MUL);
            ctx->addLine(tpos, CMD_GTOL, lbl2);
            ctx->addLine(tpos, CMD_LBL, lbl1);
            ctx->addLine(tpos, CMD_LENGTH);
            ctx->addLine(tpos, CMD_LBL, lbl2);
        } else {
            ctx->addLine(tpos, CMD_DIM_T);
            if (mode == 'C')
                ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
        }
    }
};

///////////////////
/////  SizeC  /////
///////////////////

class SizeC : public UnaryEvaluator {

    public:

    SizeC(int pos, Evaluator *ev) : UnaryEvaluator(pos, ev, false) {}

    Evaluator *clone(For *f) {
        return new SizeC(tpos, ev->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        /* Cash flow lists are assumed to be n*2 matrices.
         * Unlike Sigma lists, they use 0-based indexing,
         * but SIZEC() returns the number of the last flow,
         * so it needs to report the actual size minus one.
         */
        ev->generateCode(ctx);
        ctx->addLine(tpos, CMD_DIM_T);
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, (phloat) 1);
        ctx->addLine(tpos, CMD_SUB);
    }
};

////////////////////
/////  String  /////
////////////////////

class String : public Evaluator {

    private:

    std::string value;

    public:

    String(int pos, std::string value) : Evaluator(pos), value(value) {}

    Evaluator *clone(For *) {
        return new String(tpos, value);
    }

    bool isString() { return true; }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_XSTR, value);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        // nope
    }

    int howMany(const std::string &name) {
        return 0;
    }
};

/////////////////
/////  Sum  /////
/////////////////

class Sum : public BinaryEvaluator {

    public:

    Sum(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, true) {}

    Evaluator *clone(For *f) {
        return new Sum(tpos, left->clone(f), right->clone(f));
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_ADD);
    }
};

/////////////////
/////  Tvm  /////
/////////////////

class Tvm : public Evaluator {

    private:

    int cmd;
    std::vector<Evaluator *> *evs;

    public:

    Tvm(int pos, int cmd, std::vector<Evaluator *> *evs) : Evaluator(pos), cmd(cmd), evs(evs) {}

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Tvm(tpos, cmd, evs2);
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs);

    ~Tvm() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    void generateCode(GeneratorContext *ctx) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->generateCode(ctx);
        ctx->addLine(tpos, cmd);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        if ((*evs)[4]->howMany(name) != 0
                || (*evs)[5]->howMany(name) != 0)
            return -1;
        int n = 0;
        for (int i = 0; i < 4; i++) {
            int m = (*evs)[i]->howMany(name);
            if (m == -1)
                return -1;
            n += m;
        }
        return n;
    }
};

//////////////////////
/////  TypeTest  /////
//////////////////////

class TypeTest : public UnaryEvaluator {

    private:

    int cmd;

    public:

    TypeTest(int pos, Evaluator *ev, int cmd) : UnaryEvaluator(pos, ev, false), cmd(cmd) {}

    Evaluator *clone(For *f) {
        return new TypeTest(tpos, ev->clone(f), cmd);
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        ev->generateCode(ctx);
        int lbl1 = ctx->nextLabel();
        int lbl2 = ctx->nextLabel();
        ctx->addLine(tpos, cmd);
        ctx->addLine(tpos, CMD_GTOL, lbl1);
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, (phloat) 0);
        ctx->addLine(tpos, CMD_GTOL, lbl2);
        ctx->addLine(tpos, CMD_LBL, lbl1);
        ctx->addLine(tpos, CMD_DROP);
        ctx->addLine(tpos, (phloat) 1);
        ctx->addLine(tpos, CMD_LBL, lbl2);
    }
};

//////////////////
/////  Unit  /////
//////////////////

class Unit : public BinaryEvaluator {

    private:

    bool inverse;

    public:

    Unit(int pos, Evaluator *left, Evaluator *right, bool inverse = false) : BinaryEvaluator(pos, left, right, true), inverse(inverse) {}

    Evaluator *clone(For *f) {
        return new Unit(tpos, left->clone(f), right->clone(f), inverse);
    }

    Evaluator *invert(const std::string &name, Evaluator *rhs) {
        return left->invert(name, new Unit(0, rhs, right->clone(NULL), true));
    }

    void generateCode(GeneratorContext *ctx) {
        if (inverse) {
            left->generateCode(ctx);
            ctx->addLine(tpos, (phloat) 1);
            right->generateCode(ctx);
            ctx->addLine(tpos, CMD_TO_UNIT);
            ctx->addLine(tpos, CMD_DIV);
            ctx->addLine(tpos, CMD_UBASE);
            ctx->addLine(tpos, CMD_UNIT_T);
            ctx->addLine(tpos, CMD_RAISE, ERR_INVALID_DATA);
        } else if (left->isLiteral() && right->isString()) {
            ctx->addLine(left->pos(), CMD_N_PLUS_U);
            left->generateCode(ctx);
            right->generateCode(ctx);
        } else {
            left->generateCode(ctx);
            right->generateCode(ctx);
            ctx->addLine(tpos, CMD_TO_UNIT);
        }
    }

    int howMany(const std::string &name) {
        if (right->howMany(name) != 0)
            return -1;
        return left->howMany(name);
    }
};

//////////////////////
/////  Variable  /////
//////////////////////

class Variable : public Evaluator {

    private:

    std::string nam;

    public:

    Variable(int pos, std::string name) : Evaluator(pos), nam(name) {}

    Evaluator *clone(For *f) {
        return new Variable(tpos, nam);
    }

    std::string name() { return nam; }
    std::string name2() { return nam; }

    Evaluator *invert(const std::string &name, Evaluator *rhs) {
        if (nam == name) {
            return rhs;
        } else {
            delete rhs;
            return new Abort(tpos);
        }
    }

    void generateCode(GeneratorContext *ctx) {
        ctx->addLine(tpos, CMD_RCL, nam);
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        addIfNew(nam, vars, locals);
    }

    int howMany(const std::string &name) {
        return nam == name;
    }
};

////////////////////
/////  Xcoord  /////
////////////////////

class Xcoord : public BinaryEvaluator {

    public:

    Xcoord(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Xcoord(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(tpos, CMD_CPX_T);
            ctx->addLine(tpos, CMD_GTOL, lbl1);
            ctx->addLine(tpos, CMD_GTOL, lbl2);
            ctx->addLine(tpos, CMD_LBL, lbl1);
            ctx->addLine(tpos, CMD_RCOMPLX);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_TO_REC);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
        }
    }
};

/////////////////
/////  Xeq  /////
/////////////////

class Xeq : public Evaluator {

    private:

    std::string name;
    std::vector<Evaluator *> *evs;
    bool evaln;

    public:

    Xeq(int pos, std::string name, std::vector<Evaluator *> *evs, bool evaln) : Evaluator(pos), name(name), evs(evs), evaln(evaln) {}

    ~Xeq() {
        for (int i = 0; i < evs->size(); i++)
            delete (*evs)[i];
        delete evs;
    }

    Evaluator *clone(For *f) {
        std::vector<Evaluator *> *evs2 = new std::vector<Evaluator *>;
        for (int i = 0; i < evs->size(); i++)
            evs2->push_back((*evs)[i]->clone(f));
        return new Xeq(tpos, name, evs2, evaln);
    }

    void generateCode(GeneratorContext *ctx) {
        // Wrapping the subroutine call in another subroutine,
        // so ->PAR can create locals for the parameters without
        // stepping on any already-existing locals with the
        // same name.
        int lbl = ctx->nextLabel();
        ctx->addLine(tpos, CMD_XEQL, lbl);
        ctx->pushSubroutine();
        ctx->addLine(tpos, CMD_LBL, lbl);
        if (!evaln)
            // Start with FUNC 01, so the RPN function can abuse the stack
            // to their heart's content. We only do this for XEQ, not EVALN,
            // because we should be able to assume that generated code is
            // always well-behaved.
            ctx->addLine(tpos, CMD_FUNC, 1);
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->generateCode(ctx);
        ctx->addLine(tpos, (phloat) (int) evs->size());
        if (evaln) {
            ctx->addLine(tpos, CMD_RCL, name);
            ctx->addLine(tpos, CMD_TO_PAR);
            ctx->addLine(tpos, CMD_EVALN, 'L');
        } else {
            ctx->addLine(tpos, CMD_XSTR, name);
            ctx->addLine(tpos, CMD_TO_PAR);
            ctx->addLine(tpos, CMD_XEQ, 'L', true);
        }
        ctx->popSubroutine();
    }

    void collectVariables(std::vector<std::string> *vars, std::vector<std::string> *locals) {
        for (int i = 0; i < evs->size(); i++)
            (*evs)[i]->collectVariables(vars, locals);
    }

    int howMany(const std::string &name) {
        for (int i = 0; i < evs->size(); i++)
            if ((*evs)[i]->howMany(name) != 0)
                return -1;
        return 0;
    }
};

/////////////////
/////  Xor  /////
/////////////////

class Xor : public BinaryEvaluator {

    public:

    Xor(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Xor(tpos, left->clone(f), right->clone(f));
    }

    bool isBool() { return true; }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        right->generateCode(ctx);
        ctx->addLine(tpos, CMD_GEN_XOR);
    }
};

////////////////////
/////  Ycoord  /////
////////////////////

class Ycoord : public BinaryEvaluator {

    public:

    Ycoord(int pos, Evaluator *left, Evaluator *right) : BinaryEvaluator(pos, left, right, false) {}

    Evaluator *clone(For *f) {
        return new Ycoord(tpos, left->clone(f), right == NULL ? NULL : right->clone(f));
    }

    void generateCode(GeneratorContext *ctx) {
        left->generateCode(ctx);
        if (right == NULL) {
            int lbl1 = ctx->nextLabel();
            int lbl2 = ctx->nextLabel();
            ctx->addLine(tpos, CMD_CPX_T);
            ctx->addLine(tpos, CMD_GTOL, lbl1);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, (phloat) 0);
            ctx->addLine(tpos, CMD_GTOL, lbl2);
            ctx->addLine(tpos, CMD_LBL, lbl1);
            ctx->addLine(tpos, CMD_RCOMPLX);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_DROP);
            ctx->addLine(tpos, CMD_LBL, lbl2);
        } else {
            right->generateCode(ctx);
            ctx->addLine(tpos, CMD_SWAP);
            ctx->addLine(tpos, CMD_TO_REC);
            ctx->addLine(tpos, CMD_DROP);
        }
    }
};

/* Methods that can't be defined in their class declarations
 * because they reference other Evaluator classes
 *
 * NOTE: The invert() methods may emit nodes with swapArgs=true, but it doesn't
 * pay attention to swapArgs in the nodes it reads. The idea is that the
 * inverter logic will only be applied to parse trees created from strings, and
 * those will never have swapArgs=true; the trees created by the inverter may
 * contain swapArgs=true here or there, but they will never be fed back into
 * the inverter.
 * Should the need ever arise to apply the inverter to its own output, this
 * would have to be dealt with of course, but that doesn't seem likely.
 */

Evaluator *Evaluator::invert(const std::string &name, Evaluator *rhs) {
    delete rhs;
    return new Abort(tpos);
}

Evaluator *Badd::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Bsub(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Bsub(0, rhs, left->clone(NULL)));
}

Evaluator *Bdiv::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Bmul(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Bdiv(0, rhs, left->clone(NULL), true));
}

Evaluator *Bmul::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Bdiv(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Bdiv(0, rhs, left->clone(NULL)));
}

Evaluator *Bsub::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Badd(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Bsub(0, rhs, left->clone(NULL), true));
}

Evaluator *Bxor::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Bxor(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Bxor(0, rhs, left->clone(NULL)));
}

Evaluator *Date::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Date(0, rhs, new Negative(0, right->clone(NULL))));
    else
        return right->invert(name, new Negative(0, new Ddays(0, rhs, left->clone(NULL), new Literal(0, 1))));
}

Evaluator *Difference::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Sum(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Difference(0, rhs, left->clone(NULL), true));
}

Evaluator *Hmsadd::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Hmssub(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Hmssub(0, rhs, left->clone(NULL)));
}

Evaluator *Hmssub::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Hmsadd(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Hmssub(0, rhs, left->clone(NULL), true));
}

Evaluator *If::invert(const std::string &name, Evaluator *rhs) {
    int t = trueEv->howMany(name);
    int f = falseEv->howMany(name);
    // Note: at least one of t and f must be 1 or we wouldn't be here
    Evaluator *cond = condition->clone(NULL);
    if (t == 1 && f == 1) {
        Subroutine *sub1 = new Subroutine(rhs);
        Subroutine *sub2 = new Subroutine(sub1);
        return new If(0, cond, trueEv->invert(name, sub1), falseEv->invert(name, sub2));
    } else if (t == 1)
        return new If(0, cond, trueEv->invert(name, rhs), new Abort(tpos));
    else
        return new If(0, cond, new Abort(tpos), falseEv->invert(name, rhs));
}

Evaluator *Power::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Power(0, rhs, new UnaryFunction(0, right->clone(NULL), CMD_INV)));
    else
        return right->invert(name, new Quotient(0, new UnaryFunction(0, rhs, CMD_LN), new UnaryFunction(0, left->clone(NULL), CMD_LN)));
}

Evaluator *Product::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Quotient(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Quotient(0, rhs, left->clone(NULL)));
}

Evaluator *Quotient::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Product(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Quotient(0, rhs, left->clone(NULL), true));
}

Evaluator *Sum::invert(const std::string &name, Evaluator *rhs) {
    if (left->howMany(name) == 1)
        return left->invert(name, new Difference(0, rhs, right->clone(NULL)));
    else
        return right->invert(name, new Difference(0, rhs, left->clone(NULL)));
}

Evaluator *Tvm::invert(const std::string &name, Evaluator *rhs) {
    int before, after, new_cmd;
    if ((*evs)[0]->howMany(name) == 1) {
        before = 0;
        switch (cmd) {
            case CMD_GEN_N: new_cmd = CMD_GEN_I; after = 0; break;
            case CMD_GEN_I: new_cmd = CMD_GEN_N; after = 0; break;
            case CMD_GEN_PV: new_cmd = CMD_GEN_N; after = 1; break;
            case CMD_GEN_PMT: new_cmd = CMD_GEN_N; after = 2; break;
            case CMD_GEN_FV: new_cmd = CMD_GEN_N; after = 3; break;
        }
    } else if ((*evs)[1]->howMany(name) == 1) {
        before = 1;
        switch (cmd) {
            case CMD_GEN_N: new_cmd = CMD_GEN_PV; after = 0; break;
            case CMD_GEN_I: new_cmd = CMD_GEN_PV; after = 1; break;
            case CMD_GEN_PV: new_cmd = CMD_GEN_I; after = 1; break;
            case CMD_GEN_PMT: new_cmd = CMD_GEN_I; after = 2; break;
            case CMD_GEN_FV: new_cmd = CMD_GEN_I; after = 3; break;
        }
    } else if ((*evs)[2]->howMany(name) == 1) {
        before = 2;
        switch (cmd) {
            case CMD_GEN_N: new_cmd = CMD_GEN_PMT; after = 0; break;
            case CMD_GEN_I: new_cmd = CMD_GEN_PMT; after = 1; break;
            case CMD_GEN_PV: new_cmd = CMD_GEN_PMT; after = 2; break;
            case CMD_GEN_PMT: new_cmd = CMD_GEN_PV; after = 2; break;
            case CMD_GEN_FV: new_cmd = CMD_GEN_PV; after = 3; break;
        }
    } else {
        before = 3;
        switch (cmd) {
            case CMD_GEN_N: new_cmd = CMD_GEN_FV; after = 0; break;
            case CMD_GEN_I: new_cmd = CMD_GEN_FV; after = 1; break;
            case CMD_GEN_PV: new_cmd = CMD_GEN_FV; after = 2; break;
            case CMD_GEN_PMT: new_cmd = CMD_GEN_FV; after = 3; break;
            case CMD_GEN_FV: new_cmd = CMD_GEN_PMT; after = 3; break;
        }
    }
    std::vector<Evaluator *> *new_evs = new std::vector<Evaluator *>;
    int j = 0;
    for (int i = 0; i < 4; i++) {
        if (i == after) {
            new_evs->push_back(rhs);
        } else {
            if (j == before)
                j++;
            new_evs->push_back((*evs)[j]->clone(NULL));
            j++;
        }
    }
    new_evs->push_back((*evs)[4]->clone(NULL));
    new_evs->push_back((*evs)[5]->clone(NULL));
    return (*evs)[before]->invert(name, new Tvm(0, new_cmd, new_evs));
}

void Break::generateCode(GeneratorContext *ctx) {
    if (f == NULL)
        ctx->addLine(tpos, CMD_XSTR, std::string("BREAK"));
    else
        ctx->addLine(tpos, CMD_GTOL, f->getBreak());
}

void Continue::generateCode(GeneratorContext *ctx) {
    if (f == NULL)
        ctx->addLine(tpos, CMD_XSTR, std::string("CONTINUE"));
    else
        ctx->addLine(tpos, CMD_GTOL, f->getContinue());
}

void Evaluator::getSides(const std::string &name, Evaluator **lhs, Evaluator **rhs) {
    *lhs = this;
    *rhs = NULL;
}

void Evaluator::addIfNew(const std::string &name, std::vector<std::string> *vars, std::vector<std::string> *locals) {
    for (int i = 0; i < locals->size(); i++)
        if ((*locals)[i] == name)
            return;
    for (int i = 0; i < vars->size(); i++)
        if ((*vars)[i] == name)
            return;
    vars->push_back(name);
}

///////////////////
/////  Lexer  /////
///////////////////

class Lexer {

    private:

    std::string text;
    int pos, prevpos;

    public:

    bool compatMode;
    bool compatModeOverridden;

    Lexer(std::string text, bool compatMode) {
        this->text = text;
        this->compatMode = compatMode;
        compatModeOverridden = false;
        pos = 0;
        prevpos = 0;

        std::string t;
        int tpos;
        if (nextToken(&t, &tpos) && t == ":") {
            checkCompatToken();
            if (compatModeOverridden)
                return;
        }
        pos = 0;
        prevpos = 0;
    }

    void reset() {
        pos = 0;
        prevpos = 0;
    }

    void checkCompatToken() {
        int s_pos = pos, s_prevpos = prevpos;
        std::string t;
        int tpos;
        if (nextToken(&t, &tpos) && (t == "STD" || t == "COMP")) {
            bool cm = t == "COMP";
            if (nextToken(&t, &tpos) && t == ":") {
                compatMode = cm;
                compatModeOverridden = true;
                return;
            }
        }
        pos = s_pos;
        prevpos = s_prevpos;
    }

    int lpos() {
        return prevpos;
    }

    int cpos() {
        return pos;
    }

    std::string substring(int start, int end) {
        return text.substr(start, end - start);
    }

    bool isIdentifierStartChar(char c) {
        return !isspace(c) && c != '+' && c != '-' && c != '\1' && c != '\0'
                && c != '^' && c != '\36' && c != '(' && c != ')' && c != '<'
                && c != '>' && c != '=' && c != ':'
                && c != '.' && c != ',' && (c < '0' || c > '9') && c != 24
                && (compatMode
                        || c != '*' && c != '/' && c != '[' && c != ']' && c != '{' && c != '}' && c != '!' && c != '_');
    }

    bool isIdentifierContinuationChar(char c) {
        return c >= '0' && c <= '9' || c == '.' || c == ','
                || isIdentifierStartChar(c);
    }

    bool isIdentifier(const std::string &s) {
        if (s.length() == 0)
            return false;
        if (!isIdentifierStartChar(s[0]))
            return false;
        for (int i = 1; i < s.length(); i++)
            if (!isIdentifierContinuationChar(s[i]))
                return false;
        return true;
    }

    bool nextToken(std::string *tok, int *tpos) {
        prevpos = pos;
        while (pos < text.length() && text[pos] == ' ')
            pos++;
        if (pos == text.length()) {
            *tok = "";
            *tpos = pos;
            return true;
        }
        int start = pos;
        *tpos = start;
        char c = text[pos++];
        // Strings
        if (c == '"') {
            bool complete = false;
            bool esc = false;
            while (pos < text.length()) {
                char c2 = text[pos++];
                if (esc) {
                    esc = false;
                } else {
                    if (c2 == '\\')
                        esc = true;
                    else if (c2 == '"') {
                        complete = true;
                        break;
                    }
                }
            }
            if (complete) {
                *tok = text.substr(start, pos - start);
                return true;
            } else {
                *tok = "";
                return false;
            }
        }
        // Identifiers
        if (isIdentifierStartChar(c)) {
            while (pos < text.length() && isIdentifierContinuationChar(text[pos]))
                pos++;
            *tok = text.substr(start, pos - start);
            return true;
        }
        // Compound symbols
        if (c == '<' || c == '>') {
            if (pos < text.length()) {
                char c2 = text[pos];
                if (c2 == '=' || c == '<' && c2 == '>') {
                    pos++;
                    *tok = text.substr(start, 2);
                    return true;
                }
            }
            *tok = text.substr(start, 1);
            return true;
        }
        if (!compatMode && c == '!') {
            if (pos < text.length() && text[pos] == '=') {
                pos++;
                *tok = std::string("<>");
                return true;
            }
        }
        // One-character symbols
        if (c == '+' || c == '-' || c == '(' || c == ')'
                || c == '^' || c == '\36' || c == ':' || c == '='
                || !compatMode && (c == '*' || c == '/' || c == '[' || c == ']' || c == '{' || c == '}' || c == '_')) {
            *tok = text.substr(start, 1);
            return true;
        }
        switch (c) {
            case '\0': *tok = std::string("/"); return true;
            case '\1': *tok = std::string("*"); return true;
            case '\11': *tok = std::string("<="); return true;
            case '\13': *tok = std::string(">="); return true;
            case '\14': *tok = std::string("<>"); return true;
        }
        // What's left at this point is numbers or garbage.
        // Which one we're currently looking at depends on its
        // first character; if that's a digit or a decimal,
        // it's a number; anything else, it's garbage.
        bool multi_dot = false;
        if (c == '.' || c == ',' || c >= '0' && c <= '9') {
            int state = c == '.' || c == ',' ? 1 : 0;
            int d0 = c == '.' || c == ',' ? 0 : 1;
            int d1 = 0, d2 = 0;
            while (pos < text.length()) {
                c = text[pos];
                switch (state) {
                    case 0:
                        if (c == '.' || c == ',')
                            state = 1;
                        else if (c == 'E' || c == 'e' || c == 24)
                            state = 2;
                        else if (c >= '0' && c <= '9')
                            d0++;
                        else
                            goto done;
                        break;
                    case 1:
                        if (c == '.' || c == ',') {
                            multi_dot = true;
                            goto done;
                        }
                        else if (c == 'E' || c == 'e' || c == 24)
                            state = 2;
                        else if (c >= '0' && c <= '9')
                            d1++;
                        else
                            goto done;
                        break;
                    case 2:
                        if (c == '-' || c == '+')
                            state = 3;
                        else if (c >= '0' && c <= '9') {
                            d2++;
                            state = 3;
                        } else
                            goto done;
                        break;
                    case 3:
                        if (c >= '0' && c <= '9')
                            d2++;
                        else
                            goto done;
                        break;
                }
                pos++;
            }
            done:
            // Invalid number scenarios:
            if (d0 == 0 && d1 == 0 // A dot not followed by a digit.
                    || multi_dot // Multiple periods
                    || state == 2  // An 'E' not followed by a valid character.
                    || state == 3 && d2 == 0) { // An 'E' not followed by at least one digit
                *tok = "";
                return false;
            }
            *tok = text.substr(start, pos - start);
            return true;
        } else {
            // Garbage; return just the one character.
            // Parsing will fail at this point so no need to do anything clever.
            *tok = text.substr(start, 1);
            return true;
        }
    }
};

////////////////////
/////  Parser  /////
////////////////////

#define CTX_TOP 0
#define CTX_VALUE 1
#define CTX_BOOLEAN 2
#define CTX_ARRAY 3

/* static */ Evaluator *Parser::parse(std::string expr, bool *compatMode, bool *compatModeOverridden, int *errpos) {
    try {
        bool savedCompatMode = *compatMode;
        bool noName;
        Evaluator *ev = parse2(expr, &noName, compatMode, compatModeOverridden, errpos);
        if (ev != NULL || noName)
            return ev;
        // If parsing failed, try again without looking for a name. This is to
        // support cases like [1:2:3]=A, where the initial [1 part gets mis-
        // identified as an equation name. If this second attempt also fails,
        // report whichever errpos is higher, on the assumption that whichever
        // assumption allows the parser to get farthest into the expression is
        // most likely to be the correct one.
        int ep1 = *errpos;
        *compatMode = savedCompatMode;
        ev = parse2(expr, NULL, compatMode, compatModeOverridden, errpos);
        if (ev != NULL)
            return ev;
        if (ep1 > *errpos)
            *errpos = ep1;
        return NULL;
    } catch (std::bad_alloc &) {
        *errpos = -1;
        return NULL;
    }
}

/* static */ Evaluator *Parser::parse2(std::string expr, bool *noName, bool *compatMode, bool *compatModeOverridden, int *errpos) {
    std::string t, t2, eqnName;
    std::vector<std::string> *paramNames = NULL;
    int tpos;

    // Look for equation name
    Lexer *lex = new Lexer(expr, *compatMode);
    if (noName != NULL)
        // If lex->compatModeOverridden is set before we've even done any
        // parsing, this means that the equation starts with :STD: or :COMP:.
        // This means that the second parsing attempt (see the comment in
        // parse(), above), where we try parsing the equation again but without
        // looking for a name, can be skipped, since we already know that there
        // is no name.
        *noName = lex->compatModeOverridden;
    if (noName == NULL || lex->compatModeOverridden)
        goto name_done;
    lex->compatMode = true;
    paramNames = new std::vector<std::string>;
    if (!lex->nextToken(&t, &tpos))
        goto no_name;
    if (!lex->isIdentifier(t))
        goto no_name;
    if (!lex->nextToken(&t2, &tpos))
        goto no_name;
    if (t2 != ":" && t2 != "(")
        goto no_name;
    if (t2 == "(") {
        while (true) {
            if (!lex->nextToken(&t2, &tpos))
                goto no_name;
            if (!lex->isIdentifier(t2))
                goto no_name;
            paramNames->push_back(t2);
            if (!lex->nextToken(&t2, &tpos))
                goto no_name;
            if (t2 == ":")
                continue;
            else if (t2 == ")") {
                if (!lex->nextToken(&t2, &tpos))
                    goto no_name;
                if (t2 == ":")
                    break;
                else
                    goto no_name;
            } else
                goto no_name;
        }
    }

    if (paramNames->size() == 0) {
        delete paramNames;
        paramNames = NULL;
    }
    lex->compatMode = *compatMode;
    lex->checkCompatToken();
    eqnName = t;
    goto name_done;

    no_name:
    lex->reset();
    lex->compatMode = *compatMode;
    delete paramNames;
    paramNames = NULL;
    name_done:

    Parser pz(lex);
    Evaluator *ev = pz.parseExpr(CTX_TOP);
    if (ev == NULL) {
        fail:
        *errpos = pz.lex->lpos();
        return NULL;
    }
    if (!pz.nextToken(&t, &tpos)) {
        delete ev;
        goto fail;
    }
    if (t == "") {
        // Text consumed completely; this is the good scenario
        if (eqnName != "")
            ev = new NameTag(0, eqnName, paramNames, ev);
        *compatMode = lex->compatMode;
        *compatModeOverridden = lex->compatModeOverridden;
        return ev;
    } else {
        // Trailing garbage
        delete ev;
        *errpos = tpos;
        return NULL;
    }
}

/* static */ void Parser::generateCode(Evaluator *ev, prgm_struct *prgm, CodeMap *map) {
    try {
        GeneratorContext ctx;
        ev->generateCode(&ctx);
        ctx.store(prgm, map);
    } catch (std::bad_alloc &) {
        free(prgm->text);
        prgm->text = NULL;
    }
}

Parser::Parser(Lexer *lex) : lex(lex), pbpos(-1) {}

Parser::~Parser() {
    delete lex;
}

Evaluator *Parser::parseExpr(int context) {
    int old_context = this->context;
    this->context = context;
    Evaluator *ret = parseExpr2();
    this->context = old_context;
    return ret;
}

Evaluator *Parser::parseExpr2() {
    Evaluator *ev = parseAnd();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "")
            return ev;
        if (t == "OR" || t == "XOR") {
            if (context != CTX_BOOLEAN || !ev->isBool())
                goto fail;
            Evaluator *ev2 = parseAnd();
            if (ev2 == NULL)
                goto fail;
            if (!ev2->isBool()) {
                delete ev2;
                goto fail;
            }
            if (t == "OR")
                ev = new Or(tpos, ev, ev2);
            else // t == "XOR"
                ev = new Xor(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

Evaluator *Parser::parseAnd() {
    Evaluator *ev = parseNot();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "")
            return ev;
        if (t == "AND") {
            if (context != CTX_BOOLEAN || !ev->isBool())
                goto fail;
            Evaluator *ev2 = parseNot();
            if (ev2 == NULL)
                goto fail;
            if (!ev2->isBool()) {
                delete ev2;
                goto fail;
            }
            ev = new And(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

Evaluator *Parser::parseNot() {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    if (t == "NOT") {
        Evaluator *ev = parseComparison();
        if (ev == NULL) {
            return NULL;
        } else if (context != CTX_BOOLEAN || !ev->isBool()) {
            delete ev;
            return NULL;
        } else {
            return new Not(tpos, ev);
        }
    } else {
        pushback(t, tpos);
        return parseComparison();
    }
}

Evaluator *Parser::parseComparison() {
    Evaluator *ev = parseNumExpr();
    if (ev == NULL)
        return NULL;
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos)) {
        fail:
        delete ev;
        return NULL;
    }
    if (t == "")
        return ev;
    if (context == CTX_TOP && t == "=") {
        if (ev->isBool())
            goto fail;
        context = CTX_VALUE; // Only one '=' allowed
        Evaluator *ev2 = parseNumExpr();
        if (ev2 == NULL)
            goto fail;
        if (ev2->isBool()) {
            delete ev2;
            goto fail;
        }
        return new Equation(tpos, ev, ev2);
    } else if (t == "=" || t == "<>" || t == "<" || t == "<=" || t == ">" || t == ">=") {
        if (context != CTX_BOOLEAN || ev->isBool())
            goto fail;
        Evaluator *ev2 = parseNumExpr();
        if (ev2 == NULL)
            goto fail;
        if (ev2->isBool()) {
            delete ev2;
            goto fail;
        }
        if (t == "=")
            return new CompareEQ(tpos, ev, ev2);
        else if (t == "<>")
            return new CompareNE(tpos, ev, ev2);
        else if (t == "<")
            return new CompareLT(tpos, ev, ev2);
        else if (t == "<=")
            return new CompareLE(tpos, ev, ev2);
        else if (t == ">")
            return new CompareGT(tpos, ev, ev2);
        else // t == ">="
            return new CompareGE(tpos, ev, ev2);
    } else {
        pushback(t, tpos);
        return ev;
    }
}

Evaluator *Parser::parseNumExpr() {
    Evaluator *ev = parseTerm();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "")
            return ev;
        if (t == "+" || t == "-") {
            if (ev->isBool())
                goto fail;
            Evaluator *ev2 = parseTerm();
            if (ev2 == NULL)
                goto fail;
            if (ev2->isBool()) {
                delete ev2;
                return NULL;
            }
            if (t == "+")
                ev = new Sum(tpos, ev, ev2);
            else
                ev = new Difference(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

Evaluator *Parser::parseTerm() {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    if (t == "-" || t == "+") {
        Evaluator *ev = parseTerm();
        if (ev == NULL)
            return NULL;
        if (ev->isBool()) {
            delete ev;
            return NULL;
        }
        if (t == "+")
            return ev;
        else
            return new Negative(tpos, ev);
    } else {
        pushback(t, tpos);
        Evaluator *ev = parseFactor();
        if (ev == NULL)
            return NULL;
        while (true) {
            if (!nextToken(&t, &tpos))
                goto fail;
            if (t == "")
                return ev;
            if (t == "*" || t == "/") {
                if (ev->isBool()) {
                    fail:
                    delete ev;
                    return NULL;
                }
                Evaluator *ev2 = parseFactor();
                if (ev2 == NULL)
                    goto fail;
                if (ev2->isBool()) {
                    delete ev2;
                    goto fail;
                }
                if (t == "*")
                    ev = new Product(tpos, ev, ev2);
                else
                    ev = new Quotient(tpos, ev, ev2);
            } else {
                pushback(t, tpos);
                return ev;
            }
        }
    }
}

Evaluator *Parser::parseFactor() {
    Evaluator *ev = parseThing();
    if (ev == NULL)
        return NULL;
    while (true) {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos)) {
            fail:
            delete ev;
            return NULL;
        }
        if (t == "^" || t == "\36" || t == "_") {
            if (ev->isBool())
                goto fail;
            Evaluator *ev2 = parseThing();
            if (ev2 == NULL)
                goto fail;
            if (ev2->isBool()) {
                delete ev2;
                goto fail;
            }
            if (t == "^" || t == "\36")
                ev = new Power(tpos, ev, ev2);
            else
                ev = new Unit(tpos, ev, ev2);
        } else {
            pushback(t, tpos);
            return ev;
        }
    }
}

#define EXPR_LIST_EXPR 0
#define EXPR_LIST_BOOLEAN 1
#define EXPR_LIST_NAME 2
#define EXPR_LIST_SUBEXPR 3
#define EXPR_LIST_LVALUE 4
#define EXPR_LIST_FOR 5

std::vector<Evaluator *> *Parser::parseExprList(int min_args, int max_args, int mode) {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    pushback(t, tpos);
    std::vector<Evaluator *> *evs = new std::vector<Evaluator *>;
    if (t == ")") {
        if (min_args == 0) {
            return evs;
        } else {
            fail:
            for (int i = 0; i < evs->size(); i++)
                delete (*evs)[i];
            delete evs;
            return NULL;
        }
    } else {
        pushback(t, tpos);
    }

    while (true) {
        Evaluator *ev;
        if (mode == EXPR_LIST_NAME) {
            if (!nextToken(&t, &tpos) || t == "")
                goto fail;
            if (!lex->isIdentifier(t))
                goto fail;
            ev = new Variable(tpos, t);
            mode = EXPR_LIST_EXPR;
        } else if (mode == EXPR_LIST_LVALUE) {
            // Possibilites are: name, name[index], or ITEM(name:index)
            if (!nextToken(&t, &tpos) || t == "")
                goto fail;
            if (!lex->isIdentifier(t))
                goto fail;
            if (t == "ITEM") {
                std::string t2;
                int t2pos;
                if (!nextToken(&t2, &t2pos) || t2 != "(")
                    goto fail;
                std::vector<Evaluator *> *evs = parseExprList(2, 3, EXPR_LIST_NAME);
                if (!nextToken(&t2, &t2pos) || t2 != ")") {
                    for (int i = 0; i < evs->size(); i++)
                        delete (*evs)[i];
                    delete evs;
                    goto fail;
                }
                Evaluator *name = (*evs)[0];
                Evaluator *ev1 = (*evs)[1];
                Evaluator *ev2 = evs->size() == 3 ? (*evs)[2] : NULL;
                delete evs;
                std::string n = name->name();
                delete name;
                Item *item = new Item(tpos, n, ev1, ev2);
                item->makeLvalue();
                ev = item;
            } else {
                std::string t2;
                int t2pos;
                if (!nextToken(&t2, &t2pos) || t2 == "")
                    goto fail;
                if (t2 == ":") {
                    pushback(t2, t2pos);
                    ev = new Variable(tpos, t);
                } else if (lex->compatMode || t2 != "[") {
                    goto fail;
                } else {
                    Evaluator *ev1 = parseExpr(CTX_VALUE);
                    Evaluator *ev2 = NULL;
                    if (ev1 == NULL)
                        goto fail;
                    if (!nextToken(&t2, &t2pos)) {
                        item_fail:
                        delete ev1;
                        goto fail;
                    }
                    if (t2 == ":") {
                        ev2 = parseExpr(CTX_VALUE);
                        if (ev2 == NULL)
                            goto item_fail;
                        if (!nextToken(&t2, &t2pos)) {
                            item2_fail:
                            delete ev1;
                            delete ev2;
                            return NULL;
                        }
                    }
                    if (t2 != "]")
                        goto item2_fail;
                    Item *item = new Item(tpos, t, ev1, ev2);
                    item->makeLvalue();
                    ev = item;
                }
            }
            mode = EXPR_LIST_EXPR;
        } else {
            bool wantBool = mode == EXPR_LIST_BOOLEAN;
            int startPos = pbpos != -1 ? pbpos : lex->cpos();
            ev = parseExpr(wantBool ? CTX_BOOLEAN : CTX_VALUE);
            if (ev == NULL)
                goto fail;
            if (wantBool != ev->isBool()) {
                delete ev;
                goto fail;
            }
            if (mode == EXPR_LIST_SUBEXPR) {
                int endPos = pbpos != -1 ? pbpos : lex->cpos();
                std::string text = lex->substring(startPos, endPos);
                ev = new Subexpression(startPos, ev, text);
                mode = EXPR_LIST_NAME;
            } else if (mode == EXPR_LIST_FOR) {
                mode = EXPR_LIST_BOOLEAN;
            } else {
                mode = EXPR_LIST_EXPR;
            }
        }
        evs->push_back(ev);
        if (!nextToken(&t, &tpos))
            goto fail;
        if (t == ":") {
            if (evs->size() == max_args)
                goto fail;
        } else {
            pushback(t, tpos);
            if (t == ")" && evs->size() >= min_args)
                return evs;
            else
                goto fail;
        }
    }
}

static bool get_phloat(std::string tok, phloat *d) {
    char c = tok[0];
    if ((c < '0' || c > '9') && c != '.' && c != ',')
        return false;
    char d1 = flags.f.decimal_point ? ',' : '.';
    char d2 = flags.f.decimal_point ? '.' : ',';
    for (int i = 0; i < tok.length(); i++)
        if (tok[i] == 'E' || tok[i] == 'e')
            tok[i] = 24;
        else if (tok[i] == d1)
            tok[i] = d2;
    return string2phloat(tok.c_str(), (int) tok.length(), d) == 0;
}

static std::string get_string(std::string tok) {
    std::string res;
    int n = (int) tok.length() - 1;
    for (int i = 1; i < n; i++) {
        char c = tok[i];
        if (c == '\\') {
            if (++i == n)
                // Shouldn't happen, because this would imply
                // EOT in the middle of the string, and that
                // should have caused the lexer to error out.
                break;
            c = tok[i];
        }
        res.append(1, c);
    }
    return res;
}

Evaluator *Parser::parseThing() {
    std::string t;
    int tpos;
    if (!nextToken(&t, &tpos) || t == "")
        return NULL;
    if (t == "-" || t == "+") {
        Evaluator *ev = parseThing();
        if (ev == NULL)
            return NULL;
        if (ev->isBool()) {
            delete ev;
            return NULL;
        }
        if (t == "+")
            return ev;
        else
            return new Negative(tpos, ev);
    }
    phloat d;
    if (t[0] == '"') {
        return new String(tpos, get_string(t));
    } else if (get_phloat(t, &d)) {
        return new Literal(tpos, d);
    } else if (t == "(") {
        Evaluator *ev = parseExpr(context == CTX_TOP ? CTX_VALUE : context);
        if (ev == NULL)
            return NULL;
        std::string t2;
        int t2pos;
        if (!nextToken(&t2, &t2pos) || t2 != ")") {
            delete ev;
            return NULL;
        }
        return ev;
    } else if (!lex->compatMode && t == "{") {
        // List literal
        int lpos = tpos;
        if (!nextToken(&t, &tpos))
            return NULL;
        std::vector<Evaluator *> list;
        if (t == "}")
            return new List(lpos, list);
        pushback(t, tpos);
        For f(-1);
        forStack.push_back(&f);
        while (true) {
            Evaluator *ev = parseExpr(CTX_VALUE);
            if (ev == NULL) {
                list_fail:
                for (int i = 0; i < list.size(); i++)
                    delete list[i];
                forStack.pop_back();
                return NULL;
            }
            list.push_back(ev);
            if (!nextToken(&t, &tpos))
                goto list_fail;
            if (t == "}") {
                forStack.pop_back();
                return new List(lpos, list);
            }
            if (t != ":")
                goto list_fail;
        }
    } else if (!lex->compatMode && t == "[" && context != CTX_ARRAY) {
        // Array literal
        int apos = tpos;
        if (!nextToken(&t, &tpos))
            return NULL;
        bool one_d = t != "[";
        if (one_d)
            pushback(t, tpos);
        int width = 0;
        std::vector<std::vector<Evaluator *> > data;
        std::vector<Evaluator *> row;
        For f(-1);
        forStack.push_back(&f);
        while (true) {
            if (!nextToken(&t, &tpos))
                goto array_fail;
            if (t == "]") {
                end_row:
                int w = (int) row.size();
                if (w == 0)
                    goto array_fail;
                if (width < w)
                    width = w;
                data.push_back(row);
                row.clear();
                if (one_d)
                    goto array_success;
                if (!nextToken(&t, &tpos))
                    goto array_fail;
                if (t == "]")
                    goto array_success;
                if (t != ":")
                    goto array_fail;
                if (!nextToken(&t, &tpos))
                    goto array_fail;
                if (t != "[")
                    goto array_fail;
            } else {
                pushback(t, tpos);
                do_element:
                Evaluator *ev = parseExpr(CTX_ARRAY);
                if (ev == NULL)
                    goto array_fail;
                row.push_back(ev);
                if (!nextToken(&t, &tpos))
                    goto array_fail;
                if (t == "]")
                    goto end_row;
                if (t != ":")
                    goto array_fail;
                goto do_element;
            }
        }
        array_fail:
        forStack.pop_back();
        for (int i = 0; i < data.size(); i++)
            for (int j = 0; j < data[i].size(); j++)
                delete data[i][j];
        return NULL;
        array_success:
        forStack.pop_back();
        return new Array(apos, data, one_d);
    } else if (lex->isIdentifier(t)) {
        std::string t2;
        int t2pos;
        if (!nextToken(&t2, &t2pos))
            return NULL;
        if (t2 == "(") {
            int min_args, max_args;
            int mode;
            if (t == "SIN" || t == "COS" || t == "TAN"
                    || t == "ASIN" || t == "ACOS" || t == "ATAN"
                    || t == "SINH" || t == "COSH" || t == "TANH"
                    || t == "ASINH" || t == "ACOSH" || t == "ATANH"
                    || t == "DEG" || t == "RAD"
                    || t == "LN" || t == "LNP1" || t == "LOG"
                    || t == "EXP" || t == "EXPM1" || t == "ALOG"
                    || t == "SQRT" || t == "SQ" || t == "INV"
                    || t == "ABS" || t == "FACT" || t == "GAMMA"
                    || t == "INT" || t == "IP" || t == "FP"
                    || t == "HMS" || t == "HRS" || t == "SIZES"
                    || t == "MROWS" || t == "MCOLS" || t == "SIZEC"
                    || t == "SGN" || t == "DEC" || t == "OCT"
                    || t == "BNOT" || t == "BNEG"
                    || t == "INVRT" || t == "DET" || t == "TRANS"
                    || t == "UVEC" || t == "FNRM" || t == "RNRM"
                    || t == "RSUM" || t == "REAL?" || t == "CPX?"
                    || t == "MAT?" || t == "CPXMAT?" || t == "STR?"
                    || t == "LIST?" || t == "EQN?" || t == "UNIT?"
                    || t == "TYPE?"
                    || t == "UBASE" || t == "UVAL" || t == "STOP"
                    || t == "FCSTX" || t == "FCSTY"
                    || t == "HEAD" || t == "TAIL" || t == "LENGTH"
                    || t == "REV" || t == "S\17N" || t == "N\17S"
                    || t == "NN\17S" || t == "C\17N" || t == "N\17C") {
                min_args = max_args = 1;
                mode = EXPR_LIST_EXPR;
            } else if (t == "COMB" || t == "PERM"
                    || t == "IDIV" || t == "MOD" || t == "RND"
                    || t == "TRN" || t == "DATE" || t == "BAND"
                    || t == "BOR" || t == "BXOR" || t == "BADD"
                    || t == "BSUB" || t == "BMUL" || t == "BDIV"
                    || t == "HMSADD" || t == "HMSSUB"
                    || t == "NEWMAT" || t == "DOT" || t == "CROSS"
                    || t == "RCOMPLX" || t == "PCOMPLX" || t == "SPPV"
                    || t == "SPFV" || t == "USPV" || t == "USFV"
                    || t == "UNIT" || t == "EXTEND") {
                min_args = max_args = 2;
                mode = EXPR_LIST_EXPR;
            } else if (t == "ANGLE" || t == "RADIUS" || t == "XCOORD"
                    || t == "YCOORD") {
                min_args = 1;
                max_args = 2;
                mode = EXPR_LIST_EXPR;
            } else if (t == "DDAYS") {
                min_args = max_args = 3;
                mode = EXPR_LIST_EXPR;
            } else if (t == "MIN" || t == "MAX") {
                min_args = 0;
                max_args = INT_MAX;
                mode = EXPR_LIST_EXPR;
            } else if (t == "APPEND") {
                min_args = 2;
                max_args = INT_MAX;
                mode = EXPR_LIST_EXPR;
            } else if (t == "IF") {
                min_args = max_args = 3;
                mode = EXPR_LIST_BOOLEAN;
            } else if (t == "G" || t == "S") {
                min_args = max_args = 1;
                mode = EXPR_LIST_NAME;
            } else if (t == "L") {
                min_args = max_args = 2;
                mode = EXPR_LIST_LVALUE;
            } else if (t == "LL") {
                min_args = 3;
                max_args = INT_MAX;
                mode = EXPR_LIST_NAME;
            } else if (t == "ITEM") {
                min_args = 2;
                max_args = 3;
                mode = EXPR_LIST_NAME;
            } else if (t == "POS" || t == "SUBSTR") {
                min_args = 2;
                max_args = 3;
                mode = EXPR_LIST_EXPR;
            } else if (t == "FLOW" || t == "#T") {
                min_args = max_args = 2;
                mode = EXPR_LIST_NAME;
            } else if (t == "FOR") {
                min_args = 4;
                max_args = INT_MAX;
                mode = EXPR_LIST_FOR;
                For *f = new For(tpos);
                forStack.push_back(f);
            } else if (t == "\5") {
                min_args = max_args = 5;
                mode = EXPR_LIST_NAME;
                For *f = new For(-1);
                forStack.push_back(f);
            } else if (t == "\3") {
                min_args = 4;
                max_args = 5;
                mode = EXPR_LIST_SUBEXPR;
                For *f = new For(-1);
                forStack.push_back(f);
            } else if (t == "N" || t == "I%YR" || t == "PV"
                    || t == "PMT" || t == "FV") {
                min_args = max_args = 6;
                mode = EXPR_LIST_EXPR;
            } else if (t == "XEQ" || t == "EVALN") {
                min_args = 1;
                max_args = INT_MAX;
                mode = EXPR_LIST_NAME;
            } else if (t == "SEQ" || t == "VIEW") {
                min_args = 1;
                max_args = INT_MAX;
                mode = EXPR_LIST_EXPR;
            } else {
                // Call
                min_args = 0;
                max_args = INT_MAX;
                mode = EXPR_LIST_EXPR;
            }
            std::vector<Evaluator *> *evs = parseExprList(min_args, max_args, mode);
            if (t == "\5" || t == "\3") {
                delete forStack.back();
                forStack.pop_back();
            }
            For *f = NULL;
            if (t == "FOR") {
                f = forStack.back();
                forStack.pop_back();
            }
            if (evs == NULL) {
                delete f;
                return NULL;
            }
            if (!nextToken(&t2, &t2pos) || t2 != ")") {
                for (int i = 0; i < evs->size(); i++)
                    delete (*evs)[i];
                delete evs;
                delete f;
                return NULL;
            }
            if (t == "SIN" || t == "COS" || t == "TAN"
                    || t == "ASIN" || t == "ACOS" || t == "ATAN"
                    || t == "SINH" || t == "COSH" || t == "TANH"
                    || t == "ASINH" || t == "ACOSH" || t == "ATANH"
                    || t == "DEG" || t == "RAD"
                    || t == "LN" || t == "LNP1" || t == "LOG"
                    || t == "EXP" || t == "EXPM1" || t == "ALOG"
                    || t == "SQRT" || t == "SQ" || t == "INV"
                    || t == "ABS" || t == "FACT" || t == "GAMMA"
                    || t == "INT" || t == "IP" || t == "FP"
                    || t == "HMS" || t == "HRS" || t == "SIZES"
                    || t == "MROWS" || t == "MCOLS" || t == "SIZEC"
                    || t == "SGN" || t == "DEC" || t == "OCT"
                    || t == "BNOT" || t == "BNEG"
                    || t == "INVRT" || t == "DET" || t == "TRANS"
                    || t == "UVEC" || t == "FNRM" || t == "RNRM"
                    || t == "RSUM" || t == "REAL?" || t == "CPX?"
                    || t == "MAT?" || t == "CPXMAT?" || t == "STR?"
                    || t == "LIST?" || t == "EQN?" || t == "UNIT?"
                    || t == "TYPE?"
                    || t == "UBASE" || t == "UVAL" || t == "STOP"
                    || t == "FCSTX" || t == "FCSTY"
                    || t == "HEAD" || t == "TAIL" || t == "LENGTH"
                    || t == "REV" || t == "S\17N" || t == "N\17S"
                    || t == "NN\17S" || t == "C\17N" || t == "N\17C") {
                Evaluator *ev = (*evs)[0];
                delete evs;
                if (t == "SIN")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_SIN, CMD_ASIN);
                else if (t == "COS")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_COS, CMD_ACOS);
                else if (t == "TAN")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TAN, CMD_ATAN);
                else if (t == "ASIN")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_ASIN, CMD_SIN);
                else if (t == "ACOS")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_ACOS, CMD_COS);
                else if (t == "ATAN")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_ATAN, CMD_TAN);
                else if (t == "SINH")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_SINH, CMD_ASINH);
                else if (t == "COSH")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_COSH, CMD_ACOSH);
                else if (t == "TANH")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TANH, CMD_ATANH);
                else if (t == "ASINH")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_ASINH, CMD_SINH);
                else if (t == "ACOSH")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_ACOSH, CMD_COSH);
                else if (t == "ATANH")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_ATANH, CMD_TANH);
                else if (t == "DEG")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TO_DEG, CMD_TO_RAD);
                else if (t == "RAD")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TO_RAD, CMD_TO_DEG);
                else if (t == "LN")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_LN, CMD_E_POW_X);
                else if (t == "LNP1")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_LN_1_X, CMD_E_POW_X_1);
                else if (t == "LOG")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_LOG, CMD_10_POW_X);
                else if (t == "EXP")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_E_POW_X, CMD_LN);
                else if (t == "EXPM1")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_E_POW_X_1, CMD_LN_1_X);
                else if (t == "ALOG")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_10_POW_X, CMD_LOG);
                else if (t == "SQ")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_SQUARE, CMD_SQRT);
                else if (t == "SQRT")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_SQRT, CMD_SQUARE);
                else if (t == "INV")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_INV, CMD_INV);
                else if (t == "ABS")
                    return new UnaryFunction(tpos, ev, CMD_ABS);
                else if (t == "FACT")
                    return new UnaryFunction(tpos, ev, CMD_FACT);
                else if (t == "GAMMA")
                    return new UnaryFunction(tpos, ev, CMD_GAMMA);
                else if (t == "INT")
                    return new Int(tpos, ev);
                else if (t == "IP")
                    return new UnaryFunction(tpos, ev, CMD_IP);
                else if (t == "FP")
                    return new UnaryFunction(tpos, ev, CMD_FP);
                else if (t == "HMS")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TO_HMS, CMD_TO_HR);
                else if (t == "HRS")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TO_HR, CMD_TO_HMS);
                else if (t == "SIZES")
                    return new Size(tpos, ev, 'S');
                else if (t == "MROWS")
                    return new Size(tpos, ev, 'R');
                else if (t == "MCOLS")
                    return new Size(tpos, ev, 'C');
                else if (t == "SIZEC")
                    return new SizeC(tpos, ev);
                else if (t == "SGN")
                    return new Sgn(tpos, ev);
                else if (t == "DEC")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TO_DEC, CMD_TO_OCT);
                else if (t == "OCT")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_TO_OCT, CMD_TO_DEC);
                else if (t == "BNOT")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_NOT, CMD_NOT);
                else if (t == "BNEG")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_BASECHS, CMD_BASECHS);
                else if (t == "INVRT")
                    return new UnaryFunction(tpos, ev, CMD_INVRT);
                else if (t == "DET")
                    return new UnaryFunction(tpos, ev, CMD_DET);
                else if (t == "TRANS")
                    return new UnaryFunction(tpos, ev, CMD_TRANS);
                else if (t == "UVEC")
                    return new UnaryFunction(tpos, ev, CMD_UVEC);
                else if (t == "FNRM")
                    return new UnaryFunction(tpos, ev, CMD_FNRM);
                else if (t == "RNRM")
                    return new UnaryFunction(tpos, ev, CMD_RNRM);
                else if (t == "RSUM")
                    return new UnaryFunction(tpos, ev, CMD_RSUM);
                else if (t == "REAL?")
                    return new TypeTest(tpos, ev, CMD_REAL_T);
                else if (t == "CPX?")
                    return new TypeTest(tpos, ev, CMD_CPX_T);
                else if (t == "CPXMAT?")
                    return new TypeTest(tpos, ev, CMD_CPXMAT_T);
                else if (t == "STR?")
                    return new TypeTest(tpos, ev, CMD_STR_T);
                else if (t == "MAT?")
                    return new TypeTest(tpos, ev, CMD_MAT_T);
                else if (t == "LIST?")
                    return new TypeTest(tpos, ev, CMD_LIST_T);
                else if (t == "EQN?")
                    return new TypeTest(tpos, ev, CMD_EQN_T);
                else if (t == "UNIT?")
                    return new TypeTest(tpos, ev, CMD_UNIT_T);
                else if (t == "TYPE?")
                    return new UnaryFunction(tpos, ev, CMD_TYPE_T);
                else if (t == "UBASE")
                    return new UnaryFunction(tpos, ev, CMD_UBASE);
                else if (t == "UVAL")
                    return new UnaryFunction(tpos, ev, CMD_UVAL);
                else if (t == "STOP")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_STOP, CMD_STOP);
                else if (t == "FCSTX")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_FCSTX, CMD_FCSTY);
                else if (t == "FCSTY")
                    return new InvertibleUnaryFunction(tpos, ev, CMD_FCSTY, CMD_FCSTX);
                else if (t == "HEAD")
                    return new HeadOrTail(tpos, ev, true);
                else if (t == "TAIL")
                    return new HeadOrTail(tpos, ev, false);
                else if (t == "LENGTH")
                    return new UnaryFunction(tpos, ev, CMD_LENGTH);
                else if (t == "REV")
                    return new UnaryFunction(tpos, ev, CMD_REV);
                else if (t == "S\17N")
                    return new UnaryFunction(tpos, ev, CMD_S_TO_N);
                else if (t == "N\17S")
                    return new UnaryFunction(tpos, ev, CMD_N_TO_S);
                else if (t == "NN\17S")
                    return new UnaryFunction(tpos, ev, CMD_NN_TO_S);
                else if (t == "C\17N")
                    return new UnaryFunction(tpos, ev, CMD_C_TO_N);
                else if (t == "N\17C")
                    return new UnaryFunction(tpos, ev, CMD_N_TO_C);
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "COMB" || t == "PERM"
                    || t == "IDIV" || t == "MOD" || t == "RND"
                    || t == "TRN" || t == "DATE" || t == "BAND"
                    || t == "BOR" || t == "BXOR" || t == "BADD"
                    || t == "BSUB" || t == "BMUL" || t == "BDIV"
                    || t == "HMSADD" || t == "HMSSUB"
                    || t == "NEWMAT" || t == "DOT" || t == "CROSS"
                    || t == "RCOMPLX" || t == "PCOMPLX" || t == "SPPV"
                    || t == "SPFV" || t == "USPV" || t == "USFV"
                    || t == "UNIT" || t == "EXTEND") {
                Evaluator *left = (*evs)[0];
                Evaluator *right = (*evs)[1];
                delete evs;
                if (t == "COMB")
                    return new Comb(tpos, left, right);
                else if (t == "PERM")
                    return new Perm(tpos, left, right);
                else if (t == "IDIV")
                    return new Idiv(tpos, left, right);
                else if (t == "MOD")
                    return new Mod(tpos, left, right);
                else if (t == "RND")
                    return new Rnd(tpos, left, right, false);
                else if (t == "TRN")
                    return new Rnd(tpos, left, right, true);
                else if (t == "DATE")
                    return new Date(tpos, left, right);
                else if (t == "BAND")
                    return new Band(tpos, left, right);
                else if (t == "BOR")
                    return new Bor(tpos, left, right);
                else if (t == "BXOR")
                    return new Bxor(tpos, left, right);
                else if (t == "BADD")
                    return new Badd(tpos, left, right);
                else if (t == "BSUB")
                    return new Bsub(tpos, left, right);
                else if (t == "BMUL")
                    return new Bmul(tpos, left, right);
                else if (t == "BDIV")
                    return new Bdiv(tpos, left, right);
                else if (t == "HMSADD")
                    return new Hmsadd(tpos, left, right);
                else if (t == "HMSSUB")
                    return new Hmssub(tpos, left, right);
                else if (t == "NEWMAT")
                    return new Newmat(tpos, left, right);
                else if (t == "DOT")
                    return new Dot(tpos, left, right);
                else if (t == "CROSS")
                    return new Cross(tpos, left, right);
                else if (t == "RCOMPLX")
                    return new Rcomplx(tpos, left, right);
                else if (t == "PCOMPLX")
                    return new Pcomplx(tpos, left, right);
                else if (t == "SPPV")
                    return new BinaryFunction(tpos, left, right, CMD_SPPV);
                else if (t == "SPFV")
                    return new BinaryFunction(tpos, left, right, CMD_SPFV);
                else if (t == "USPV")
                    return new BinaryFunction(tpos, left, right, CMD_USPV);
                else if (t == "USFV")
                    return new BinaryFunction(tpos, left, right, CMD_USFV);
                else if (t == "UNIT")
                    return new Unit(tpos, left, right);
                else if (t == "EXTEND")
                    return new BinaryFunction(tpos, left, right, CMD_EXTEND);
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "ANGLE" || t == "RADIUS" || t == "XCOORD"
                    || t == "YCOORD") {
                Evaluator *left = (*evs)[0];
                Evaluator *right = evs->size() > 1 ? (*evs)[1] : NULL;
                delete evs;
                if (t == "ANGLE")
                    return new Angle(tpos, left, right);
                else if (t == "RADIUS")
                    return new Radius(tpos, left, right);
                else if (t == "XCOORD")
                    return new Xcoord(tpos, left, right);
                else if (t == "YCOORD")
                    return new Ycoord(tpos, left, right);
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "DDAYS") {
                Evaluator *date1 = (*evs)[0];
                Evaluator *date2 = (*evs)[1];
                Evaluator *cal = (*evs)[2];
                delete evs;
                return new Ddays(tpos, date1, date2, cal);
            } else if (t == "MAX" || t == "MIN") {
                if (t == "MAX")
                    return new Max(tpos, evs);
                else // t == "MIN"
                    return new Min(tpos, evs);
            } else if (t == "APPEND") {
                return new Append(tpos, evs);
            } else if (t == "N" || t == "I%YR" || t == "PV"
                    || t == "PMT" || t == "FV") {
                if (t == "N")
                    return new Tvm(tpos, CMD_GEN_N, evs);
                else if (t == "I%YR")
                    return new Tvm(tpos, CMD_GEN_I, evs);
                else if (t == "PV")
                    return new Tvm(tpos, CMD_GEN_PV, evs);
                else if (t == "PMT")
                    return new Tvm(tpos, CMD_GEN_PMT, evs);
                else if (t == "FV")
                    return new Tvm(tpos, CMD_GEN_FV, evs);
                else
                    // Shouldn't get here
                    return NULL;
            } else if (t == "XEQ" || t == "EVALN") {
                Evaluator *name = (*evs)[0];
                std::string n = name->name();
                evs->erase(evs->begin());
                return new Xeq(tpos, n, evs, t == "EVALN");
            } else if (t == "SEQ" || t == "VIEW") {
                return new Seq(tpos, t == "VIEW", lex->compatMode, evs);
            } else if (t == "IF") {
                Evaluator *condition = (*evs)[0];
                Evaluator *trueEv = (*evs)[1];
                Evaluator *falseEv = (*evs)[2];
                delete evs;
                return new If(tpos, condition, trueEv, falseEv);
            } else if (t == "G") {
                Evaluator *name = (*evs)[0];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Gee(tpos, n, lex->compatMode);
            } else if (t == "S") {
                Evaluator *name = (*evs)[0];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Ess(tpos, n);
            } else if (t == "L") {
                Evaluator *left = (*evs)[0];
                Evaluator *right = (*evs)[1];
                delete evs;
                std::string n = left->name();
                if (n != "") {
                    delete left;
                    return new Ell(tpos, n, right, lex->compatMode);
                } else {
                    return new Ell(tpos, left, right, lex->compatMode);
                }
            } else if (t == "LL") {
                Evaluator *name = (*evs)[0];
                evs->erase(evs->begin());
                Evaluator *value = (*evs)[0];
                evs->erase(evs->begin());
                std::string n = name->name();
                delete name;
                return new LocalEll(tpos, n, value, evs);
            } else if (t == "ITEM") {
                Evaluator *name = (*evs)[0];
                Evaluator *ev1 = (*evs)[1];
                Evaluator *ev2 = evs->size() == 3 ? (*evs)[2] : NULL;
                delete evs;
                std::string n = name->name();
                delete name;
                return new Item(tpos, n, ev1, ev2);
            } else if (t == "POS" || t == "SUBSTR") {
                return new PosOrSubstr(tpos, evs, t == "POS");
            } else if (t == "FLOW" || t == "#T") {
                Evaluator *name = (*evs)[0];
                Evaluator *ev = (*evs)[1];
                delete evs;
                std::string n = name->name();
                delete name;
                return new FlowItem(tpos, n, ev, t == "FLOW" ? 1 : 2);
            } else if (t == "FOR") {
                Evaluator *init = (*evs)[0];
                evs->erase(evs->begin());
                Evaluator *cond = (*evs)[0];
                evs->erase(evs->begin());
                Evaluator *next = (*evs)[0];
                evs->erase(evs->begin());
                f->finish_init(init, cond, next, evs);
                return f;
            } else if (t == "\5") {
                Evaluator *name = (*evs)[0];
                Evaluator *from = (*evs)[1];
                Evaluator *to = (*evs)[2];
                Evaluator *step = (*evs)[3];
                Evaluator *ev = (*evs)[4];
                delete evs;
                std::string n = name->name();
                delete name;
                return new Sigma(tpos, n, from, to, step, ev);
            } else if (t == "\3") {
                Evaluator *expr = (*evs)[0];
                Evaluator *name = (*evs)[1];
                std::string integ_var = name->name();
                delete name;
                Evaluator *llim = (*evs)[2];
                Evaluator *ulim = (*evs)[3];
                Evaluator *acc = evs->size() == 5 ? (*evs)[4] : NULL;
                delete evs;
                return new Integ(tpos, expr, integ_var, llim, ulim, acc);
            } else
                return new Call(tpos, t, evs);
        } else if (!lex->compatMode && t2 == "[") {
            Evaluator *ev1 = parseExpr(CTX_VALUE);
            Evaluator *ev2 = NULL;
            if (ev1 == NULL)
                return NULL;
            if (!nextToken(&t2, &t2pos)) {
                item_fail:
                delete ev1;
                return NULL;
            }
            if (t2 == ":") {
                ev2 = parseExpr(CTX_VALUE);
                if (ev2 == NULL)
                    goto item_fail;
                if (!nextToken(&t2, &t2pos)) {
                    item2_fail:
                    delete ev1;
                    delete ev2;
                    return NULL;
                }
            }
            if (t2 != "]")
                goto item2_fail;
            if (t == "STACK")
                return new Register(tpos, ev1);
            else
                return new Item(tpos, t, ev1, ev2);
        } else {
            pushback(t2, t2pos);
            if (t == "PI" || t == "\7")
                return new RecallFunction(tpos, CMD_PI);
            else if (t == "RAN#")
                return new RecallFunction(tpos, CMD_RAN);
            else if (t == "CDATE")
                return new RecallFunction(tpos, CMD_DATE);
            else if (t == "CTIME")
                return new RecallFunction(tpos, CMD_TIME);
            else if (t == "NEWLIST")
                return new RecallFunction(tpos, CMD_NEWLIST);
            else if (t == "REGX")
                return new Register(tpos, 1);
            else if (t == "REGY")
                return new Register(tpos, 2);
            else if (t == "REGZ")
                return new Register(tpos, 3);
            else if (t == "REGT")
                return new Register(tpos, 4);
            else if (t == "LASTX")
                return new Register(tpos, 0);
            else if (t == "\5X")
                return new RecallFunction(tpos, CMD_SX);
            else if (t == "\5X2")
                return new RecallFunction(tpos, CMD_SX2);
            else if (t == "\5Y")
                return new RecallFunction(tpos, CMD_SY);
            else if (t == "\5Y2")
                return new RecallFunction(tpos, CMD_SY2);
            else if (t == "\5XY")
                return new RecallFunction(tpos, CMD_SXY);
            else if (t == "\5N")
                return new RecallFunction(tpos, CMD_SN);
            else if (t == "\5LNX")
                return new RecallFunction(tpos, CMD_SLNX);
            else if (t == "\5LNX2")
                return new RecallFunction(tpos, CMD_SLNX2);
            else if (t == "\5LNY")
                return new RecallFunction(tpos, CMD_SLNY);
            else if (t == "\5LNY2")
                return new RecallFunction(tpos, CMD_SLNY2);
            else if (t == "\5LNXLNY")
                return new RecallFunction(tpos, CMD_SLNXLNY);
            else if (t == "\5XLNY")
                return new RecallFunction(tpos, CMD_SXLNY);
            else if (t == "\5YLNX")
                return new RecallFunction(tpos, CMD_SYLNX);
            else if (t == "WMEAN")
                return new RecallFunction(tpos, CMD_WMEAN);
            else if (t == "CORR")
                return new RecallFunction(tpos, CMD_CORR);
            else if (t == "SLOPE")
                return new RecallFunction(tpos, CMD_SLOPE);
            else if (t == "YINT")
                return new RecallFunction(tpos, CMD_YINT);
            else if (t == "MEANX")
                return new RecallOneOfTwoFunction(tpos, CMD_MEAN, true);
            else if (t == "MEANY")
                return new RecallOneOfTwoFunction(tpos, CMD_MEAN, false);
            else if (t == "SDEVX")
                return new RecallOneOfTwoFunction(tpos, CMD_SDEV, true);
            else if (t == "SDEVY")
                return new RecallOneOfTwoFunction(tpos, CMD_SDEV, false);
            else if (t == "BREAK" || t == "CONTINUE") {
                if (forStack.size() == 0)
                    return NULL;
                For *f = forStack.back();
                if (f->pos() == -1)
                    return NULL;
                else if (t == "BREAK")
                    return new Break(tpos, f);
                else
                    return new Continue(tpos, f);
            } else
                return new Variable(tpos, t);
        }
    } else {
        return NULL;
    }
}

bool Parser::nextToken(std::string *tok, int *tpos) {
    if (pbpos != -1) {
        *tok = pb;
        *tpos = pbpos;
        pbpos = -1;
        return true;
    } else
        return lex->nextToken(tok, tpos);
}

void Parser::pushback(std::string o, int p) {
    pb = o;
    pbpos = p;
}

void get_varmenu_row_for_eqn(vartype *eqn, int need_eval, int *rows, int *row, char ktext[6][7], int klen[6]) {
    Evaluator *ev = ((vartype_equation *) eqn)->data->ev;
    std::vector<std::string> vars;
    std::vector<std::string> locals;
    ev->collectVariables(&vars, &locals);
    *rows = ((int) vars.size() + 5 + (need_eval != 0)) / 6;
    if (*rows == 0)
        return;
    if (*row >= *rows)
        *row = *rows - 1;
    for (int i = 0; i < 6; i++) {
        int r = 6 * *row + i - (need_eval != 0);
        if (r == -1) {
            if (need_eval == 1) {
                memcpy(ktext[i], "EVAL", 4);
                klen[i] = 4;
            } else {
                memcpy(ktext[i], "STK", 3);
                klen[i] = 3;
            }
        } else if (r < vars.size()) {
            std::string t = vars[r];
            int len = (int) t.length();
            if (len > 7)
                len = 7;
            memcpy(ktext[i], t.c_str(), len);
            klen[i] = len;
        } else
            klen[i] = 0;
    }
}

static vartype *isolate2(vartype *eqn, const char *name, int length) {
    if (eqn == NULL || eqn->type != TYPE_EQUATION)
        return NULL;
    vartype_equation *eq = (vartype_equation *) eqn;
    equation_data *eqd = eq->data;
    Evaluator *ev = eqd->ev;
    std::string n(name, length);
    if (ev->howMany(n) != 1)
        return NULL;
    Evaluator *rhs;
    ev->getSides(n, &ev, &rhs);
    char *ntext = NULL;
    if (rhs == NULL) {
        rhs = new Literal(0, 0);
    } else {
        rhs = rhs->clone(NULL);
        if (ev->name() == n) {
            /* Trivial: 'name' is already isolated.
             * In this case, we can use the original text and
             * create a code map...
             */
            ntext = (char *) malloc(eqd->length);
            if (ntext != NULL)
                memcpy(ntext, eqd->text, eqd->length);
        }
    }
    ev = ev->invert(n, rhs);

    // After this point, no bad_alloc should be thrown, since we're doing our own
    // allocations with nothrow from here to the end, and the allocation failures
    // in Parser::generateCode() are handled in that function.

    int4 neq = new_eqn_idx();
    if (neq == -1) {
        delete ev;
        return NULL;
    }
    equation_data *neqd = new (std::nothrow) equation_data;
    if (neqd == NULL) {
        delete ev;
        return NULL;
    }
    eq_dir->prgms[neq].eq_data = neqd;
    neqd->compatMode = eqd->compatMode;
    neqd->eqn_index = neq;
    if (ntext != NULL) {
        neqd->map = new (std::nothrow) CodeMap;
        if (neqd->map == NULL) {
            free(ntext);
        } else {
            neqd->text = ntext;
            neqd->length = eqd->length;
        }
    }
    Parser::generateCode(ev, eq_dir->prgms + neq, neqd->map);
    if (neqd->map != NULL && neqd->map->getSize() == -1) {
        delete neqd->map;
        neqd->map = NULL;
    }
    delete ev;
    if (eq_dir->prgms[neq].text == NULL) {
        // Code generator failure
        eq_dir->prgms[neq].eq_data = NULL;
        delete neqd;
        return NULL;
    } else {
        vartype *v = new_equation(neqd);
        if (v == NULL)
            delete neqd;
        return v;
    }
}

vartype *isolate(vartype *eqn, const char *name, int length) {
    try {
        return isolate2(eqn, name, length);
    } catch (std::bad_alloc &) {
        return NULL;
    }
}

bool has_parameters(equation_data *eqdata) {
    std::vector<std::string> names, locals;
    eqdata->ev->collectVariables(&names, &locals);
    return names.size() > 0;
}

std::vector<std::string> get_parameters(equation_data *eqdata) {
    std::vector<std::string> names, locals;
    eqdata->ev->collectVariables(&names, &locals);
    return names;
}

std::vector<std::string> get_mvars(const char *name, int namelen) {
    std::vector<std::string> names;
    if (namelen > 7)
        // Too long
        return names;
    arg_struct arg;
    arg.type = ARGTYPE_STR;
    int len;
    string_copy(arg.val.text, &len, name, namelen);
    arg.length = len;
    pgm_index prgm;
    int4 pc;
    if (!find_global_label(&arg, &prgm, &pc))
        return names;
    pgm_index saved_prgm = current_prgm;
    current_prgm = prgm;
    int cmd;
    pc += get_command_length(current_prgm, pc);
    while (get_next_command(&pc, &cmd, &arg, 0, NULL), cmd == CMD_MVAR)
        names.push_back(std::string(arg.val.text, arg.length));
    current_prgm = saved_prgm;
    return names;
}

bool is_equation(vartype *v) {
    if (v->type != TYPE_EQUATION)
        return false;
    equation_data *eqd = ((vartype_equation *) v)->data;
    Evaluator *lhs, *rhs;
    eqd->ev->getSides("foo", &lhs, &rhs);
    return rhs != NULL;
}

void num_parameters(vartype *v, int *black, int *total) {
    equation_data *eqd = ((vartype_equation *) v)->data;
    std::vector<std::string> names, locals;
    eqd->ev->collectVariables(&names, &locals);
    *total = (int) names.size();
    std::vector<std::string> *paramNames = eqd->ev->eqnParamNames();
    *black = paramNames == NULL || paramNames->size() == 0 ? *total : (int) paramNames->size();
}
