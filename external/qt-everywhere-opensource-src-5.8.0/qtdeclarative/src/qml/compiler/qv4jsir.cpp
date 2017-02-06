/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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

#include <vector>

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

class RemoveSharedExpressions
{
    CloneExpr clone;
    std::vector<Expr *> subexpressions; // contains all the non-cloned subexpressions in the given function. sorted using std::lower_bound.
    Expr *uniqueExpr;

public:
    RemoveSharedExpressions(): uniqueExpr(0) {}

    void operator()(IR::Function *function)
    {
        subexpressions.clear();
        subexpressions.reserve(function->basicBlockCount() * 8);

        for (BasicBlock *block : function->basicBlocks()) {
            if (block->isRemoved())
                continue;
            clone.setBasicBlock(block);

            for (Stmt *s : block->statements()) {
                visit(s);
            }
        }
    }

private:
    template <typename Expr_>
    Expr_ *cleanup(Expr_ *expr)
    {
        std::vector<Expr *>::iterator it = std::lower_bound(subexpressions.begin(), subexpressions.end(), expr);
        if (it == subexpressions.end() || *it != expr) {
            subexpressions.insert(it, expr);
            IR::Expr *e = expr;
            qSwap(uniqueExpr, e);
            visit(expr);
            qSwap(uniqueExpr, e);
            return static_cast<Expr_ *>(e);
        }

        // the cloned expression is unique by definition
        // so we don't need to add it to `subexpressions'.
        return clone(expr);
    }

    void visit(Stmt *s)
    {
        if (auto e = s->asExp()) {
            e->expr = cleanup(e->expr);
        } else if (auto m = s->asMove()) {
            m->target = cleanup(m->target);
            m->source = cleanup(m->source);
        } else if (auto c = s->asCJump()) {
            c->cond = cleanup(c->cond);
        } else if (auto r = s->asRet()) {
            r->expr = cleanup(r->expr);
        }
    }

