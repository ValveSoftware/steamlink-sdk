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
#ifndef QV4JSIR_P_H
#define QV4JSIR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "private/qv4global_p.h"
#include <private/qqmljsmemorypool_p.h>
#include <private/qqmljsastfwd_p.h>
#include <private/qflagpointer_p.h>

#include <QtCore/private/qnumeric_p.h>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtCore/QBitArray>
#include <QtCore/qurl.h>
#include <QtCore/QVarLengthArray>
#include <qglobal.h>

#if defined(CONST) && defined(Q_OS_WIN)
# define QT_POP_CONST
# pragma push_macro("CONST")
# undef CONST // CONST conflicts with our own identifier
#endif

QT_BEGIN_NAMESPACE

class QTextStream;
class QQmlType;
class QQmlPropertyData;
class QQmlPropertyCache;
class QQmlEnginePrivate;

namespace QV4 {

inline bool isNegative(double d)
{
    uchar *dch = (uchar *)&d;
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
        return (dch[0] & 0x80);
    else
        return (dch[7] & 0x80);

}

namespace IR {

struct BasicBlock;
struct Function;
struct Module;

struct Stmt;
struct Expr;

// expressions
struct Const;
struct String;
struct RegExp;
struct Name;
struct Temp;
struct ArgLocal;
struct Closure;
struct Convert;
struct Unop;
struct Binop;
struct Call;
struct New;
struct Subscript;
struct Member;

// statements
struct Exp;
struct Move;
struct Jump;
struct CJump;
struct Ret;
struct Phi;

template<class T, int Prealloc>
class VarLengthArray: public QVarLengthArray<T, Prealloc>
{
public:
    bool removeOne(const T &element)
    {
        for (int i = 0; i < this->size(); ++i) {
            if (this->at(i) == element) {
                this->remove(i);
                return true;
            }
        }

        return false;
    }
};

// Flag pointer:
// * The first flag indicates whether the meta object is final.
//   If final, then none of its properties themselves need to
//   be final when considering for lookups in QML.
// * The second flag indicates whether enums should be included
//   in the lookup of properties or not. The default is false.
typedef QFlagPointer<QQmlPropertyCache> IRMetaObject;

enum AluOp {
    OpInvalid = 0,

    OpIfTrue,
    OpNot,
    OpUMinus,
    OpUPlus,
    OpCompl,
    OpIncrement,
    OpDecrement,

    OpBitAnd,
    OpBitOr,
    OpBitXor,

    OpAdd,
    OpSub,
    OpMul,
    OpDiv,
    OpMod,

    OpLShift,
    OpRShift,
    OpURShift,

    OpGt,
    OpLt,
    OpGe,
    OpLe,
    OpEqual,
    OpNotEqual,
    OpStrictEqual,
    OpStrictNotEqual,

    OpInstanceof,
    OpIn,

    OpAnd,
    OpOr,

    LastAluOp = OpOr
};
AluOp binaryOperator(int op);
const char *opname(IR::AluOp op);

enum Type : quint16 {
    UnknownType   = 0,

    MissingType   = 1 << 0,
    UndefinedType = 1 << 1,
    NullType      = 1 << 2,
    BoolType      = 1 << 3,

    SInt32Type    = 1 << 4,
    UInt32Type    = 1 << 5,
    DoubleType    = 1 << 6,
    NumberType    = SInt32Type | UInt32Type | DoubleType,

    StringType    = 1 << 7,
    QObjectType   = 1 << 8,
    VarType       = 1 << 9
};

inline bool strictlyEqualTypes(Type t1, Type t2)
{
    return t1 == t2 || ((t1 & NumberType) && (t2 & NumberType));
}

QString typeName(Type t);

struct MemberExpressionResolver;

struct DiscoveredType {
    int type;
    MemberExpressionResolver *memberResolver;

    DiscoveredType() : type(UnknownType), memberResolver(0) {}
    DiscoveredType(Type t) : type(t), memberResolver(0) { Q_ASSERT(type != QObjectType); }
    explicit DiscoveredType(int t) : type(t), memberResolver(0) { Q_ASSERT(type != QObjectType); }
    explicit DiscoveredType(MemberExpressionResolver *memberResolver)
        : type(QObjectType)
        , memberResolver(memberResolver)
    { Q_ASSERT(memberResolver); }

    bool test(Type t) const { return type & t; }
    bool isNumber() const { return (type & NumberType) && !(type & ~NumberType); }

    bool operator!=(Type other) const { return type != other; }
    bool operator==(Type other) const { return type == other; }
    bool operator==(const DiscoveredType &other) const { return type == other.type; }
    bool operator!=(const DiscoveredType &other) const { return type != other.type; }
};

struct MemberExpressionResolver
{
    typedef DiscoveredType (*ResolveFunction)(QQmlEnginePrivate *engine,
                                              const MemberExpressionResolver *resolver,
                                              Member *member);

    MemberExpressionResolver()
        : resolveMember(0), data(0), extraData(0), owner(nullptr), flags(0) {}

    bool isValid() const { return !!resolveMember; }
    void clear() { *this = MemberExpressionResolver(); }

    ResolveFunction resolveMember;
    void *data; // Could be pointer to meta object, importNameSpace, etc. - depends on resolveMember implementation
    void *extraData; // Could be QQmlTypeNameCache
    Function *owner;
    unsigned int flags;
};

struct Q_AUTOTEST_EXPORT Expr {
    enum ExprKind : quint8 {
        NameExpr,
        TempExpr,
        ArgLocalExpr,
        SubscriptExpr,
        MemberExpr,

