/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qv4jsir_p.h"
#include <private/qqmljsast_p.h>

#ifndef V4_BOOTSTRAP
#include <private/qqmlpropertycache_p.h>
#endif

#include <QtCore/QBuffer>
#include <QtCore/qtextstream.h>
#include <QtCore/qdebug.h>
#include <QtCore/qset.h>
#include <cmath>

#ifdef CONST
#undef CONST
#endif

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace IR {

QString typeName(Type t)
{
    switch (t) {
    case UnknownType: return QStringLiteral("");
    case MissingType: return QStringLiteral("missing");
    case UndefinedType: return QStringLiteral("undefined");
    case NullType: return QStringLiteral("null");
    case BoolType: return QStringLiteral("bool");
    case UInt32Type: return QStringLiteral("uint32");
    case SInt32Type: return QStringLiteral("int32");
    case DoubleType: return QStringLiteral("double");
    case NumberType: return QStringLiteral("number");
    case StringType: return QStringLiteral("string");
    case VarType: return QStringLiteral("var");
    case QObjectType: return QStringLiteral("qobject");
    default: return QStringLiteral("multiple");
    }
}

const char *opname(AluOp op)
{
    switch (op) {
    case OpInvalid: return "?";

    case OpIfTrue: return "(bool)";
    case OpNot: return "not";
    case OpUMinus: return "neg";
    case OpUPlus: return "plus";
    case OpCompl: return "invert";
    case OpIncrement: return "incr";
    case OpDecrement: return "decr";

    case OpBitAnd: return "bitand";
    case OpBitOr: return "bitor";
    case OpBitXor: return "bitxor";

    case OpAdd: return "add";
    case OpSub: return "sub";
    case OpMul: return "mul";
    case OpDiv: return "div";
    case OpMod: return "mod";

    case OpLShift: return "shl";
    case OpRShift: return "shr";
    case OpURShift: return "asr";

    case OpGt: return "gt";
    case OpLt: return "lt";
    case OpGe: return "ge";
    case OpLe: return "le";
    case OpEqual: return "eq";
    case OpNotEqual: return "ne";
    case OpStrictEqual: return "se";
    case OpStrictNotEqual: return "sne";

    case OpInstanceof: return "instanceof";
    case OpIn: return "in";

    case OpAnd: return "and";
    case OpOr: return "or";

    default: return "?";

    } // switch
}

AluOp binaryOperator(int op)
{
    switch (static_cast<QSOperator::Op>(op)) {
    case QSOperator::Add: return OpAdd;
    case QSOperator::And: return OpAnd;
    case QSOperator::BitAnd: return OpBitAnd;
    case QSOperator::BitOr: return OpBitOr;
    case QSOperator::BitXor: return OpBitXor;
    case QSOperator::Div: return OpDiv;
    case QSOperator::Equal: return OpEqual;
    case QSOperator::Ge: return OpGe;
    case QSOperator::Gt: return OpGt;
    case QSOperator::Le: return OpLe;
    case QSOperator::LShift: return OpLShift;
    case QSOperator::Lt: return OpLt;
    case QSOperator::Mod: return OpMod;
    case QSOperator::Mul: return OpMul;
    case QSOperator::NotEqual: return OpNotEqual;
    case QSOperator::Or: return OpOr;
    case QSOperator::RShift: return OpRShift;
    case QSOperator::StrictEqual: return OpStrictEqual;
    case QSOperator::StrictNotEqual: return OpStrictNotEqual;
    case QSOperator::Sub: return OpSub;
    case QSOperator::URShift: return OpURShift;
    case QSOperator::InstanceOf: return OpInstanceof;
    case QSOperator::In: return OpIn;
    default: return OpInvalid;
    }
}

struct RemoveSharedExpressions: IR::StmtVisitor, IR::ExprVisitor
{
    CloneExpr clone;
    QSet<Expr *> subexpressions; // contains all the non-cloned subexpressions in the given function
    Expr *uniqueExpr;

    RemoveSharedExpressions(): uniqueExpr(0) {}

    void operator()(IR::Function *function)
    {
        subexpressions.clear();
        subexpressions.reserve(function->basicBlockCount() * 8);

        foreach (BasicBlock *block, function->basicBlocks()) {
            if (block->isRemoved())
                continue;
            clone.setBasicBlock(block);

            foreach (Stmt *s, block->statements()) {
                s->accept(this);
            }
        }
    }

    template <typename _Expr>
    _Expr *cleanup(_Expr *expr)
    {
        if (subexpressions.contains(expr)) {
             // the cloned expression is unique by definition
            // so we don't need to add it to `subexpressions'.
            return clone(expr);
        }

        subexpressions.insert(expr);
        IR::Expr *e = expr;
        qSwap(uniqueExpr, e);
        expr->accept(this);
        qSwap(uniqueExpr, e);
        return static_cast<_Expr *>(e);
    }

    // statements
    virtual void visitExp(Exp *s)
    {
        s->expr = cleanup(s->expr);
    }

    virtual void visitMove(Move *s)
    {
        s->target = cleanup(s->target);
        s->source = cleanup(s->source);
    }

    virtual void visitJump(Jump *)
    {
        // nothing to do for Jump statements
    }

    virtual void visitCJump(CJump *s)
    {
        s->cond = cleanup(s->cond);
    }

    virtual void visitRet(Ret *s)
    {
        s->expr = cleanup(s->expr);
    }

    virtual void visitPhi(IR::Phi *) { Q_UNIMPLEMENTED(); }

    // expressions
    virtual void visitConst(Const *) {}
    virtual void visitString(String *) {}
    virtual void visitRegExp(RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitTemp(Temp *) {}
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}

    virtual void visitConvert(Convert *e)
    {
        e->expr = cleanup(e->expr);
    }

    virtual void visitUnop(Unop *e)
    {
        e->expr = cleanup(e->expr);
    }

    virtual void visitBinop(Binop *e)
    {
        e->left = cleanup(e->left);
        e->right = cleanup(e->right);
    }

    virtual void visitCall(Call *e)
    {
        e->base = cleanup(e->base);
        for (IR::ExprList *it = e->args; it; it = it->next)
            it->expr = cleanup(it->expr);
    }

    virtual void visitNew(New *e)
    {
        e->base = cleanup(e->base);
        for (IR::ExprList *it = e->args; it; it = it->next)
            it->expr = cleanup(it->expr);
    }

    virtual void visitSubscript(Subscript *e)
    {
        e->base = cleanup(e->base);
        e->index = cleanup(e->index);
    }

    virtual void visitMember(Member *e)
    {
        e->base = cleanup(e->base);
    }
};

void Name::initGlobal(const QString *id, quint32 line, quint32 column)
{
    this->id = id;
    this->builtin = builtin_invalid;
    this->global = true;
    this->qmlSingleton = false;
    this->freeOfSideEffects = false;
    this->line = line;
    this->column = column;
}

void Name::init(const QString *id, quint32 line, quint32 column)
{
    this->id = id;
    this->builtin = builtin_invalid;
    this->global = false;
    this->qmlSingleton = false;
    this->freeOfSideEffects = false;
    this->line = line;
    this->column = column;
}

void Name::init(Builtin builtin, quint32 line, quint32 column)
{
    this->id = 0;
    this->builtin = builtin;
    this->global = false;
    this->qmlSingleton = false;
    this->freeOfSideEffects = false;
    this->line = line;
    this->column = column;
}

const char *builtin_to_string(Name::Builtin b)
{
    switch (b) {
    case Name::builtin_invalid:
        return "builtin_invalid";
    case Name::builtin_typeof:
        return "builtin_typeof";
    case Name::builtin_delete:
        return "builtin_delete";
    case Name::builtin_throw:
        return "builtin_throw";
    case Name::builtin_rethrow:
        return "builtin_rethrow";
    case Name::builtin_unwind_exception:
        return "builtin_unwind_exception";
    case Name::builtin_push_catch_scope:
        return "builtin_push_catch_scope";
    case IR::Name::builtin_foreach_iterator_object:
        return "builtin_foreach_iterator_object";
    case IR::Name::builtin_foreach_next_property_name:
        return "builtin_foreach_next_property_name";
    case IR::Name::builtin_push_with_scope:
        return "builtin_push_with_scope";
    case IR::Name::builtin_pop_scope:
        return "builtin_pop_scope";
    case IR::Name::builtin_declare_vars:
        return "builtin_declare_vars";
    case IR::Name::builtin_define_array:
        return "builtin_define_array";
    case IR::Name::builtin_define_object_literal:
        return "builtin_define_object_literal";
    case IR::Name::builtin_setup_argument_object:
        return "builtin_setup_argument_object";
    case IR::Name::builtin_convert_this_to_object:
        return "builtin_convert_this_to_object";
    case IR::Name::builtin_qml_id_array:
        return "builtin_qml_id_array";
    case IR::Name::builtin_qml_imported_scripts_object:
        return "builtin_qml_imported_scripts_object";
    case IR::Name::builtin_qml_scope_object:
        return "builtin_qml_scope_object";
    case IR::Name::builtin_qml_context_object:
        return "builtin_qml_context_object";
    }
    return "builtin_(###FIXME)";
};

bool operator<(const Temp &t1, const Temp &t2) Q_DECL_NOTHROW
{
    if (t1.kind < t2.kind) return true;
    if (t1.kind > t2.kind) return false;
    return t1.index < t2.index;
}

Function *Module::newFunction(const QString &name, Function *outer)
{
    Function *f = new Function(this, outer, name);
    functions.append(f);
    if (!outer) {
        if (!isQmlModule) {
            Q_ASSERT(!rootFunction);
            rootFunction = f;
        }
    } else {
        outer->nestedFunctions.append(f);
    }
    return f;
}

Module::~Module()
{
    qDeleteAll(functions);
}

void Module::setFileName(const QString &name)
{
    if (fileName.isEmpty())
        fileName = name;
    else {
        Q_ASSERT(fileName == name);
    }
}

Function::Function(Module *module, Function *outer, const QString &name)
    : module(module)
    , pool(&module->pool)
    , tempCount(0)
    , maxNumberOfArguments(0)
    , outer(outer)
    , insideWithOrCatch(0)
    , hasDirectEval(false)
    , usesArgumentsObject(false)
    , isStrict(false)
    , isNamedExpression(false)
    , hasTry(false)
    , hasWith(false)
    , unused(0)
    , line(-1)
    , column(-1)
    , _allBasicBlocks(0)
    , _statementCount(0)
{
    this->name = newString(name);
    _basicBlocks.reserve(8);
}

Function::~Function()
{
    if (_allBasicBlocks) {
        qDeleteAll(*_allBasicBlocks);
        delete _allBasicBlocks;
    } else {
        qDeleteAll(_basicBlocks);
    }

    pool = 0;
    module = 0;
}


const QString *Function::newString(const QString &text)
{
    return &*strings.insert(text);
}

BasicBlock *Function::newBasicBlock(BasicBlock *catchBlock, BasicBlockInsertMode mode)
{
    BasicBlock *block = new BasicBlock(this, catchBlock);
    return mode == InsertBlock ? addBasicBlock(block) : block;
}

BasicBlock *Function::addBasicBlock(BasicBlock *block)
{
    Q_ASSERT(block->index() < 0);
    block->setIndex(_basicBlocks.size());
    _basicBlocks.append(block);
    return block;
}

void Function::removeBasicBlock(BasicBlock *block)
{
    block->markAsRemoved();
    block->in.clear();
    block->out.clear();
}

int Function::liveBasicBlocksCount() const
{
    int count = 0;
    foreach (BasicBlock *bb, basicBlocks())
        if (!bb->isRemoved())
            ++count;
    return count;
}

void Function::removeSharedExpressions()
{
    RemoveSharedExpressions removeSharedExpressions;
    removeSharedExpressions(this);
}

int Function::indexOfArgument(const QStringRef &string) const
{
    for (int i = formals.size() - 1; i >= 0; --i) {
        if (*formals.at(i) == string)
            return i;
    }
    return -1;
}

void Function::setScheduledBlocks(const QVector<BasicBlock *> &scheduled)
{
    Q_ASSERT(!_allBasicBlocks);
    _allBasicBlocks = new QVector<BasicBlock *>(basicBlocks());
    _basicBlocks = scheduled;
}

void Function::renumberBasicBlocks()
{
    for (int i = 0, ei = basicBlockCount(); i != ei; ++i)
        basicBlock(i)->changeIndex(i);
}

BasicBlock *Function::getOrCreateBasicBlock(int index)
{
    if (_basicBlocks.size() <= index) {
        const int oldSize = _basicBlocks.size();
        _basicBlocks.resize(index + 1);
        for (int i = oldSize; i <= index; ++i) {
            BasicBlock *block = new BasicBlock(this, 0);
            block->setIndex(i);
            _basicBlocks[i] = block;
        }
    }

    return _basicBlocks.at(index);
}

void Function::setStatementCount(int cnt)
{
    _statementCount = cnt;
}

BasicBlock::~BasicBlock()
{
    foreach (Stmt *s, _statements)
        s->destroyData();
}

unsigned BasicBlock::newTemp()
{
    Q_ASSERT(!isRemoved());
    return function->tempCount++;
}

Temp *BasicBlock::TEMP(unsigned index)
{
    Q_ASSERT(!isRemoved());
    Temp *e = function->New<Temp>();
    e->init(Temp::VirtualRegister, index);
    return e;
}

ArgLocal *BasicBlock::ARG(unsigned index, unsigned scope)
{
    Q_ASSERT(!isRemoved());
    ArgLocal *e = function->New<ArgLocal>();
    e->init(scope ? ArgLocal::ScopedFormal : ArgLocal::Formal, index, scope);
    return e;
}

ArgLocal *BasicBlock::LOCAL(unsigned index, unsigned scope)
{
    Q_ASSERT(!isRemoved());
    ArgLocal *e = function->New<ArgLocal>();
    e->init(scope ? ArgLocal::ScopedLocal : ArgLocal::Local, index, scope);
    return e;
}

Expr *BasicBlock::CONST(Type type, double value)
{
    Q_ASSERT(!isRemoved());
    Const *e = function->New<Const>();
    if (type == NumberType) {
        int ival = (int)value;
        // +0 != -0, so we need to convert to double when negating 0
        if (ival == value && !(value == 0 && isNegative(value)))
            type = SInt32Type;
        else
            type = DoubleType;
    } else if (type == NullType) {
        value = 0;
    } else if (type == UndefinedType) {
        value = qSNaN();
    }

    e->init(type, value);
    return e;
}

Expr *BasicBlock::STRING(const QString *value)
{
    Q_ASSERT(!isRemoved());
    String *e = function->New<String>();
    e->init(value);
    return e;
}

Expr *BasicBlock::REGEXP(const QString *value, int flags)
{
    Q_ASSERT(!isRemoved());
    RegExp *e = function->New<RegExp>();
    e->init(value, flags);
    return e;
}

Name *BasicBlock::NAME(const QString &id, quint32 line, quint32 column)
{
    Q_ASSERT(!isRemoved());
    Name *e = function->New<Name>();
    e->init(function->newString(id), line, column);
    return e;
}

Name *BasicBlock::GLOBALNAME(const QString &id, quint32 line, quint32 column)
{
    Q_ASSERT(!isRemoved());
    Name *e = function->New<Name>();
    e->initGlobal(function->newString(id), line, column);
    return e;
}


Name *BasicBlock::NAME(Name::Builtin builtin, quint32 line, quint32 column)
{
    Q_ASSERT(!isRemoved());
    Name *e = function->New<Name>();
    e->init(builtin, line, column);
    return e;
}

Closure *BasicBlock::CLOSURE(int functionInModule)
{
    Q_ASSERT(!isRemoved());
    Closure *clos = function->New<Closure>();
    clos->init(functionInModule, function->module->functions.at(functionInModule)->name);
    return clos;
}

Expr *BasicBlock::CONVERT(Expr *expr, Type type)
{
    Q_ASSERT(!isRemoved());
    Convert *e = function->New<Convert>();
    e->init(expr, type);
    return e;
}

Expr *BasicBlock::UNOP(AluOp op, Expr *expr)
{
    Q_ASSERT(!isRemoved());
    Unop *e = function->New<Unop>();
    e->init(op, expr);
    return e;
}

Expr *BasicBlock::BINOP(AluOp op, Expr *left, Expr *right)
{
    Q_ASSERT(!isRemoved());
    Binop *e = function->New<Binop>();
    e->init(op, left, right);
    return e;
}

Expr *BasicBlock::CALL(Expr *base, ExprList *args)
{
    Q_ASSERT(!isRemoved());
    Call *e = function->New<Call>();
    e->init(base, args);
    int argc = 0;
    for (ExprList *it = args; it; it = it->next)
        ++argc;
    function->maxNumberOfArguments = qMax(function->maxNumberOfArguments, argc);
    return e;
}

Expr *BasicBlock::NEW(Expr *base, ExprList *args)
{
    Q_ASSERT(!isRemoved());
    New *e = function->New<New>();
    e->init(base, args);
    return e;
}

Expr *BasicBlock::SUBSCRIPT(Expr *base, Expr *index)
{
    Q_ASSERT(!isRemoved());
    Subscript *e = function->New<Subscript>();
    e->init(base, index);
    return e;
}

Expr *BasicBlock::MEMBER(Expr *base, const QString *name, QQmlPropertyData *property, uchar kind, int attachedPropertiesIdOrEnumValue)
{
    Q_ASSERT(!isRemoved());
    Member*e = function->New<Member>();
    e->init(base, name, property, kind, attachedPropertiesIdOrEnumValue);
    return e;
}

Stmt *BasicBlock::EXP(Expr *expr)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    Exp *s = function->NewStmt<Exp>();
    s->init(expr);
    appendStatement(s);
    return s;
}

