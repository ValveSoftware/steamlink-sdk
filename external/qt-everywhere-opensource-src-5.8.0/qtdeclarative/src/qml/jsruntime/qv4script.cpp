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

#include "qv4script_p.h"
#include <private/qv4mm_p.h>
#include "qv4functionobject_p.h"
#include "qv4function_p.h"
#include "qv4context_p.h"
#include "qv4debugging_p.h"
#include "qv4profiling_p.h"
#include "qv4scopedvalue_p.h"

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmlengine_p.h>
#include <private/qv4profiling_p.h>
#include <qv4jsir_p.h>
#include <qv4codegen_p.h>
#include <private/qqmlcontextwrapper_p.h>

#include <QtCore/QDebug>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace Heap {

struct QmlBindingWrapper : FunctionObject {
    void init(QV4::QmlContext *scope, Function *f);
};

}

struct QmlBindingWrapper : FunctionObject {
    V4_OBJECT2(QmlBindingWrapper, FunctionObject)

    static void call(const Managed *that, Scope &scope, CallData *callData);
};

}

QT_END_NAMESPACE

using namespace QV4;

DEFINE_OBJECT_VTABLE(QmlBindingWrapper);

void Heap::QmlBindingWrapper::init(QV4::QmlContext *scope, Function *f)
{
    Heap::FunctionObject::init(scope, scope->d()->engine->id_eval(), /*createProto = */ false);

    Q_ASSERT(scope->inUse());

    function = f;
    if (function)
        function->compilationUnit->addref();
}

void QmlBindingWrapper::call(const Managed *that, Scope &scope, CallData *callData)
{
    const QmlBindingWrapper *This = static_cast<const QmlBindingWrapper *>(that);
    ExecutionEngine *v4 = static_cast<const Object *>(that)->engine();
    if (v4->hasException) {
        scope.result = Encode::undefined();
        return;
    }
    CHECK_STACK_LIMITS(v4, scope);

    ExecutionContextSaver ctxSaver(scope);

    QV4::Function *f = This->function();
    if (!f) {
        scope.result = QV4::Encode::undefined();
        return;
    }

    Scoped<CallContext> ctx(scope, v4->currentContext->newCallContext(This, callData));
    v4->pushContext(ctx);

    scope.result = Q_V4_PROFILE(v4, f);
}

Script::Script(ExecutionEngine *v4, QmlContext *qml, CompiledData::CompilationUnit *compilationUnit)
    : line(0), column(0), scope(v4->rootContext()), strictMode(false), inheritContext(true), parsed(false)
    , compilationUnit(compilationUnit), vmFunction(0), parseAsBinding(true)
{
    if (qml)
        qmlContext.set(v4, *qml);

    parsed = true;

    vmFunction = compilationUnit ? compilationUnit->linkToEngine(v4) : 0;
}

Script::~Script()
{
}

void Script::parse()
{
    if (parsed)
        return;

    using namespace QQmlJS;

    parsed = true;

    ExecutionEngine *v4 = scope->engine();
    Scope valueScope(v4);

    MemoryManager::GCBlocker gcBlocker(v4->memoryManager);

    IR::Module module(v4->debugger() != 0);

    QQmlJS::Engine ee, *engine = &ee;
    Lexer lexer(engine);
    lexer.setCode(sourceCode, line, parseAsBinding);
    Parser parser(engine);

    const bool parsed = parser.parseProgram();

    foreach (const QQmlJS::DiagnosticMessage &m, parser.diagnosticMessages()) {
        if (m.isError()) {
            valueScope.engine->throwSyntaxError(m.message, sourceFile, m.loc.startLine, m.loc.startColumn);
            return;
        } else {
            qWarning() << sourceFile << ':' << m.loc.startLine << ':' << m.loc.startColumn
                      << ": warning: " << m.message;
        }
    }

    if (parsed) {
        using namespace AST;
        Program *program = AST::cast<Program *>(parser.rootNode());
        if (!program) {
            // if parsing was successful, and we have no program, then
            // we're done...:
            return;
        }

        QStringList inheritedLocals;
        if (inheritContext) {
            Scoped<CallContext> ctx(valueScope, scope);
            if (ctx) {
                for (Identifier * const *i = ctx->variables(), * const *ei = i + ctx->variableCount(); i < ei; ++i)
                    inheritedLocals.append(*i ? (*i)->string : QString());
            }
        }

        RuntimeCodegen cg(v4, strictMode);
        cg.generateFromProgram(sourceFile, sourceCode, program, &module, QQmlJS::Codegen::EvalCode, inheritedLocals);
        if (v4->hasException)
            return;

        QV4::Compiler::JSUnitGenerator jsGenerator(&module);
        QScopedPointer<EvalInstructionSelection> isel(v4->iselFactory->create(QQmlEnginePrivate::get(v4), v4->executableAllocator, &module, &jsGenerator));
        if (inheritContext)
            isel->setUseFastLookups(false);
        compilationUnit = isel->compile();
        vmFunction = compilationUnit->linkToEngine(v4);
    }

    if (!vmFunction) {
        // ### FIX file/line number
        ScopedObject error(valueScope, v4->newSyntaxErrorObject(QStringLiteral("Syntax error")));
        v4->throwError(error);
    }
}