        LastLValue = MemberExpr,

        ConstExpr,
        StringExpr,
        RegExpExpr,
        ClosureExpr,
        ConvertExpr,
        UnopExpr,
        BinopExpr,
        CallExpr,
        NewExpr
    };

    Type type;
    const ExprKind exprKind;

    Expr &operator=(const Expr &other) {
        Q_ASSERT(exprKind == other.exprKind);
        type = other.type;
        return *this;
    }

    template <typename To>
    inline bool isa() const {
        return To::classof(this);
    }

    template <typename To>
    inline To *as() {
        if (isa<To>()) {
            return static_cast<To *>(this);
        } else {
            return nullptr;
        }
    }

    template <typename To>
    inline const To *as() const {
        if (isa<To>()) {
            return static_cast<const To *>(this);
        } else {
            return nullptr;
        }
    }

    Expr(ExprKind exprKind): type(UnknownType), exprKind(exprKind) {}
    bool isLValue() const;

    Const *asConst() { return as<Const>(); }
    String *asString() { return as<String>(); }
    RegExp *asRegExp() { return as<RegExp>(); }
    Name *asName() { return as<Name>(); }
    Temp *asTemp() { return as<Temp>(); }
    ArgLocal *asArgLocal() { return as<ArgLocal>(); }
    Closure *asClosure() { return as<Closure>(); }
    Convert *asConvert() { return as<Convert>(); }
    Unop *asUnop() { return as<Unop>(); }
    Binop *asBinop() { return as<Binop>(); }
    Call *asCall() { return as<Call>(); }
    New *asNew() { return as<New>(); }
    Subscript *asSubscript() { return as<Subscript>(); }
    Member *asMember() { return as<Member>(); }
};

#define EXPR_VISIT_ALL_KINDS(e) \
    switch (e->exprKind) { \
    case QV4::IR::Expr::ConstExpr: \
        break; \
    case QV4::IR::Expr::StringExpr: \
        break; \
    case QV4::IR::Expr::RegExpExpr: \
        break; \
    case QV4::IR::Expr::NameExpr: \
        break; \
    case QV4::IR::Expr::TempExpr: \
        break; \
    case QV4::IR::Expr::ArgLocalExpr: \
        break; \
    case QV4::IR::Expr::ClosureExpr: \
        break; \
    case QV4::IR::Expr::ConvertExpr: { \
        auto casted = e->asConvert(); \
        visit(casted->expr); \
    } break; \
    case QV4::IR::Expr::UnopExpr: { \
        auto casted = e->asUnop(); \
        visit(casted->expr); \
    } break; \
    case QV4::IR::Expr::BinopExpr: { \
        auto casted = e->asBinop(); \
        visit(casted->left); \
        visit(casted->right); \
    } break; \
    case QV4::IR::Expr::CallExpr: { \
        auto casted = e->asCall(); \
        visit(casted->base); \
        for (QV4::IR::ExprList *it = casted->args; it; it = it->next) \
            visit(it->expr); \
    } break; \
    case QV4::IR::Expr::NewExpr: { \
        auto casted = e->asNew(); \
        visit(casted->base); \
        for (QV4::IR::ExprList *it = casted->args; it; it = it->next) \
            visit(it->expr); \
    } break; \
    case QV4::IR::Expr::SubscriptExpr: { \
        auto casted = e->asSubscript(); \
        visit(casted->base); \
        visit(casted->index); \
    } break; \
    case QV4::IR::Expr::MemberExpr: { \
        auto casted = e->asMember(); \
        visit(casted->base); \
    } break; \
    }

struct ExprList {
    Expr *expr;
    ExprList *next;

    ExprList(): expr(0), next(0) {}

    void init(Expr *expr, ExprList *next = 0)
    {
        this->expr = expr;
        this->next = next;
    }
};

struct Const: Expr {
    double value;

    Const(): Expr(ConstExpr) {}

    void init(Type type, double value)
    {
        this->type = type;
        this->value = value;
    }

    static bool classof(const Expr *c) { return c->exprKind == ConstExpr; }
};

struct String: Expr {
    const QString *value;

    String(): Expr(StringExpr) {}

    void init(const QString *value)
    {
        this->value = value;
    }

    static bool classof(const Expr *c) { return c->exprKind == StringExpr; }
};

struct RegExp: Expr {
    // needs to be compatible with the flags in the lexer, and in RegExpObject
    enum Flags {
        RegExp_Global     = 0x01,
        RegExp_IgnoreCase = 0x02,
        RegExp_Multiline  = 0x04
    };

    const QString *value;
    int flags;

    RegExp(): Expr(RegExpExpr) {}

    void init(const QString *value, int flags)
    {
        this->value = value;
        this->flags = flags;
    }

    static bool classof(const Expr *c) { return c->exprKind == RegExpExpr; }
};

struct Name: Expr {
    enum Builtin {
        builtin_invalid,
        builtin_typeof,
        builtin_delete,
        builtin_throw,
        builtin_rethrow,
        builtin_unwind_exception,
        builtin_push_catch_scope,
        builtin_foreach_iterator_object,
        builtin_foreach_next_property_name,
        builtin_push_with_scope,
        builtin_pop_scope,
        builtin_declare_vars,
        builtin_define_array,
        builtin_define_object_literal,
        builtin_setup_argument_object,
        builtin_convert_this_to_object,
        builtin_qml_context,
        builtin_qml_imported_scripts_object
    };