Stmt *BasicBlock::MOVE(Expr *target, Expr *source)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    Move *s = function->NewStmt<Move>();
    s->init(target, source);
    appendStatement(s);
    return s;
}

Stmt *BasicBlock::JUMP(BasicBlock *target)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    Jump *s = function->NewStmt<Jump>();
    s->init(target);
    appendStatement(s);

    Q_ASSERT(! out.contains(target));
    out.append(target);

    Q_ASSERT(! target->in.contains(this));
    target->in.append(this);

    return s;
}

Stmt *BasicBlock::CJUMP(Expr *cond, BasicBlock *iftrue, BasicBlock *iffalse)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    if (iftrue == iffalse) {
        MOVE(TEMP(newTemp()), cond);
        return JUMP(iftrue);
    }

    CJump *s = function->NewStmt<CJump>();
    s->init(cond, iftrue, iffalse, this);
    appendStatement(s);

    Q_ASSERT(! out.contains(iftrue));
    out.append(iftrue);

    Q_ASSERT(! iftrue->in.contains(this));
    iftrue->in.append(this);

    Q_ASSERT(! out.contains(iffalse));
    out.append(iffalse);

    Q_ASSERT(! iffalse->in.contains(this));
    iffalse->in.append(this);

    return s;
}

Stmt *BasicBlock::RET(Expr *expr)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    Ret *s = function->NewStmt<Ret>();
    s->init(expr);
    appendStatement(s);
    return s;
}