ReturnedValue Script::run()
{
    if (!parsed)
        parse();
    if (!vmFunction)
        return Encode::undefined();

    QV4::ExecutionEngine *engine = scope->engine();
    QV4::Scope valueScope(engine);

    if (qmlContext.isUndefined()) {
        TemporaryAssignment<Function*> savedGlobalCode(engine->globalCode, vmFunction);

        ExecutionContextSaver ctxSaver(valueScope);
        ContextStateSaver stateSaver(valueScope, scope);
        scope->d()->strictMode = vmFunction->isStrict();
        scope->d()->lookups = vmFunction->compilationUnit->runtimeLookups;
        scope->d()->constantTable = vmFunction->compilationUnit->constants;
        scope->d()->compilationUnit = vmFunction->compilationUnit;

        return Q_V4_PROFILE(engine, vmFunction);
    } else {
        Scoped<QmlContext> qml(valueScope, qmlContext.value());
        ScopedFunctionObject f(valueScope, engine->memoryManager->allocObject<QmlBindingWrapper>(qml, vmFunction));
        ScopedCallData callData(valueScope);
        callData->thisObject = Primitive::undefinedValue();
        f->call(valueScope, callData);
        return valueScope.result.asReturnedValue();
    }
}

Function *Script::function()
{
    if (!parsed)
        parse();
    return vmFunction;
}

QQmlRefPointer<QV4::CompiledData::CompilationUnit> Script::precompile(IR::Module *module, Compiler::JSUnitGenerator *unitGenerator, ExecutionEngine *engine, const QUrl &url, const QString &source, QList<QQmlError> *reportedErrors, QQmlJS::Directives *directivesCollector)
{
    using namespace QQmlJS;
    using namespace QQmlJS::AST;

    QQmlJS::Engine ee;
    if (directivesCollector)
        ee.setDirectives(directivesCollector);
    QQmlJS::Lexer lexer(&ee);
    lexer.setCode(source, /*line*/1, /*qml mode*/false);
    QQmlJS::Parser parser(&ee);

    parser.parseProgram();

    QList<QQmlError> errors;

    foreach (const QQmlJS::DiagnosticMessage &m, parser.diagnosticMessages()) {
        if (m.isWarning()) {
            qWarning("%s:%d : %s", qPrintable(url.toString()), m.loc.startLine, qPrintable(m.message));
            continue;
        }

        QQmlError error;
        error.setUrl(url);
        error.setDescription(m.message);
        error.setLine(m.loc.startLine);
        error.setColumn(m.loc.startColumn);
        errors << error;
    }

    if (!errors.isEmpty()) {
        if (reportedErrors)
            *reportedErrors << errors;
        return 0;
    }

    Program *program = AST::cast<Program *>(parser.rootNode());
    if (!program) {
        // if parsing was successful, and we have no program, then
        // we're done...:
        return 0;
    }

    QQmlJS::Codegen cg(/*strict mode*/false);
    cg.generateFromProgram(url.toString(), source, program, module, QQmlJS::Codegen::EvalCode);
    errors = cg.qmlErrors();
    if (!errors.isEmpty()) {
        if (reportedErrors)
            *reportedErrors << errors;
        return 0;
    }

    QScopedPointer<EvalInstructionSelection> isel(engine->iselFactory->create(QQmlEnginePrivate::get(engine), engine->executableAllocator, module, unitGenerator));
    isel->setUseFastLookups(false);
    return isel->compile(/*generate unit data*/false);
}

ReturnedValue Script::qmlBinding()
{
    if (!parsed)
        parse();
    ExecutionEngine *v4 = scope->engine();
    Scope valueScope(v4);
    Scoped<QmlContext> qml(valueScope, qmlContext.value());
    ScopedObject v(valueScope, v4->memoryManager->allocObject<QmlBindingWrapper>(qml, vmFunction));
    return v.asReturnedValue();
}

QV4::ReturnedValue Script::evaluate(ExecutionEngine *engine, const QString &script, QmlContext *qmlContext)
{
    QV4::Scope scope(engine);
    QV4::Script qmlScript(engine, qmlContext, script, QString());

    qmlScript.parse();
    QV4::ScopedValue result(scope);
    if (!scope.engine->hasException)
        result = qmlScript.run();
    if (scope.engine->hasException) {
        scope.engine->catchException();
        return Encode::undefined();
    }
    return result->asReturnedValue();
}
