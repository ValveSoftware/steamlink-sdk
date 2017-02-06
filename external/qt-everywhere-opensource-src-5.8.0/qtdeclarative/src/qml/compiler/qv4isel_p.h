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

#ifndef QV4ISEL_P_H
#define QV4ISEL_P_H

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
#include <private/qv4compileddata_p.h>
#include <private/qv4compiler_p.h>

#include <qglobal.h>
#include <QHash>

QT_BEGIN_NAMESPACE

class QQmlEnginePrivate;

namespace QV4 {

class EvalISelFactory;
class ExecutableAllocator;
struct Function;

class Q_QML_PRIVATE_EXPORT EvalInstructionSelection
{
public:
    EvalInstructionSelection(QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator, EvalISelFactory *iselFactory);
    virtual ~EvalInstructionSelection() = 0;

    QQmlRefPointer<QV4::CompiledData::CompilationUnit> compile(bool generateUnitData = true);

    void setUseFastLookups(bool b) { useFastLookups = b; }
    void setUseTypeInference(bool onoff) { useTypeInference = onoff; }

    int registerString(const QString &str) { return jsGenerator->registerString(str); }
    uint registerIndexedGetterLookup() { return jsGenerator->registerIndexedGetterLookup(); }
    uint registerIndexedSetterLookup() { return jsGenerator->registerIndexedSetterLookup(); }
    uint registerGetterLookup(const QString &name) { return jsGenerator->registerGetterLookup(name); }
    uint registerSetterLookup(const QString &name) { return jsGenerator->registerSetterLookup(name); }
    uint registerGlobalGetterLookup(const QString &name) { return jsGenerator->registerGlobalGetterLookup(name); }
    int registerRegExp(IR::RegExp *regexp) { return jsGenerator->registerRegExp(regexp); }
    int registerJSClass(int count, IR::ExprList *args) { return jsGenerator->registerJSClass(count, args); }
    QV4::Compiler::JSUnitGenerator *jsUnitGenerator() const { return jsGenerator; }

protected:
    virtual void run(int functionIndex) = 0;
    virtual QQmlRefPointer<QV4::CompiledData::CompilationUnit> backendCompileStep() = 0;

    bool useFastLookups;
    bool useTypeInference;
    QV4::ExecutableAllocator *executableAllocator;
    QV4::Compiler::JSUnitGenerator *jsGenerator;
    QScopedPointer<QV4::Compiler::JSUnitGenerator> ownJSGenerator;
    IR::Module *irModule;
};

class Q_QML_PRIVATE_EXPORT EvalISelFactory
{
public:
    EvalISelFactory(const QString &codeGeneratorName) : codeGeneratorName(codeGeneratorName) {}
    virtual ~EvalISelFactory() = 0;
    virtual EvalInstructionSelection *create(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator) = 0;
    virtual bool jitCompileRegexps() const = 0;
    virtual QQmlRefPointer<QV4::CompiledData::CompilationUnit> createUnitForLoading() = 0;

    const QString codeGeneratorName;
};

namespace IR {
class Q_QML_PRIVATE_EXPORT IRDecoder
{
public:
    IRDecoder() : _function(0) {}
    virtual ~IRDecoder() = 0;