void BasicBlock::setStatements(const QVector<Stmt *> &newStatements)
{
    Q_ASSERT(!isRemoved());
    Q_ASSERT(newStatements.size() >= _statements.size());
    // FIXME: this gets quite inefficient for large basic-blocks, so this function/case should be re-worked.
    foreach (Stmt *s, _statements) {
        if (!newStatements.contains(s))
            s->destroyData();
    }
    _statements = newStatements;
}

void BasicBlock::appendStatement(Stmt *statement)
{
    Q_ASSERT(!isRemoved());
    if (nextLocation.startLine)
        statement->location = nextLocation;
    _statements.append(statement);
}

void BasicBlock::prependStatement(Stmt *stmt)
{
    Q_ASSERT(!isRemoved());
    _statements.prepend(stmt);
}

void BasicBlock::prependStatements(const QVector<Stmt *> &stmts)
{
    Q_ASSERT(!isRemoved());
    QVector<Stmt *> newStmts = stmts;
    newStmts += _statements;
    _statements = newStmts;
}

void BasicBlock::insertStatementBefore(Stmt *before, Stmt *newStmt)
{
    int idx = _statements.indexOf(before);
    Q_ASSERT(idx >= 0);
    _statements.insert(idx, newStmt);
}

void BasicBlock::insertStatementBefore(int index, Stmt *newStmt)
{
    Q_ASSERT(index >= 0);
    _statements.insert(index, newStmt);
}