    const QString *id;
    Builtin builtin;
    bool global : 1;
    bool qmlSingleton : 1;
    bool freeOfSideEffects : 1;
    quint32 line;
    quint32 column;

    Name(): Expr(NameExpr) {}

    void initGlobal(const QString *id, quint32 line, quint32 column);
    void init(const QString *id, quint32 line, quint32 column);
    void init(Builtin builtin, quint32 line, quint32 column);

    static bool classof(const Expr *c) { return c->exprKind == NameExpr; }
};

struct Q_AUTOTEST_EXPORT Temp: Expr {
    enum Kind {
        Invalid = 0,
        VirtualRegister,
        PhysicalRegister,
        StackSlot
    };

    unsigned index      : 28;
    unsigned isReadOnly :  1;
    unsigned kind       :  3;

    // Used when temp is used as base in member expression
    MemberExpressionResolver *memberResolver;

    Temp()
        : Expr(TempExpr)
        , index((1 << 28) - 1)
        , isReadOnly(0)
        , kind(Invalid)
        , memberResolver(0)
    {}

    void init(unsigned kind, unsigned index)
    {
        this->index = index;
        this->isReadOnly = false;
        this->kind = kind;
    }

    bool isInvalid() const { return kind == Invalid; }

    static bool classof(const Expr *c) { return c->exprKind == TempExpr; }
};

inline bool operator==(const Temp &t1, const Temp &t2) Q_DECL_NOTHROW
{ return t1.index == t2.index && t1.kind == t2.kind && t1.type == t2.type; }

inline bool operator!=(const Temp &t1, const Temp &t2) Q_DECL_NOTHROW
{ return !(t1 == t2); }

inline uint qHash(const Temp &t, uint seed = 0) Q_DECL_NOTHROW
{ return t.index ^ t.kind ^ seed; }

bool operator<(const Temp &t1, const Temp &t2) Q_DECL_NOTHROW;

struct Q_AUTOTEST_EXPORT ArgLocal: Expr {
    enum Kind {
        Formal = 0,
        ScopedFormal,
        Local,
        ScopedLocal
    };

    unsigned index;
    unsigned scope             : 29; // how many scopes outside the current one?
    unsigned kind              :  2;
    unsigned isArgumentsOrEval :  1;

    void init(unsigned kind, unsigned index, unsigned scope)
    {
        Q_ASSERT((kind == ScopedLocal && scope != 0) ||
                 (kind == ScopedFormal && scope != 0) ||
                 (scope == 0));

        this->kind = kind;
        this->index = index;
        this->scope = scope;
        this->isArgumentsOrEval = false;
    }

    ArgLocal(): Expr(ArgLocalExpr) {}

    bool operator==(const ArgLocal &other) const
    { return index == other.index && scope == other.scope && kind == other.kind; }

    static bool classof(const Expr *c) { return c->exprKind == ArgLocalExpr; }
};

struct Closure: Expr {
    int value; // index in _module->functions
    const QString *functionName;

    Closure(): Expr(ClosureExpr) {}

    void init(int functionInModule, const QString *functionName)
    {
        this->value = functionInModule;
        this->functionName = functionName;
    }

    static bool classof(const Expr *c) { return c->exprKind == ClosureExpr; }
};

struct Convert: Expr {
    Expr *expr;

    Convert(): Expr(ConvertExpr) {}

    void init(Expr *expr, Type type)
    {
        this->expr = expr;
        this->type = type;
    }

    static bool classof(const Expr *c) { return c->exprKind == ConvertExpr; }
};

struct Unop: Expr {
    Expr *expr;
    AluOp op;

    Unop(): Expr(UnopExpr) {}

    void init(AluOp op, Expr *expr)
    {
        this->op = op;
        this->expr = expr;
    }

    static bool classof(const Expr *c) { return c->exprKind == UnopExpr; }
};

struct Binop: Expr {
    Expr *left; // Temp or Const
    Expr *right; // Temp or Const
    AluOp op;

    Binop(): Expr(BinopExpr) {}

    void init(AluOp op, Expr *left, Expr *right)
    {
        this->op = op;
        this->left = left;
        this->right = right;
    }

    static bool classof(const Expr *c) { return c->exprKind == BinopExpr; }
};

struct Call: Expr {
    Expr *base; // Name, Member, Temp
    ExprList *args; // List of Temps

    Call(): Expr(CallExpr) {}

    void init(Expr *base, ExprList *args)
    {
        this->base = base;
        this->args = args;
    }

    Expr *onlyArgument() const {
        if (args && ! args->next)
            return args->expr;
        return 0;
    }

    static bool classof(const Expr *c) { return c->exprKind == CallExpr; }
};

struct New: Expr {
    Expr *base; // Name, Member, Temp
    ExprList *args; // List of Temps

    New(): Expr(NewExpr) {}

    void init(Expr *base, ExprList *args)
    {
        this->base = base;
        this->args = args;
    }

    Expr *onlyArgument() const {
        if (args && ! args->next)
            return args->expr;
        return 0;
    }

    static bool classof(const Expr *c) { return c->exprKind == NewExpr; }
};

struct Subscript: Expr {
    Expr *base;
    Expr *index;

    Subscript(): Expr(SubscriptExpr) {}

    void init(Expr *base, Expr *index)
    {
        this->base = base;
        this->index = index;
    }

