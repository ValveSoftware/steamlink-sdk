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
#ifndef QV4CODEGEN_P_H
#define QV4CODEGEN_P_H

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
#include "qv4jsir_p.h"
#include <private/qqmljsastvisitor_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmljsengine_p.h>
#include <QtCore/QStringList>
#include <QStack>
#ifndef V4_BOOTSTRAP
#include <qqmlerror.h>
#endif
#include <private/qv4util_p.h>

QT_BEGIN_NAMESPACE

namespace QQmlJS {
namespace AST {
class UiParameterList;
}


class Q_QML_PRIVATE_EXPORT Codegen: protected AST::Visitor
{
public:
    Codegen(bool strict);

    enum CompilationMode {
        GlobalCode,
        EvalCode,
        FunctionCode,
        QmlBinding // This is almost the same as EvalCode, except:
                   //  * function declarations are moved to the return address when encountered
                   //  * return statements are allowed everywhere (like in FunctionCode)
                   //  * variable declarations are treated as true locals (like in FunctionCode)
    };

    void generateFromProgram(const QString &fileName,
                             const QString &sourceCode,
                             AST::Program *ast,
                             QV4::IR::Module *module,
                             CompilationMode mode = GlobalCode,
                             const QStringList &inheritedLocals = QStringList());
    void generateFromFunctionExpression(const QString &fileName,
                             const QString &sourceCode,
                             AST::FunctionExpression *ast,
                             QV4::IR::Module *module);

protected:
    enum Format { ex, cx, nx };
    struct Result {
        QV4::IR::Expr *code;
        QV4::IR::BasicBlock *iftrue;
        QV4::IR::BasicBlock *iffalse;
        Format format;
        Format requested;

        explicit Result(Format requested = ex)
            : code(0)
            , iftrue(0)
            , iffalse(0)
            , format(ex)
            , requested(requested) {}

        explicit Result(QV4::IR::BasicBlock *iftrue, QV4::IR::BasicBlock *iffalse)
            : code(0)
            , iftrue(iftrue)
            , iffalse(iffalse)
            , format(ex)
            , requested(cx) {}

        inline QV4::IR::Expr *operator*() const { Q_ASSERT(format == ex); return code; }
        inline QV4::IR::Expr *operator->() const { Q_ASSERT(format == ex); return code; }

        bool accept(Format f)
        {
            if (requested == f) {
                format = f;
                return true;
            }
            return false;
        }
    };

    struct Environment {
        Environment *parent;

        enum MemberType {
            UndefinedMember,
            VariableDefinition,
            VariableDeclaration,
            FunctionDefinition
        };
        struct Member {
            MemberType type;
            int index;
            AST::FunctionExpression *function;
        };
        typedef QMap<QString, Member> MemberMap;

        MemberMap members;
        AST::FormalParameterList *formals;
        int maxNumberOfArguments;
        bool hasDirectEval;
        bool hasNestedFunctions;
        bool isStrict;
        bool isNamedFunctionExpression;
        bool usesThis;
        enum UsesArgumentsObject {
            ArgumentsObjectUnknown,
            ArgumentsObjectNotUsed,
            ArgumentsObjectUsed
        };

        UsesArgumentsObject usesArgumentsObject;

        CompilationMode compilationMode;

        Environment(Environment *parent, CompilationMode mode)
            : parent(parent)
            , formals(0)
            , maxNumberOfArguments(0)
            , hasDirectEval(false)
            , hasNestedFunctions(false)
            , isStrict(false)
            , isNamedFunctionExpression(false)
            , usesThis(false)
            , usesArgumentsObject(ArgumentsObjectUnknown)
            , compilationMode(mode)
        {
            if (parent && parent->isStrict)
                isStrict = true;
        }

        int findMember(const QString &name) const
        {
            MemberMap::const_iterator it = members.find(name);
            if (it == members.end())
                return -1;
            Q_ASSERT((*it).index != -1 || !parent);
            return (*it).index;
        }