void BasicBlock::insertStatementBeforeTerminator(Stmt *stmt)
{
    Q_ASSERT(!isRemoved());
    _statements.insert(_statements.size() - 1, stmt);
}

void BasicBlock::replaceStatement(int index, Stmt *newStmt)
{
    Q_ASSERT(!isRemoved());
    _statements[index]->destroyData();
    _statements[index] = newStmt;
}

void BasicBlock::removeStatement(Stmt *stmt)
{
    Q_ASSERT(!isRemoved());
    stmt->destroyData();
    _statements.remove(_statements.indexOf(stmt));
}

void BasicBlock::removeStatement(int idx)
{
    Q_ASSERT(!isRemoved());
    _statements[idx]->destroyData();
    _statements.remove(idx);
}

CloneExpr::CloneExpr(BasicBlock *block)
    : block(block), cloned(0)
{
}

void CloneExpr::setBasicBlock(BasicBlock *block)
{
    this->block = block;
}

ExprList *CloneExpr::clone(ExprList *list)
{
    if (! list)
        return 0;

    ExprList *clonedList = block->function->New<IR::ExprList>();
    clonedList->init(clone(list->expr), clone(list->next));
    return clonedList;
}

void CloneExpr::visitConst(Const *e)
{
    cloned = cloneConst(e, block->function);
}