    static bool classof(const Expr *c) { return c->exprKind == SubscriptExpr; }
};

struct Member: Expr {
    // Used for property dependency tracking
    enum MemberKind {
        UnspecifiedMember,
        MemberOfEnum,
        MemberOfQmlScopeObject,
        MemberOfQmlContextObject,
        MemberOfIdObjectsArray,
        MemberOfSingletonObject,
    };

    Expr *base;
    const QString *name;
    QQmlPropertyData *property;
    union {  // depending on kind
        int attachedPropertiesId;
        int enumValue;
        int idIndex;
    };
    uchar freeOfSideEffects : 1;

    // This is set for example for for QObject properties. All sorts of extra behavior
    // is defined when writing to them, for example resettable properties are reset
    // when writing undefined to them, and an exception is thrown when they're missing
    // a reset function. And then there's also Qt.binding().
    uchar inhibitTypeConversionOnWrite: 1;

    uchar kind: 3; // MemberKind

    Member(): Expr(MemberExpr) {}

    void setEnumValue(int value) {
        kind = MemberOfEnum;
        enumValue = value;
    }

    void setAttachedPropertiesId(int id) {
        Q_ASSERT(kind != MemberOfEnum && kind != MemberOfIdObjectsArray);
        attachedPropertiesId = id;
    }

    void init(Expr *base, const QString *name, QQmlPropertyData *property = 0, uchar kind = UnspecifiedMember, int index = 0)
    {
        this->base = base;
        this->name = name;
        this->property = property;
        this->idIndex = index;
        this->freeOfSideEffects = false;
        this->inhibitTypeConversionOnWrite = property != 0;
        this->kind = kind;
    }

    static bool classof(const Expr *c) { return c->exprKind == MemberExpr; }
};

inline bool Expr::isLValue() const {
    if (auto t = as<Temp>())
        return !t->isReadOnly;
    return exprKind <= LastLValue;
}

struct Stmt {
    enum StmtKind: quint8 {
        MoveStmt,
        ExpStmt,
        JumpStmt,
        CJumpStmt,
        RetStmt,
        PhiStmt
    };

    template <typename To>
    inline bool isa() const {
        return To::classof(this);
    }

    template <typename To>
    inline To *as() {
        if (isa<To>())
            return static_cast<To *>(this);
        else
            return nullptr;
    }

    enum { InvalidId = -1 };

    QQmlJS::AST::SourceLocation location;

    explicit Stmt(int id, StmtKind stmtKind): _id(id), stmtKind(stmtKind) {}

    Stmt *asTerminator();

    Exp *asExp() { return as<Exp>(); }
    Move *asMove() { return as<Move>(); }
    Jump *asJump() { return as<Jump>(); }
    CJump *asCJump() { return as<CJump>(); }
    Ret *asRet() { return as<Ret>(); }
    Phi *asPhi() { return as<Phi>(); }

    int id() const { return _id; }

private: // For memory management in BasicBlock
    friend struct BasicBlock;

private:
    friend struct Function;
    int _id;

public:
    const StmtKind stmtKind;
};

#define STMT_VISIT_ALL_KINDS(s) \
    switch (s->stmtKind) { \
    case QV4::IR::Stmt::MoveStmt: { \
        auto casted = s->asMove(); \
        visit(casted->target); \
        visit(casted->source); \
    } break; \
    case QV4::IR::Stmt::ExpStmt: { \
        auto casted = s->asExp(); \
        visit(casted->expr); \
    } break; \
    case QV4::IR::Stmt::JumpStmt: \
        break; \
    case QV4::IR::Stmt::CJumpStmt: { \
        auto casted = s->asCJump(); \
        visit(casted->cond); \
    } break; \
    case QV4::IR::Stmt::RetStmt: { \
        auto casted = s->asRet(); \
        visit(casted->expr); \
    } break; \
    case QV4::IR::Stmt::PhiStmt: { \
        auto casted = s->asPhi(); \
        visit(casted->targetTemp); \
        for (auto *e : casted->incoming) { \
            visit(e); \
        } \
    } break; \
    }

struct Exp: Stmt {
    Expr *expr;

    Exp(int id): Stmt(id, ExpStmt) {}

    void init(Expr *expr)
    {
        this->expr = expr;
    }

    static bool classof(const Stmt *c) { return c->stmtKind == ExpStmt; }
};

struct Move: Stmt {
    Expr *target; // LHS - Temp, Name, Member or Subscript
    Expr *source;
    bool swap;

    Move(int id): Stmt(id, MoveStmt) {}

    void init(Expr *target, Expr *source)
    {
        this->target = target;
        this->source = source;
        this->swap = false;
    }

    static bool classof(const Stmt *c) { return c->stmtKind == MoveStmt; }
};

struct Jump: Stmt {
    BasicBlock *target;

    Jump(int id): Stmt(id, JumpStmt) {}

    void init(BasicBlock *target)
    {
        this->target = target;
    }

    static bool classof(const Stmt *c) { return c->stmtKind == JumpStmt; }
};

struct CJump: Stmt {
    Expr *cond; // Temp, Binop
    BasicBlock *iftrue;
    BasicBlock *iffalse;
    BasicBlock *parent;

    CJump(int id): Stmt(id, CJumpStmt) {}

    void init(Expr *cond, BasicBlock *iftrue, BasicBlock *iffalse, BasicBlock *parent)
    {
        this->cond = cond;
        this->iftrue = iftrue;
        this->iffalse = iffalse;
        this->parent = parent;
    }

    static bool classof(const Stmt *c) { return c->stmtKind == CJumpStmt; }
};

struct Ret: Stmt {
    Expr *expr;