        bool lookupMember(const QString &name, Environment **scope, int *index, int *distance)
        {
            Environment *it = this;
            *distance = 0;
            for (; it; it = it->parent, ++(*distance)) {
                int idx = it->findMember(name);
                if (idx != -1) {
                    *scope = it;
                    *index = idx;
                    return true;
                }
            }
            return false;
        }

        void enter(const QString &name, MemberType type, AST::FunctionExpression *function = 0)
        {
            if (! name.isEmpty()) {
                if (type != FunctionDefinition) {
                    for (AST::FormalParameterList *it = formals; it; it = it->next)
                        if (it->name == name)
                            return;
                }
                MemberMap::iterator it = members.find(name);
                if (it == members.end()) {
                    Member m;
                    m.index = -1;
                    m.type = type;
                    m.function = function;
                    members.insert(name, m);
                } else {
                    if ((*it).type <= type) {
                        (*it).type = type;
                        (*it).function = function;
                    }
                }
            }
        }
    };

    struct TempScope {
        TempScope(QV4::IR::Function *f)
            : function(f),
              tempCountForScope(f->currentTemp) {}
        ~TempScope() {
            function->currentTemp = tempCountForScope;
        }
        QV4::IR::Function *function;
        int tempCountForScope;
    };

    Environment *newEnvironment(AST::Node *node, Environment *parent, CompilationMode compilationMode)
    {
        Environment *env = new Environment(parent, compilationMode);
        _envMap.insert(node, env);
        return env;
    }

    struct UiMember {
    };

    struct ScopeAndFinally {
        enum ScopeType {
            WithScope,
            TryScope,
            CatchScope
        };

        ScopeAndFinally *parent;
        AST::Finally *finally;
        ScopeType type;

        ScopeAndFinally(ScopeAndFinally *parent, ScopeType t = WithScope) : parent(parent), finally(0), type(t) {}
        ScopeAndFinally(ScopeAndFinally *parent, AST::Finally *finally)
        : parent(parent), finally(finally), type(TryScope)
        {}
    };

    struct Loop {
        AST::LabelledStatement *labelledStatement;
        AST::Statement *node;
        QV4::IR::BasicBlock *breakBlock;
        QV4::IR::BasicBlock *continueBlock;
        Loop *parent;
        ScopeAndFinally *scopeAndFinally;

        Loop(AST::Statement *node, QV4::IR::BasicBlock *breakBlock, QV4::IR::BasicBlock *continueBlock, Loop *parent)
            : labelledStatement(0), node(node), breakBlock(breakBlock), continueBlock(continueBlock), parent(parent) {}
    };

    void enterEnvironment(AST::Node *node);
    void leaveEnvironment();

    void enterLoop(AST::Statement *node, QV4::IR::BasicBlock *breakBlock, QV4::IR::BasicBlock *continueBlock);
    void leaveLoop();
    QV4::IR::BasicBlock *exceptionHandler() const
    {
        if (_exceptionHandlers.isEmpty())
            return 0;
        return _exceptionHandlers.top();
    }
    void pushExceptionHandler(QV4::IR::BasicBlock *handler)
    {
        handler->setExceptionHandler(true);
        _exceptionHandlers.push(handler);
    }
    void popExceptionHandler()
    {
        Q_ASSERT(!_exceptionHandlers.isEmpty());
        _exceptionHandlers.pop();
    }