void CloneExpr::visitString(String *e)
{
    cloned = block->STRING(e->value);
}

void CloneExpr::visitRegExp(RegExp *e)
{
    cloned = block->REGEXP(e->value, e->flags);
}

void CloneExpr::visitName(Name *e)
{
    cloned = cloneName(e, block->function);
}

void CloneExpr::visitTemp(Temp *e)
{
    cloned = cloneTemp(e, block->function);
}

void CloneExpr::visitArgLocal(ArgLocal *e)
{
    cloned = cloneArgLocal(e, block->function);
}

void CloneExpr::visitClosure(Closure *e)
{
    cloned = block->CLOSURE(e->value);
}

void CloneExpr::visitConvert(Convert *e)
{
    cloned = block->CONVERT(clone(e->expr), e->type);
}

void CloneExpr::visitUnop(Unop *e)
{
    cloned = block->UNOP(e->op, clone(e->expr));
}

void CloneExpr::visitBinop(Binop *e)
{
    cloned = block->BINOP(e->op, clone(e->left), clone(e->right));
}

void CloneExpr::visitCall(Call *e)
{
    cloned = block->CALL(clone(e->base), clone(e->args));
}

void CloneExpr::visitNew(New *e)
{
    cloned = block->NEW(clone(e->base), clone(e->args));
}