    Ret(int id): Stmt(id, RetStmt) {}

    void init(Expr *expr)
    {
        this->expr = expr;
    }

    static bool classof(const Stmt *c) { return c->stmtKind == RetStmt; }
};

// Phi nodes can only occur at the start of a basic block. If there are any, they need to be
// subsequent to eachother, and the first phi node should be the first statement in the basic-block.
// A number of loops rely on this behavior, so they don't need to walk through the whole list
// of instructions in a basic-block (e.g. the calls to destroyData in BasicBlock::~BasicBlock).
struct Phi: Stmt {
    Temp *targetTemp;
    VarLengthArray<Expr *, 4> incoming;

    Phi(int id): Stmt(id, PhiStmt) {}

    static bool classof(const Stmt *c) { return c->stmtKind == PhiStmt; }

    void destroyData()
    { incoming.~VarLengthArray(); }
};

inline Stmt *Stmt::asTerminator()
{
    if (auto s = asJump()) {
        return s;
    } else if (auto s = asCJump()) {
        return s;
    } else if (auto s = asRet()) {
        return s;
    } else {
        return nullptr;
    }
}

struct Q_QML_PRIVATE_EXPORT Module {
    QQmlJS::MemoryPool pool;
    QVector<Function *> functions;
    Function *rootFunction;
    QString fileName;
    qint64 sourceTimeStamp;
    bool isQmlModule; // implies rootFunction is always 0
    uint unitFlags; // flags merged into CompiledData::Unit::flags
#ifdef QT_NO_QML_DEBUGGER
    static const bool debugMode = false;
#else
    bool debugMode;
#endif

    Function *newFunction(const QString &name, Function *outer);

    Module(bool debugMode)
        : rootFunction(0)
        , sourceTimeStamp(0)
        , isQmlModule(false)
        , unitFlags(0)
#ifndef QT_NO_QML_DEBUGGER
        , debugMode(debugMode)
    {}
#else
    { Q_UNUSED(debugMode); }
#endif
    ~Module();

    void setFileName(const QString &name);
};

struct BasicBlock {
private:
    Q_DISABLE_COPY(BasicBlock)

public:
    typedef VarLengthArray<BasicBlock *, 4> IncomingEdges;
    typedef VarLengthArray<BasicBlock *, 2> OutgoingEdges;

    Function *function;
    BasicBlock *catchBlock;
    IncomingEdges in;
    OutgoingEdges out;
    QQmlJS::AST::SourceLocation nextLocation;

    BasicBlock(Function *function, BasicBlock *catcher)
        : function(function)
        , catchBlock(catcher)
        , _containingGroup(0)
        , _index(-1)
        , _isExceptionHandler(false)
        , _groupStart(false)
        , _isRemoved(false)
    {}

    ~BasicBlock()
    {
        for (Stmt *s : qAsConst(_statements)) {
            if (Phi *p = s->asPhi()) {
                p->destroyData();
            } else {
                break;
            }
        }
    }

    const QVector<Stmt *> &statements() const
    {
        Q_ASSERT(!isRemoved());
        return _statements;
    }

    int statementCount() const
    {
        Q_ASSERT(!isRemoved());
        return _statements.size();
    }

    void setStatements(const QVector<Stmt *> &newStatements);

    template <typename Instr> inline Instr i(Instr i)
    {
        Q_ASSERT(!isRemoved());
        appendStatement(i);
        return i;
    }

    void appendStatement(Stmt *statement)
    {
        Q_ASSERT(!isRemoved());
        if (nextLocation.startLine)
            statement->location = nextLocation;
        _statements.append(statement);
    }

    void prependStatement(Stmt *stmt)
    {
        Q_ASSERT(!isRemoved());
        _statements.prepend(stmt);
    }

    void prependStatements(const QVector<Stmt *> &stmts)
    {
        Q_ASSERT(!isRemoved());
        QVector<Stmt *> newStmts = stmts;
        newStmts += _statements;
        _statements = newStmts;
    }

    void insertStatementBefore(Stmt *before, Stmt *newStmt)
    {
        int idx = _statements.indexOf(before);
        Q_ASSERT(idx >= 0);
        _statements.insert(idx, newStmt);
    }

    void insertStatementBefore(int index, Stmt *newStmt)
    {
        Q_ASSERT(index >= 0);
        _statements.insert(index, newStmt);
    }

    void insertStatementBeforeTerminator(Stmt *stmt)
    {
        Q_ASSERT(!isRemoved());
        _statements.insert(_statements.size() - 1, stmt);
    }

    void replaceStatement(int index, Stmt *newStmt)
    {
        Q_ASSERT(!isRemoved());
        if (Phi *p = _statements[index]->asPhi()) {
            p->destroyData();
        }
        _statements[index] = newStmt;
    }

    void removeStatement(Stmt *stmt)
    {
        Q_ASSERT(!isRemoved());
        if (Phi *p = stmt->asPhi()) {
            p->destroyData();
        }
        _statements.remove(_statements.indexOf(stmt));
    }

    void removeStatement(int idx)
    {
        Q_ASSERT(!isRemoved());
        if (Phi *p = _statements[idx]->asPhi()) {
            p->destroyData();
        }
        _statements.remove(idx);
    }

    inline bool isEmpty() const {
        Q_ASSERT(!isRemoved());
        return _statements.isEmpty();
    }

    inline Stmt *terminator() const {
        Q_ASSERT(!isRemoved());
        if (! _statements.isEmpty() && _statements.last()->asTerminator() != 0)
            return _statements.last();
        return 0;
    }