    void visit(Stmt *s)
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

private: // visitor methods for StmtVisitor:
    void visitMove(IR::Move *s);
    void visitExp(IR::Exp *s);

public: // to implement by subclasses:
    virtual void callBuiltinInvalid(IR::Name *func, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void callBuiltinTypeofQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::Expr *result) = 0;
    virtual void callBuiltinTypeofMember(IR::Expr *base, const QString &name, IR::Expr *result) = 0;
    virtual void callBuiltinTypeofSubscript(IR::Expr *base, IR::Expr *index, IR::Expr *result) = 0;
    virtual void callBuiltinTypeofName(const QString &name, IR::Expr *result) = 0;
    virtual void callBuiltinTypeofValue(IR::Expr *value, IR::Expr *result) = 0;
    virtual void callBuiltinDeleteMember(IR::Expr *base, const QString &name, IR::Expr *result) = 0;
    virtual void callBuiltinDeleteSubscript(IR::Expr *base, IR::Expr *index, IR::Expr *result) = 0;
    virtual void callBuiltinDeleteName(const QString &name, IR::Expr *result) = 0;
    virtual void callBuiltinDeleteValue(IR::Expr *result) = 0;
    virtual void callBuiltinThrow(IR::Expr *arg) = 0;
    virtual void callBuiltinReThrow() = 0;
    virtual void callBuiltinUnwindException(IR::Expr *) = 0;
    virtual void callBuiltinPushCatchScope(const QString &exceptionName) = 0;
    virtual void callBuiltinForeachIteratorObject(IR::Expr *arg, IR::Expr *result) = 0;
    virtual void callBuiltinForeachNextPropertyname(IR::Expr *arg, IR::Expr *result) = 0;
    virtual void callBuiltinPushWithScope(IR::Expr *arg) = 0;
    virtual void callBuiltinPopScope() = 0;
    virtual void callBuiltinDeclareVar(bool deletable, const QString &name) = 0;
    virtual void callBuiltinDefineArray(IR::Expr *result, IR::ExprList *args) = 0;
    virtual void callBuiltinDefineObjectLiteral(IR::Expr *result, int keyValuePairCount, IR::ExprList *keyValuePairs, IR::ExprList *arrayEntries, bool needSparseArray) = 0;
    virtual void callBuiltinSetupArgumentObject(IR::Expr *result) = 0;
    virtual void callBuiltinConvertThisToObject() = 0;
    virtual void callValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void callQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void callProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void callSubscript(IR::Expr *base, IR::Expr *index, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void convertType(IR::Expr *source, IR::Expr *target) = 0;
    virtual void constructActivationProperty(IR::Name *func, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void constructProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void constructValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result) = 0;
    virtual void loadThisObject(IR::Expr *target) = 0;
    virtual void loadQmlContext(IR::Expr *target) = 0;
    virtual void loadQmlImportedScripts(IR::Expr *target) = 0;
    virtual void loadQmlSingleton(const QString &name, IR::Expr *target) = 0;
    virtual void loadConst(IR::Const *sourceConst, IR::Expr *target) = 0;
    virtual void loadString(const QString &str, IR::Expr *target) = 0;
    virtual void loadRegexp(IR::RegExp *sourceRegexp, IR::Expr *target) = 0;
    virtual void getActivationProperty(const IR::Name *name, IR::Expr *target) = 0;
    virtual void setActivationProperty(IR::Expr *source, const QString &targetName) = 0;
    virtual void initClosure(IR::Closure *closure, IR::Expr *target) = 0;
    virtual void getProperty(IR::Expr *base, const QString &name, IR::Expr *target) = 0;
    virtual void getQObjectProperty(IR::Expr *base, int propertyIndex, bool captureRequired, bool isSingletonProperty, int attachedPropertiesId, IR::Expr *target) = 0;
    virtual void getQmlContextProperty(IR::Expr *source, IR::Member::MemberKind kind, int index, bool captureRequired, IR::Expr *target) = 0;
    virtual void setProperty(IR::Expr *source, IR::Expr *targetBase, const QString &targetName) = 0;
    virtual void setQmlContextProperty(IR::Expr *source, IR::Expr *targetBase, IR::Member::MemberKind kind, int propertyIndex) = 0;
    virtual void setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int propertyIndex) = 0;
    virtual void getElement(IR::Expr *base, IR::Expr *index, IR::Expr *target) = 0;
    virtual void setElement(IR::Expr *source, IR::Expr *targetBase, IR::Expr *targetIndex) = 0;
    virtual void copyValue(IR::Expr *source, IR::Expr *target) = 0;
    virtual void swapValues(IR::Expr *source, IR::Expr *target) = 0;
    virtual void unop(IR::AluOp oper, IR::Expr *source, IR::Expr *target) = 0;
    virtual void binop(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target) = 0;

protected:
    virtual void visitJump(IR::Jump *) = 0;
    virtual void visitCJump(IR::CJump *) = 0;
    virtual void visitRet(IR::Ret *) = 0;
    virtual void visitPhi(IR::Phi *) {}

    virtual void callBuiltin(IR::Call *c, IR::Expr *result);

    IR::Function *_function; // subclass needs to set
};
} // namespace IR

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4ISEL_P_H