    QV4::IR::Expr *member(QV4::IR::Expr *base, const QString *name);
    QV4::IR::Expr *argument(QV4::IR::Expr *expr);
    QV4::IR::Expr *reference(QV4::IR::Expr *expr);
    QV4::IR::Expr *unop(QV4::IR::AluOp op, QV4::IR::Expr *expr, const AST::SourceLocation &loc = AST::SourceLocation());
    QV4::IR::Expr *binop(QV4::IR::AluOp op, QV4::IR::Expr *left, QV4::IR::Expr *right, const AST::SourceLocation &loc = AST::SourceLocation());
    QV4::IR::Expr *call(QV4::IR::Expr *base, QV4::IR::ExprList *args);
    QV4::IR::Stmt *move(QV4::IR::Expr *target, QV4::IR::Expr *source, QV4::IR::AluOp op = QV4::IR::OpInvalid);
    QV4::IR::Stmt *cjump(QV4::IR::Expr *cond, QV4::IR::BasicBlock *iftrue, QV4::IR::BasicBlock *iffalse);

    // Returns index in _module->functions
    int defineFunction(const QString &name, AST::Node *ast,
                       AST::FormalParameterList *formals,
                       AST::SourceElements *body,
                       const QStringList &inheritedLocals = QStringList());

    void unwindException(ScopeAndFinally *outest);

    void statement(AST::Statement *ast);
    void statement(AST::ExpressionNode *ast);
    void condition(AST::ExpressionNode *ast, QV4::IR::BasicBlock *iftrue, QV4::IR::BasicBlock *iffalse);
    Result expression(AST::ExpressionNode *ast);
    Result sourceElement(AST::SourceElement *ast);
    UiMember uiObjectMember(AST::UiObjectMember *ast);

    void accept(AST::Node *node);

    void functionBody(AST::FunctionBody *ast);
    void program(AST::Program *ast);
    void sourceElements(AST::SourceElements *ast);
    void variableDeclaration(AST::VariableDeclaration *ast);
    void variableDeclarationList(AST::VariableDeclarationList *ast);

    QV4::IR::Expr *identifier(const QString &name, int line = 0, int col = 0);
    // Hook provided to implement QML lookup semantics
    virtual QV4::IR::Expr *fallbackNameLookup(const QString &name, int line, int col);
    virtual void beginFunctionBodyHook() {}

    // nodes
    bool visit(AST::ArgumentList *ast) override;
    bool visit(AST::CaseBlock *ast) override;
    bool visit(AST::CaseClause *ast) override;
    bool visit(AST::CaseClauses *ast) override;
    bool visit(AST::Catch *ast) override;
    bool visit(AST::DefaultClause *ast) override;
    bool visit(AST::ElementList *ast) override;
    bool visit(AST::Elision *ast) override;
    bool visit(AST::Finally *ast) override;
    bool visit(AST::FormalParameterList *ast) override;
    bool visit(AST::FunctionBody *ast) override;
    bool visit(AST::Program *ast) override;
    bool visit(AST::PropertyNameAndValue *ast) override;
    bool visit(AST::PropertyAssignmentList *ast) override;
    bool visit(AST::PropertyGetterSetter *ast) override;
    bool visit(AST::SourceElements *ast) override;
    bool visit(AST::StatementList *ast) override;
    bool visit(AST::UiArrayMemberList *ast) override;
    bool visit(AST::UiImport *ast) override;
    bool visit(AST::UiHeaderItemList *ast) override;
    bool visit(AST::UiPragma *ast) override;
    bool visit(AST::UiObjectInitializer *ast) override;
    bool visit(AST::UiObjectMemberList *ast) override;
    bool visit(AST::UiParameterList *ast) override;
    bool visit(AST::UiProgram *ast) override;
    bool visit(AST::UiQualifiedId *ast) override;
    bool visit(AST::UiQualifiedPragmaId *ast) override;
    bool visit(AST::VariableDeclaration *ast) override;
    bool visit(AST::VariableDeclarationList *ast) override;