    inline bool isTerminated() const {
        Q_ASSERT(!isRemoved());
        if (terminator() != 0)
            return true;
        return false;
    }

    unsigned newTemp();

    Temp *TEMP(unsigned kind);
    ArgLocal *ARG(unsigned index, unsigned scope);
    ArgLocal *LOCAL(unsigned index, unsigned scope);

    Expr *CONST(Type type, double value);
    Expr *STRING(const QString *value);
    Expr *REGEXP(const QString *value, int flags);

    Name *NAME(const QString &id, quint32 line, quint32 column);
    Name *NAME(Name::Builtin builtin, quint32 line, quint32 column);

    Name *GLOBALNAME(const QString &id, quint32 line, quint32 column);

    Closure *CLOSURE(int functionInModule);

    Expr *CONVERT(Expr *expr, Type type);
    Expr *UNOP(AluOp op, Expr *expr);
    Expr *BINOP(AluOp op, Expr *left, Expr *right);
    Expr *CALL(Expr *base, ExprList *args = 0);
    Expr *NEW(Expr *base, ExprList *args = 0);
    Expr *SUBSCRIPT(Expr *base, Expr *index);
    Expr *MEMBER(Expr *base, const QString *name, QQmlPropertyData *property = 0, uchar kind = Member::UnspecifiedMember, int attachedPropertiesIdOrEnumValue = 0);

    Stmt *EXP(Expr *expr);

    Stmt *MOVE(Expr *target, Expr *source);

    Stmt *JUMP(BasicBlock *target);
    Stmt *CJUMP(Expr *cond, BasicBlock *iftrue, BasicBlock *iffalse);
    Stmt *RET(Expr *expr);

    BasicBlock *containingGroup() const
    {
        Q_ASSERT(!isRemoved());
        return _containingGroup;
    }

    void setContainingGroup(BasicBlock *loopHeader)
    {
        Q_ASSERT(!isRemoved());
        _containingGroup = loopHeader;
    }

    bool isGroupStart() const
    {
        Q_ASSERT(!isRemoved());
        return _groupStart;
    }

    void markAsGroupStart(bool mark = true)
    {
        Q_ASSERT(!isRemoved());
        _groupStart = mark;
    }

    // Returns the index of the basic-block.
    // See Function for the full description.
    int index() const
    {
        Q_ASSERT(!isRemoved());
        return _index;
    }

    bool isExceptionHandler() const
    { return _isExceptionHandler; }

    void setExceptionHandler(bool onoff)
    { _isExceptionHandler = onoff; }

    bool isRemoved() const
    { return _isRemoved; }

private: // For Function's eyes only.
    friend struct Function;
    void setIndex(int index)
    {
        Q_ASSERT(_index < 0);
        changeIndex(index);
    }

    void changeIndex(int index)
    {
        Q_ASSERT(index >= 0);
        _index = index;
    }

    void markAsRemoved()
    {
        _isRemoved = true;
        _index = -1;
    }

private:
    QVector<Stmt *> _statements;
    BasicBlock *_containingGroup;
    int _index;
    unsigned _isExceptionHandler : 1;
    unsigned _groupStart : 1;
    unsigned _isRemoved : 1;
};

template <typename T>
class SmallSet: public QVarLengthArray<T, 8>
{
public:
    void insert(int value)
    {
        for (auto it : *this) {
            if (it == value)
                return;
        }
        this->append(value);
    }
};

// Map from meta property index (existence implies dependency) to notify signal index
struct KeyValuePair
{
    quint32 _key;
    quint32 _value;

    KeyValuePair(): _key(0), _value(0) {}
    KeyValuePair(quint32 key, quint32 value): _key(key), _value(value) {}

    quint32 key() const { return _key; }
    quint32 value() const { return _value; }
};

class PropertyDependencyMap: public QVarLengthArray<KeyValuePair, 8>
{
public:
    void insert(quint32 key, quint32 value)
    {
        for (auto it = begin(), eit = end(); it != eit; ++it) {
            if (it->_key == key) {
                it->_value = value;
                return;
            }
        }
        append(KeyValuePair(key, value));
    }
};

// The Function owns (manages), among things, a list of basic-blocks. All the blocks have an index,
// which corresponds to the index in the entry/index in the vector in which they are stored. This
// means that algorithms/classes can also store any information about a basic block in an array,
// where the index corresponds to the index of the basic block, which can then be used to query
// the function for a pointer to a basic block. This also means that basic-blocks cannot be removed
// or renumbered.
//
// Note that currently there is one exception: after optimization and block scheduling, the
// method setScheduledBlocks can be called once, to register a newly ordered list. For debugging
// purposes, these blocks are not immediately renumbered, so renumberBasicBlocks should be called
// immediately after changing the order. That will restore the property of having a corresponding
// block-index and block-position-in-basicBlocks-vector.
//
// In order for optimization/transformation passes to skip uninteresting basic blocks that will be
// removed, the block can be marked as such. After doing so, any access will result in a failing
// assertion.
struct Function {
    Module *module;
    QQmlJS::MemoryPool *pool;
    const QString *name;
    int tempCount;
    int maxNumberOfArguments;
    QSet<QString> strings;
    QList<const QString *> formals;
    QList<const QString *> locals;
    QVector<Function *> nestedFunctions;
    Function *outer;

    int insideWithOrCatch;