void CloneExpr::visitSubscript(Subscript *e)
{
    cloned = block->SUBSCRIPT(clone(e->base), clone(e->index));
}

void CloneExpr::visitMember(Member *e)
{
    Expr *clonedBase = clone(e->base);
    cloned = block->MEMBER(clonedBase, e->name, e->property, e->kind, e->attachedPropertiesIdOrEnumValue);
}

IRPrinter::IRPrinter(QTextStream *out)
    : out(out)
    , positionSize(Stmt::InvalidId)
    , currentBB(0)
{
}

IRPrinter::~IRPrinter()
{
}

void IRPrinter::print(Stmt *s)
{
    s->accept(this);
}

void IRPrinter::print(const Expr &e)
{
    const_cast<Expr *>(&e)->accept(this);
}

void IRPrinter::print(Expr *e)
{
    e->accept(this);
}

void IRPrinter::print(Function *f)
{
    if (positionSize == Stmt::InvalidId)
        positionSize = QString::number(f->statementCount()).size();

    QString n = f->name ? *f->name : QString();
    if (n.isEmpty())
        n.sprintf("%p", f);
    *out << "function " << n << '(';

    for (int i = 0; i < f->formals.size(); ++i) {
        if (i != 0)
            *out << ", ";
        *out << *f->formals.at(i);
    }
    *out << ')' << endl
        << '{' << endl;

    foreach (const QString *local, f->locals)
        *out << "    local var " << *local << endl;

    bool needsSeperator = !f->locals.isEmpty();
    foreach (BasicBlock *bb, f->basicBlocks()) {
        if (bb->isRemoved())
            continue;

        if (needsSeperator)
            *out << endl;
        else
            needsSeperator = true;
        print(bb);
    }
    *out << '}' << endl;
}