    // expressions
    bool visit(AST::Expression *ast) override;
    bool visit(AST::ArrayLiteral *ast) override;
    bool visit(AST::ArrayMemberExpression *ast) override;
    bool visit(AST::BinaryExpression *ast) override;
    bool visit(AST::CallExpression *ast) override;
    bool visit(AST::ConditionalExpression *ast) override;
    bool visit(AST::DeleteExpression *ast) override;
    bool visit(AST::FalseLiteral *ast) override;
    bool visit(AST::FieldMemberExpression *ast) override;
    bool visit(AST::FunctionExpression *ast) override;
    bool visit(AST::IdentifierExpression *ast) override;
    bool visit(AST::NestedExpression *ast) override;
    bool visit(AST::NewExpression *ast) override;
    bool visit(AST::NewMemberExpression *ast) override;
    bool visit(AST::NotExpression *ast) override;
    bool visit(AST::NullExpression *ast) override;
    bool visit(AST::NumericLiteral *ast) override;
    bool visit(AST::ObjectLiteral *ast) override;
    bool visit(AST::PostDecrementExpression *ast) override;
    bool visit(AST::PostIncrementExpression *ast) override;
    bool visit(AST::PreDecrementExpression *ast) override;
    bool visit(AST::PreIncrementExpression *ast) override;
    bool visit(AST::RegExpLiteral *ast) override;
    bool visit(AST::StringLiteral *ast) override;
    bool visit(AST::ThisExpression *ast) override;
    bool visit(AST::TildeExpression *ast) override;
    bool visit(AST::TrueLiteral *ast) override;
    bool visit(AST::TypeOfExpression *ast) override;
    bool visit(AST::UnaryMinusExpression *ast) override;
    bool visit(AST::UnaryPlusExpression *ast) override;
    bool visit(AST::VoidExpression *ast) override;
    bool visit(AST::FunctionDeclaration *ast) override;

    // source elements
    bool visit(AST::FunctionSourceElement *ast) override;
    bool visit(AST::StatementSourceElement *ast) override;

    // statements
    bool visit(AST::Block *ast) override;
    bool visit(AST::BreakStatement *ast) override;
    bool visit(AST::ContinueStatement *ast) override;
    bool visit(AST::DebuggerStatement *ast) override;
    bool visit(AST::DoWhileStatement *ast) override;
    bool visit(AST::EmptyStatement *ast) override;
    bool visit(AST::ExpressionStatement *ast) override;
    bool visit(AST::ForEachStatement *ast) override;
    bool visit(AST::ForStatement *ast) override;
    bool visit(AST::IfStatement *ast) override;
    bool visit(AST::LabelledStatement *ast) override;
    bool visit(AST::LocalForEachStatement *ast) override;
    bool visit(AST::LocalForStatement *ast) override;
    bool visit(AST::ReturnStatement *ast) override;
    bool visit(AST::SwitchStatement *ast) override;
    bool visit(AST::ThrowStatement *ast) override;
    bool visit(AST::TryStatement *ast) override;
    bool visit(AST::VariableStatement *ast) override;
    bool visit(AST::WhileStatement *ast) override;
    bool visit(AST::WithStatement *ast) override;

    // ui object members
    bool visit(AST::UiArrayBinding *ast) override;
    bool visit(AST::UiObjectBinding *ast) override;
    bool visit(AST::UiObjectDefinition *ast) override;
    bool visit(AST::UiPublicMember *ast) override;
    bool visit(AST::UiScriptBinding *ast) override;
    bool visit(AST::UiSourceElement *ast) override;

    bool throwSyntaxErrorOnEvalOrArgumentsInStrictMode(QV4::IR::Expr* expr, const AST::SourceLocation &loc);
    virtual void throwSyntaxError(const AST::SourceLocation &loc, const QString &detail);
    virtual void throwReferenceError(const AST::SourceLocation &loc, const QString &detail);

public:
    QList<DiagnosticMessage> errors() const;
#ifndef V4_BOOTSTRAP
    QList<QQmlError> qmlErrors() const;
#endif

protected:
    Result _expr;
    QString _property;
    UiMember _uiMember;
    QV4::IR::Module *_module;
    QV4::IR::Function *_function;
    QV4::IR::BasicBlock *_block;
    QV4::IR::BasicBlock *_exitBlock;
    unsigned _returnAddress;
    Environment *_env;
    Loop *_loop;
    AST::LabelledStatement *_labelledStatement;
    ScopeAndFinally *_scopeAndFinally;
    QHash<AST::Node *, Environment *> _envMap;
    QHash<AST::FunctionExpression *, int> _functionMap;
    QStack<QV4::IR::BasicBlock *> _exceptionHandlers;
    bool _strictMode;