    uint hasDirectEval: 1;
    uint usesArgumentsObject : 1;
    uint usesThis : 1;
    uint isStrict: 1;
    uint isNamedExpression : 1;
    uint hasTry: 1;
    uint hasWith: 1;
    uint isQmlBinding: 1;
    uint unused : 24;

    // Location of declaration in source code (0 if not specified)
    uint line;
    uint column;

    // Qml extension:
    SmallSet<int> idObjectDependencies;
    PropertyDependencyMap contextObjectPropertyDependencies;
    PropertyDependencyMap scopeObjectPropertyDependencies;

    template <typename T> T *New() { return new (pool->allocate(sizeof(T))) T(); }
    template <typename T> T *NewStmt() {
        return new (pool->allocate(sizeof(T))) T(getNewStatementId());
    }

    Function(Module *module, Function *outer, const QString &name);
    ~Function();

    enum BasicBlockInsertMode {
        InsertBlock,
        DontInsertBlock
    };

    BasicBlock *newBasicBlock(BasicBlock *catchBlock, BasicBlockInsertMode mode = InsertBlock);
    const QString *newString(const QString &text);

    void RECEIVE(const QString &name) { formals.append(newString(name)); }
    void LOCAL(const QString &name) { locals.append(newString(name)); }

    BasicBlock *addBasicBlock(BasicBlock *block);
    void removeBasicBlock(BasicBlock *block);

    const QVector<BasicBlock *> &basicBlocks() const
    { return _basicBlocks; }

    BasicBlock *basicBlock(int idx) const
    { return _basicBlocks.at(idx); }

    int basicBlockCount() const
    { return _basicBlocks.size(); }

    int liveBasicBlocksCount() const;

    void removeSharedExpressions();

    int indexOfArgument(const QStringRef &string) const;

    bool variablesCanEscape() const
    { return hasDirectEval || !nestedFunctions.isEmpty() || module->debugMode; }

    void setScheduledBlocks(const QVector<BasicBlock *> &scheduled);
    void renumberBasicBlocks();

    int getNewStatementId() { return _statementCount++; }
    int statementCount() const { return _statementCount; }

private:
    BasicBlock *getOrCreateBasicBlock(int index);
    void setStatementCount(int cnt);

private:
    QVector<BasicBlock *> _basicBlocks;
    QVector<BasicBlock *> *_allBasicBlocks;
    int _statementCount;
};

class CloneExpr
{
public:
    explicit CloneExpr(IR::BasicBlock *block = 0);

    void setBasicBlock(IR::BasicBlock *block);

    template <typename ExprSubclass>
    ExprSubclass *operator()(ExprSubclass *expr)
    {
        return clone(expr);
    }

    template <typename ExprSubclass>
    ExprSubclass *clone(ExprSubclass *expr)
    {
        Expr *c = expr;
        qSwap(cloned, c);
        visit(expr);
        qSwap(cloned, c);
        return static_cast<ExprSubclass *>(c);
    }

    static Const *cloneConst(Const *c, Function *f)
    {
        Const *newConst = f->New<Const>();
        newConst->init(c->type, c->value);
        return newConst;
    }

    static Name *cloneName(Name *n, Function *f)
    {
        Name *newName = f->New<Name>();
        newName->type = n->type;
        newName->id = n->id;
        newName->builtin = n->builtin;
        newName->global = n->global;
        newName->qmlSingleton = n->qmlSingleton;
        newName->freeOfSideEffects = n->freeOfSideEffects;
        newName->line = n->line;
        newName->column = n->column;
        return newName;
    }

    static Temp *cloneTemp(Temp *t, Function *f)
    {
        Temp *newTemp = f->New<Temp>();
        newTemp->init(t->kind, t->index);
        newTemp->type = t->type;
        newTemp->memberResolver = t->memberResolver;
        return newTemp;
    }

    static ArgLocal *cloneArgLocal(ArgLocal *argLocal, Function *f)
    {
        ArgLocal *newArgLocal = f->New<ArgLocal>();
        newArgLocal->init(argLocal->kind, argLocal->index, argLocal->scope);
        newArgLocal->type = argLocal->type;
        return newArgLocal;
    }

private:
    IR::ExprList *clone(IR::ExprList *list);

    void visit(Expr *e);

protected:
    IR::BasicBlock *block;

private:
    IR::Expr *cloned;
};

class Q_AUTOTEST_EXPORT IRPrinter
{
public:
    IRPrinter(QTextStream *out);
    virtual ~IRPrinter();

    void print(Stmt *s);
    void print(Expr *e);
    void print(const Expr &e);

    virtual void print(Function *f);
    virtual void print(BasicBlock *bb);

    void visit(Stmt *s);
    virtual void visitExp(Exp *s);
    virtual void visitMove(Move *s);
    virtual void visitJump(Jump *s);
    virtual void visitCJump(CJump *s);
    virtual void visitRet(Ret *s);
    virtual void visitPhi(Phi *s);

    void visit(Expr *e);
    virtual void visitConst(Const *e);
    virtual void visitString(String *e);
    virtual void visitRegExp(RegExp *e);
    virtual void visitName(Name *e);
    virtual void visitTemp(Temp *e);
    virtual void visitArgLocal(ArgLocal *e);
    virtual void visitClosure(Closure *e);
    virtual void visitConvert(Convert *e);
    virtual void visitUnop(Unop *e);
    virtual void visitBinop(Binop *e);
    virtual void visitCall(Call *e);
    virtual void visitNew(New *e);
    virtual void visitSubscript(Subscript *e);
    virtual void visitMember(Member *e);