void IRPrinter::print(BasicBlock *bb)
{
    std::swap(currentBB, bb);
    printBlockStart();

    foreach (Stmt *s, currentBB->statements()) {
        if (!s)
            continue;

        QByteArray str;
        QBuffer buf(&str);
        buf.open(QIODevice::WriteOnly);
        QTextStream os(&buf);
        QTextStream *prevOut = &os;
        std::swap(out, prevOut);
        addStmtNr(s);
        s->accept(this);
        if (s->location.startLine) {
            out->flush();
            for (int i = 58 - str.length(); i > 0; --i)
                *out << ' ';
            *out << "    ; line: " << s->location.startLine << ", column: " << s->location.startColumn;
        }

        out->flush();
        std::swap(out, prevOut);

        *out << "    " << str << endl;
    }

    std::swap(currentBB, bb);
}

void IRPrinter::visitExp(Exp *s)
{
    *out << "void ";
    s->expr->accept(this);
}

void IRPrinter::visitMove(Move *s)
{
    if (Temp *targetTemp = s->target->asTemp())
        if (!s->swap && targetTemp->type != UnknownType)
            *out << typeName(targetTemp->type) << ' ';

    s->target->accept(this);
    *out << ' ';
    if (s->swap)
        *out << "<=> ";
    else
        *out << "= ";
    s->source->accept(this);
}

void IRPrinter::visitJump(Jump *s)
{
    *out << "goto L" << s->target->index();
}

void IRPrinter::visitCJump(CJump *s)
{
    *out << "if ";
    s->cond->accept(this);
    *out << " goto L" << s->iftrue->index()
         << " else goto L" << s->iffalse->index();
}

void IRPrinter::visitRet(Ret *s)
{
    *out << "return";
    if (s->expr) {
        *out << ' ';
        s->expr->accept(this);
    }
}

void IRPrinter::visitPhi(Phi *s)
{
    if (s->targetTemp->type != UnknownType)
        *out << typeName(s->targetTemp->type) << ' ';

    s->targetTemp->accept(this);
    *out << " = phi ";
    for (int i = 0, ei = s->d->incoming.size(); i < ei; ++i) {
        if (i > 0)
            *out << ", ";
        if (currentBB)
            *out << 'L' << currentBB->in.at(i)->index() << ": ";
        if (s->d->incoming[i])
            s->d->incoming[i]->accept(this);
    }
}

void IRPrinter::visitConst(Const *e)
{
    switch (e->type) {
    case QV4::IR::UndefinedType:
        *out << "undefined";
        break;
    case QV4::IR::NullType:
        *out << "null";
        break;
    case QV4::IR::BoolType:
        *out << (e->value ? "true" : "false");
        break;
    case QV4::IR::MissingType:
        *out << "missing";
        break;
    default:
        if (int(e->value) == 0 && int(e->value) == e->value) {
            if (isNegative(e->value))
                *out << "-0";
            else
                *out << "0";
        } else {
            *out << QString::number(e->value, 'g', 16);
        }
        break;
    }
}

void IRPrinter::visitString(String *e)
{
    *out << '"' << escape(*e->value) << '"';
}

void IRPrinter::visitRegExp(RegExp *e)
{
    char f[3];
    int i = 0;
    if (e->flags & RegExp::RegExp_Global)
        f[i++] = 'g';
    if (e->flags & RegExp::RegExp_IgnoreCase)
        f[i++] = 'i';
    if (e->flags & RegExp::RegExp_Multiline)
        f[i++] = 'm';
    f[i] = 0;

    *out << '/' << *e->value << '/' << f;
}

void IRPrinter::visitName(Name *e)
{
    if (e->id) {
        if (*e->id != QStringLiteral("this"))
            *out << '.';
        *out << *e->id;
    } else {
        *out << builtin_to_string(e->builtin);
    }
}

void IRPrinter::visitTemp(Temp *e)
{
    switch (e->kind) {
    case Temp::VirtualRegister:  *out << '%' << e->index; break;
    case Temp::PhysicalRegister: *out << (e->type == DoubleType ? "fp" : "r")
                                     << e->index; break;
    case Temp::StackSlot:        *out << '&' << e->index; break;
    default:                     *out << "INVALID";
    }
}

void IRPrinter::visitArgLocal(ArgLocal *e)
{
    switch (e->kind) {
    case ArgLocal::Formal:           *out << '#' << e->index; break;
    case ArgLocal::ScopedFormal:     *out << '#' << e->index
                                     << '@' << e->scope; break;
    case ArgLocal::Local:            *out << '$' << e->index; break;
    case ArgLocal::ScopedLocal:      *out << '$' << e->index
                                     << '@' << e->scope; break;
    default:                     *out << "INVALID";
    }
}