    bool _fileNameIsUrl;
    bool hasError;
    QList<QQmlJS::DiagnosticMessage> _errors;

    class ScanFunctions: protected Visitor
    {
        typedef QV4::TemporaryAssignment<bool> TemporaryBoolAssignment;
    public:
        ScanFunctions(Codegen *cg, const QString &sourceCode, CompilationMode defaultProgramMode);
        void operator()(AST::Node *node);

        void enterEnvironment(AST::Node *node, CompilationMode compilationMode);
        void leaveEnvironment();

        void enterQmlScope(AST::Node *ast, const QString &name)
        { enterFunction(ast, name, /*formals*/0, /*body*/0, /*expr*/0, /*isExpression*/false); }

        void enterQmlFunction(AST::FunctionDeclaration *ast)
        { enterFunction(ast, false, false); }

    protected:
        using Visitor::visit;
        using Visitor::endVisit;

        void checkDirectivePrologue(AST::SourceElements *ast);

        void checkName(const QStringRef &name, const AST::SourceLocation &loc);
        void checkForArguments(AST::FormalParameterList *parameters);

        bool visit(AST::Program *ast) override;
        void endVisit(AST::Program *) override;

        bool visit(AST::CallExpression *ast) override;
        bool visit(AST::NewMemberExpression *ast) override;
        bool visit(AST::ArrayLiteral *ast) override;
        bool visit(AST::VariableDeclaration *ast) override;
        bool visit(AST::IdentifierExpression *ast) override;
        bool visit(AST::ExpressionStatement *ast) override;
        bool visit(AST::FunctionExpression *ast) override;

        void enterFunction(AST::FunctionExpression *ast, bool enterName, bool isExpression = true);

        void endVisit(AST::FunctionExpression *) override;

        bool visit(AST::ObjectLiteral *ast) override;

        bool visit(AST::PropertyGetterSetter *ast) override;
        void endVisit(AST::PropertyGetterSetter *) override;

        bool visit(AST::FunctionDeclaration *ast) override;
        void endVisit(AST::FunctionDeclaration *) override;

        bool visit(AST::WithStatement *ast) override;

        bool visit(AST::DoWhileStatement *ast) override;
        bool visit(AST::ForStatement *ast) override;
        bool visit(AST::LocalForStatement *ast) override;
        bool visit(AST::ForEachStatement *ast) override;
        bool visit(AST::LocalForEachStatement *ast) override;
        bool visit(AST::ThisExpression *ast) override;

        bool visit(AST::Block *ast) override;

    protected:
        void enterFunction(AST::Node *ast, const QString &name, AST::FormalParameterList *formals, AST::FunctionBody *body, AST::FunctionExpression *expr, bool isExpression);

    // fields:
        Codegen *_cg;
        const QString _sourceCode;
        Environment *_env;
        QStack<Environment *> _envStack;

        bool _allowFuncDecls;
        CompilationMode defaultProgramMode;
    };

};

#ifndef V4_BOOTSTRAP
class RuntimeCodegen : public Codegen
{
public:
    RuntimeCodegen(QV4::ExecutionEngine *engine, bool strict)
        : Codegen(strict)
        , engine(engine)
    {}

    void throwSyntaxError(const AST::SourceLocation &loc, const QString &detail) override;
    void throwReferenceError(const AST::SourceLocation &loc, const QString &detail) override;
private:
    QV4::ExecutionEngine *engine;
};
#endif // V4_BOOTSTRAP

}

QT_END_NAMESPACE

#endif // QV4CODEGEN_P_H