    static QString escape(const QString &s);

protected:
    virtual void addStmtNr(Stmt *s);
    void addJustifiedNr(int pos);
    void printBlockStart();

protected:
    QTextStream *out;
    int positionSize;
    BasicBlock *currentBB;
};

inline unsigned BasicBlock::newTemp()
{
    Q_ASSERT(!isRemoved());
    return function->tempCount++;
}

inline Temp *BasicBlock::TEMP(unsigned index)
{
    Q_ASSERT(!isRemoved());
    Temp *e = function->New<Temp>();
    e->init(Temp::VirtualRegister, index);
    return e;
}

inline ArgLocal *BasicBlock::ARG(unsigned index, unsigned scope)
{
    Q_ASSERT(!isRemoved());
    ArgLocal *e = function->New<ArgLocal>();
    e->init(scope ? ArgLocal::ScopedFormal : ArgLocal::Formal, index, scope);
    return e;
}

inline ArgLocal *BasicBlock::LOCAL(unsigned index, unsigned scope)
{
    Q_ASSERT(!isRemoved());
    ArgLocal *e = function->New<ArgLocal>();
    e->init(scope ? ArgLocal::ScopedLocal : ArgLocal::Local, index, scope);
    return e;
}

inline Expr *BasicBlock::CONST(Type type, double value)
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
        value = qt_qnan();
    }

    e->init(type, value);
    return e;
}

inline Expr *BasicBlock::STRING(const QString *value)
{
    Q_ASSERT(!isRemoved());
    String *e = function->New<String>();
    e->init(value);
    return e;
}

inline Expr *BasicBlock::REGEXP(const QString *value, int flags)
{
    Q_ASSERT(!isRemoved());
    RegExp *e = function->New<RegExp>();
    e->init(value, flags);
    return e;
}

inline Name *BasicBlock::NAME(const QString &id, quint32 line, quint32 column)
{
    Q_ASSERT(!isRemoved());
    Name *e = function->New<Name>();
    e->init(function->newString(id), line, column);
    return e;
}

inline Name *BasicBlock::GLOBALNAME(const QString &id, quint32 line, quint32 column)
{
    Q_ASSERT(!isRemoved());
    Name *e = function->New<Name>();
    e->initGlobal(function->newString(id), line, column);
    return e;
}


inline Name *BasicBlock::NAME(Name::Builtin builtin, quint32 line, quint32 column)
{
    Q_ASSERT(!isRemoved());
    Name *e = function->New<Name>();
    e->init(builtin, line, column);
    return e;
}

inline Closure *BasicBlock::CLOSURE(int functionInModule)
{
    Q_ASSERT(!isRemoved());
    Closure *clos = function->New<Closure>();
    clos->init(functionInModule, function->module->functions.at(functionInModule)->name);
    return clos;
}

inline Expr *BasicBlock::CONVERT(Expr *expr, Type type)
{
    Q_ASSERT(!isRemoved());
    Convert *e = function->New<Convert>();
    e->init(expr, type);
    return e;
}

inline Expr *BasicBlock::UNOP(AluOp op, Expr *expr)
{
    Q_ASSERT(!isRemoved());
    Unop *e = function->New<Unop>();
    e->init(op, expr);
    return e;
}

inline Expr *BasicBlock::BINOP(AluOp op, Expr *left, Expr *right)
{
    Q_ASSERT(!isRemoved());
    Binop *e = function->New<Binop>();
    e->init(op, left, right);
    return e;
}

inline Expr *BasicBlock::CALL(Expr *base, ExprList *args)
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

inline Expr *BasicBlock::NEW(Expr *base, ExprList *args)
{
    Q_ASSERT(!isRemoved());
    New *e = function->New<New>();
    e->init(base, args);
    return e;
}

inline Expr *BasicBlock::SUBSCRIPT(Expr *base, Expr *index)
{
    Q_ASSERT(!isRemoved());
    Subscript *e = function->New<Subscript>();
    e->init(base, index);
    return e;
}

inline Expr *BasicBlock::MEMBER(Expr *base, const QString *name, QQmlPropertyData *property, uchar kind, int attachedPropertiesIdOrEnumValue)
{
    Q_ASSERT(!isRemoved());
    Member*e = function->New<Member>();
    e->init(base, name, property, kind, attachedPropertiesIdOrEnumValue);
    return e;
}

inline Stmt *BasicBlock::EXP(Expr *expr)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    Exp *s = function->NewStmt<Exp>();
    s->init(expr);
    appendStatement(s);
    return s;
}

inline Stmt *BasicBlock::MOVE(Expr *target, Expr *source)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    Move *s = function->NewStmt<Move>();
    s->init(target, source);
    appendStatement(s);
    return s;
}

inline Stmt *BasicBlock::JUMP(BasicBlock *target)
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

inline Stmt *BasicBlock::CJUMP(Expr *cond, BasicBlock *iftrue, BasicBlock *iffalse)
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

inline Stmt *BasicBlock::RET(Expr *expr)
{
    Q_ASSERT(!isRemoved());
    if (isTerminated())
        return 0;

    Ret *s = function->NewStmt<Ret>();
    s->init(expr);
    appendStatement(s);
    return s;
}

} // end of namespace IR

} // end of namespace QV4

QT_END_NAMESPACE

#if defined(QT_POP_CONST)
# pragma pop_macro("CONST") // Restore peace
# undef QT_POP_CONST
#endif

#endif // QV4IR_P_H