    void visit(Expr *e)
    {
        if (auto c = e->asConvert()) {
            c->expr = cleanup(c->expr);
        } else if (auto u = e->asUnop()) {
            u->expr = cleanup(u->expr);
        } else if (auto b = e->asBinop()) {
            b->left = cleanup(b->left);
            b->right = cleanup(b->right);
        } else if (auto c = e->asCall()) {
            c->base = cleanup(c->base);
            for (IR::ExprList *it = c->args; it; it = it->next) {
                it->expr = cleanup(it->expr);
            }
        } else if (auto n = e->asNew()) {
            n->base = cleanup(n->base);
            for (IR::ExprList *it = n->args; it; it = it->next) {
                it->expr = cleanup(it->expr);
            }
        } else if (auto s = e->asSubscript()) {
            s->base = cleanup(s->base);
            s->index = cleanup(s->index);
        } else if (auto m = e->asMember()) {
            m->base = cleanup(m->base);
        }
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
    case IR::Name::builtin_qml_context:
        return "builtin_qml_context";
    case IR::Name::builtin_qml_imported_scripts_object:
        return "builtin_qml_imported_scripts_object";
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
    , isQmlBinding(false)
    , unused(0)
    , line(0)
    , column(0)
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
    for (BasicBlock *bb : basicBlocks())
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

void BasicBlock::setStatements(const QVector<Stmt *> &newStatements)
{
    Q_ASSERT(!isRemoved());
    Q_ASSERT(newStatements.size() >= _statements.size());
    for (Stmt *s : qAsConst(_statements)) {
        if (Phi *p = s->asPhi()) {
            if (!newStatements.contains(p)) {
                // phi-node was not copied over, so:
                p->destroyData();
            }
        } else {
            break;
        }
    }
    _statements = newStatements;
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

void CloneExpr::visit(Expr *e)
{
    if (auto c = e->asConst()) {
        cloned = cloneConst(c, block->function);
    } else if (auto s = e->asString()) {
        cloned = block->STRING(s->value);
    } else if (auto r = e->asRegExp()) {
        cloned = block->REGEXP(r->value, r->flags);
    } else if (auto n = e->asName()) {
        cloned = cloneName(n, block->function);
    } else if (auto t = e->asTemp()) {
        cloned = cloneTemp(t, block->function);
    } else if (auto a = e->asArgLocal()) {
        cloned = cloneArgLocal(a, block->function);
    } else if (auto c = e->asClosure()) {
        cloned = block->CLOSURE(c->value);
    } else if (auto c = e->asConvert()) {
        cloned = block->CONVERT(clone(c->expr), c->type);
    } else if (auto u = e->asUnop()) {
        cloned = block->UNOP(u->op, clone(u->expr));
    } else if (auto b = e->asBinop()) {
        cloned = block->BINOP(b->op, clone(b->left), clone(b->right));
    } else if (auto c = e->asCall()) {
        cloned = block->CALL(clone(c->base), clone(c->args));
    } else if (auto n = e->asNew()) {
        cloned = block->NEW(clone(n->base), clone(n->args));
    } else if (auto s = e->asSubscript()) {
        cloned = block->SUBSCRIPT(clone(s->base), clone(s->index));
    } else if (auto m = e->asMember()) {
        cloned = block->MEMBER(clone(m->base), m->name, m->property, m->kind, m->idIndex);
    } else {
        Q_UNREACHABLE();
    }
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
    visit(s);
}

void IRPrinter::print(const Expr &e)
{
    visit(const_cast<Expr *>(&e));
}

void IRPrinter::print(Expr *e)
{
    visit(e);
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

    for (const QString *local : qAsConst(f->locals))
        *out << "    local var " << *local << endl;

    bool needsSeperator = !f->locals.isEmpty();
    for (BasicBlock *bb : f->basicBlocks()) {
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

    for (Stmt *s : currentBB->statements()) {
        if (!s)
            continue;

        QByteArray str;
        QBuffer buf(&str);
        buf.open(QIODevice::WriteOnly);
        QTextStream os(&buf);
        QTextStream *prevOut = &os;
        std::swap(out, prevOut);
        addStmtNr(s);
        visit(s);
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

void IRPrinter::visit(Stmt *s)
{
    if (auto e = s->asExp()) {
        visitExp(e);
    } else if (auto m = s->asMove()) {
        visitMove(m);
    } else if (auto j = s->asJump()) {
        visitJump(j);
    } else if (auto c = s->asCJump()) {
        visitCJump(c);
    } else if (auto r = s->asRet()) {
        visitRet(r);
    } else if (auto p = s->asPhi()) {
        visitPhi(p);
    } else {
        Q_UNREACHABLE();
    }
}

void IRPrinter::visitExp(Exp *s)
{
    *out << "void ";
    visit(s->expr);
}

void IRPrinter::visitMove(Move *s)
{
    if (Temp *targetTemp = s->target->asTemp())
        if (!s->swap && targetTemp->type != UnknownType)
            *out << typeName(targetTemp->type) << ' ';

    visit(s->target);
    *out << ' ';
    if (s->swap)
        *out << "<=> ";
    else
        *out << "= ";
    visit(s->source);
}

void IRPrinter::visitJump(Jump *s)
{
    *out << "goto L" << s->target->index();
}

void IRPrinter::visitCJump(CJump *s)
{
    *out << "if ";
    visit(s->cond);
    *out << " goto L" << s->iftrue->index()
         << " else goto L" << s->iffalse->index();
}

void IRPrinter::visitRet(Ret *s)
{
    *out << "return";
    if (s->expr) {
        *out << ' ';
        visit(s->expr);
    }
}

void IRPrinter::visitPhi(Phi *s)
{
    if (s->targetTemp->type != UnknownType)
        *out << typeName(s->targetTemp->type) << ' ';

    visit(s->targetTemp);
    *out << " = phi ";
    for (int i = 0, ei = s->incoming.size(); i < ei; ++i) {
        if (i > 0)
            *out << ", ";
        if (currentBB)
            *out << 'L' << currentBB->in.at(i)->index() << ": ";
        if (s->incoming[i])
            visit(s->incoming[i]);
    }
}

void IRPrinter::visit(Expr *e)
{
    if (auto c = e->asConst()) {
        visitConst(c);
    } else if (auto s = e->asString()) {
        visitString(s);
    } else if (auto r = e->asRegExp()) {
        visitRegExp(r);
    } else if (auto n = e->asName()) {
        visitName(n);
    } else if (auto t = e->asTemp()) {
        visitTemp(t);
    } else if (auto a = e->asArgLocal()) {
        visitArgLocal(a);
    } else if (auto c = e->asClosure()) {
        visitClosure(c);
    } else if (auto c = e->asConvert()) {
        visitConvert(c);
    } else if (auto u = e->asUnop()) {
        visitUnop(u);
    } else if (auto b = e->asBinop()) {
        visitBinop(b);
    } else if (auto c = e->asCall()) {
        visitCall(c);
    } else if (auto n = e->asNew()) {
        visitNew(n);
    } else if (auto s = e->asSubscript()) {
        visitSubscript(s);
    } else if (auto m = e->asMember()) {
        visitMember(m);
    } else {
        Q_UNREACHABLE();
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
        if (*e->id != QLatin1String("this"))
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
    visit(e->expr);
}

void IRPrinter::visitUnop(Unop *e)
{
    *out << opname(e->op) << ' ';
    visit(e->expr);
}

void IRPrinter::visitBinop(Binop *e)
{
    *out << opname(e->op) << ' ';
    visit(e->left);
    *out << ", ";
    visit(e->right);
}

void IRPrinter::visitCall(Call *e)
{
    *out << "call ";
    visit(e->base);
    *out << '(';
    for (ExprList *it = e->args; it; it = it->next) {
        if (it != e->args)
            *out << ", ";
        visit(it->expr);
    }
    *out << ')';
}

void IRPrinter::visitNew(New *e)
{
    *out << "new ";
    visit(e->base);
    *out << '(';
    for (ExprList *it = e->args; it; it = it->next) {
        if (it != e->args)
            *out << ", ";
        visit(it->expr);
    }
    *out << ')';
}

void IRPrinter::visitSubscript(Subscript *e)
{
    visit(e->base);
    *out << '[';
    visit(e->index);
    *out << ']';
}

void IRPrinter::visitMember(Member *e)
{
    if (e->kind != Member::MemberOfEnum && e->kind != Member::MemberOfIdObjectsArray
            && e->attachedPropertiesId != 0 && !e->base->asTemp())
        *out << "[[attached property from " << e->attachedPropertiesId << "]]";
    else
        visit(e->base);
    *out << '.' << *e->name;
#ifndef V4_BOOTSTRAP
    if (e->property)
        *out << " (meta-property " << e->property->coreIndex()
            << " <" << QMetaType::typeName(e->property->propType())
            << ">)";
    else if (e->kind == Member::MemberOfIdObjectsArray)
        *out << "(id object " << e->idIndex << ")";
#endif
}

QString IRPrinter::escape(const QString &s)
{
    QString r;
    for (int i = 0; i < s.length(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('\n'))
            r += QLatin1String("\\n");
        else if (ch == QLatin1Char('\r'))
            r += QLatin1String("\\r");
        else if (ch == QLatin1Char('\\'))
            r += QLatin1String("\\\\");
        else if (ch == QLatin1Char('"'))
            r += QLatin1String("\\\"");
        else if (ch == QLatin1Char('\''))
            r += QLatin1String("\\'");
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
    for (BasicBlock *in : qAsConst(currentBB->in))
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