void IRPrinter::visitClosure(Closure *e)
{
    QString name = e->functionName ? *e->functionName : QString();
    if (name.isEmpty())
        name.sprintf("%x", e->value);
    *out << "closure " << name;
}

void IRPrinter::visitConvert(Convert *e)
{
    *out << "convert " << typeName(e->expr->type) << " to " << typeName(e->type) << ' ';
    e->expr->accept(this);
}

void IRPrinter::visitUnop(Unop *e)
{
    *out << opname(e->op) << ' ';
    e->expr->accept(this);
}

void IRPrinter::visitBinop(Binop *e)
{
    *out << opname(e->op) << ' ';
    e->left->accept(this);
    *out << ", ";
    e->right->accept(this);
}

void IRPrinter::visitCall(Call *e)
{
    *out << "call ";
    e->base->accept(this);
    *out << '(';
    for (ExprList *it = e->args; it; it = it->next) {
        if (it != e->args)
            *out << ", ";
        it->expr->accept(this);
    }
    *out << ')';
}

void IRPrinter::visitNew(New *e)
{
    *out << "new ";
    e->base->accept(this);
    *out << '(';
    for (ExprList *it = e->args; it; it = it->next) {
        if (it != e->args)
            *out << ", ";
        it->expr->accept(this);
    }
    *out << ')';
}

void IRPrinter::visitSubscript(Subscript *e)
{
    e->base->accept(this);
    *out << '[';
    e->index->accept(this);
    *out << ']';
}

void IRPrinter::visitMember(Member *e)
{
    if (e->kind != Member::MemberOfEnum
            && e->attachedPropertiesIdOrEnumValue != 0 && !e->base->asTemp())
        *out << "[[attached property from " << e->attachedPropertiesIdOrEnumValue << "]]";
    else
        e->base->accept(this);
    *out << '.' << *e->name;
#ifndef V4_BOOTSTRAP
    if (e->property)
        *out << " (meta-property " << e->property->coreIndex
            << " <" << QMetaType::typeName(e->property->propType)
            << ">)";
#endif
}

QString IRPrinter::escape(const QString &s)
{
    QString r;
    for (int i = 0; i < s.length(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('\n'))
            r += QStringLiteral("\\n");
        else if (ch == QLatin1Char('\r'))
            r += QStringLiteral("\\r");
        else if (ch == QLatin1Char('\\'))
            r += QStringLiteral("\\\\");
        else if (ch == QLatin1Char('"'))
            r += QStringLiteral("\\\"");
        else if (ch == QLatin1Char('\''))
            r += QStringLiteral("\\'");
        else
            r += ch;
    }
    return r;
}

void IRPrinter::addStmtNr(Stmt *s)
{
    if (s->id() >= 0)
        addJustifiedNr(s->id());
}

void IRPrinter::addJustifiedNr(int pos)
{
    if (positionSize == Stmt::InvalidId) {
        *out << pos << ": ";
    } else {
        QString posStr;
        if (pos != Stmt::InvalidId)
            posStr = QString::number(pos);
        *out << posStr.rightJustified(positionSize);
        if (pos == Stmt::InvalidId)
            *out << "  ";
        else
            *out << ": ";
    }
}

void IRPrinter::printBlockStart()
{
    if (currentBB->isRemoved()) {
        *out << "(block has been removed)";
        return;
    }

    QByteArray str;
    str.append('L');
    str.append(QByteArray::number(currentBB->index()));
    str.append(':');
    if (currentBB->catchBlock) {
        str.append(" (exception handler L");
        str.append(QByteArray::number(currentBB->catchBlock->index()));
        str.append(')');
    }
    for (int i = 66 - str.length(); i; --i)
        str.append(' ');
    *out << str;

    *out << "; predecessors:";
    foreach (BasicBlock *in, currentBB->in)
        *out << " L" << in->index();
    if (currentBB->in.isEmpty())
        *out << " none";
    if (BasicBlock *container = currentBB->containingGroup())
        *out << ", container: L" << container->index();
    if (currentBB->isGroupStart())
        *out << ", loop_header: yes";
    *out << endl;
}

} // end of namespace IR
} // end of namespace QV4

QT_END_NAMESPACE
