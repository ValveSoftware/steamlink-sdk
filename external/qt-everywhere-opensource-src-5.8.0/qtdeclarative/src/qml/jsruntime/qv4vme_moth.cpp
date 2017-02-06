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

#include "qv4vme_moth_p.h"
#include "qv4instr_moth_p.h"

#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

#include <private/qv4value_p.h>
#include <private/qv4debugging_p.h>
#include <private/qv4function_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4math_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4lookup_p.h>
#include <private/qv4string_p.h>
#include <iostream>

#include "qv4alloca_p.h"

#undef DO_TRACE_INSTR // define to enable instruction tracing

#ifdef DO_TRACE_INSTR
#  define TRACE_INSTR(I) qDebug("executing a %s\n", #I);
#  define TRACE(n, str, ...) { char buf[4096]; snprintf(buf, 4096, str, __VA_ARGS__); qDebug("    %s : %s", #n, buf); }
#else
#  define TRACE_INSTR(I)
#  define TRACE(n, str, ...)
#endif // DO_TRACE_INSTR

extern "C" {

// This is the interface to Qt Creator's (new) QML debugger.

/*! \internal
    \since 5.5

    This function is called uncondionally from VME::run().

    An attached debugger can set a breakpoint here to
    intercept calls to VME::run().
 */

Q_QML_EXPORT void qt_v4ResolvePendingBreakpointsHook()
{
}

/*! \internal
    \since 5.5

    This function is called when a QML interpreter breakpoint
    is hit.

    An attached debugger can set a breakpoint here.
*/
Q_QML_EXPORT void qt_v4TriggeredBreakpointHook()
{
}

/*! \internal
    \since 5.5

    The main entry point into "Native Mixed" Debugging.

    Commands are passed as UTF-8 encoded JSON data.
    The data has two compulsory fields:
    \list
    \li \c version: Version of the protocol (currently 1)
    \li \c command: Name of the command
    \endlist

    Depending on \c command, more fields can be present.

    Error is indicated by negative return values,
    success by non-negative return values.

    \c protocolVersion:
    Returns version of implemented protocol.

    \c insertBreakpoint:
    Sets a breakpoint on a given file and line.
    \list
    \li \c fullName: Name of the QML/JS file
    \li \c lineNumber: Line number in the file
    \li \c condition: Breakpoint condition
    \endlist
    Returns a unique positive number as handle.

    \c removeBreakpoint:
    Removes a breakpoint from a given file and line.
    \list
    \li \c fullName: Name of the QML/JS file
    \li \c lineNumber: Line number in the file
    \li \c condition: Breakpoint condition
    \endlist
    Returns zero on success, a negative number on failure.

    \c prepareStep:
    Puts the interpreter in stepping mode.
    Returns zero.

*/
Q_QML_EXPORT int qt_v4DebuggerHook(const char *json);


} // extern "C"

#ifndef QT_NO_QML_DEBUGGER
static int qt_v4BreakpointCount = 0;
static bool qt_v4IsDebugging = true;
static bool qt_v4IsStepping = false;

class Breakpoint
{
public:
    Breakpoint() : bpNumber(0), lineNumber(-1) {}

    bool matches(const QString &file, int line) const
    {
        return fullName == file && lineNumber == line;
    }

    int bpNumber;
    int lineNumber;
    QString fullName;      // e.g. /opt/project/main.qml
    QString engineName;    // e.g. qrc:/main.qml
    QString condition;     // optional
};

static QVector<Breakpoint> qt_v4Breakpoints;
static Breakpoint qt_v4LastStop;

static QV4::Function *qt_v4ExtractFunction(QV4::ExecutionContext *context)
{
    QV4::Scope scope(context->engine());
    QV4::ScopedFunctionObject function(scope, context->getFunctionObject());
    if (function)
        return function->function();
    else
        return context->d()->engine->globalCode;
}

static void qt_v4TriggerBreakpoint(const Breakpoint &bp, QV4::Function *function)
{
    qt_v4LastStop = bp;

    // Set up some auxiliary data for informational purpose.
    // This is not part of the protocol.
    QV4::Heap::String *functionName = function->name();
    QByteArray functionNameUtf8;
    if (functionName)
        functionNameUtf8 = functionName->toQString().toUtf8();

    qt_v4TriggeredBreakpointHook(); // Trigger Breakpoint.
}

int qt_v4DebuggerHook(const char *json)
{
    const int ProtocolVersion = 1;

    enum {
        Success = 0,
        WrongProtocol,
        NoSuchCommand,
        NoSuchBreakpoint
    };

    QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonObject ob = doc.object();
    QByteArray command = ob.value(QLatin1String("command")).toString().toUtf8();

    if (command == "protocolVersion") {
        return ProtocolVersion; // Version number.
    }

    int version = ob.value(QLatin1Literal("version")).toString().toInt();
    if (version != ProtocolVersion) {
        return -WrongProtocol;
    }

    if (command == "insertBreakpoint") {
        Breakpoint bp;
        bp.bpNumber = ++qt_v4BreakpointCount;
        bp.lineNumber = ob.value(QLatin1String("lineNumber")).toString().toInt();
        bp.engineName = ob.value(QLatin1String("engineName")).toString();
        bp.fullName = ob.value(QLatin1String("fullName")).toString();
        bp.condition = ob.value(QLatin1String("condition")).toString();
        qt_v4Breakpoints.append(bp);
        return bp.bpNumber;
    }

    if (command == "removeBreakpoint") {
        int lineNumber = ob.value(QLatin1String("lineNumber")).toString().toInt();
        QString fullName = ob.value(QLatin1String("fullName")).toString();
        if (qt_v4Breakpoints.last().matches(fullName, lineNumber)) {
            qt_v4Breakpoints.removeLast();
            return Success;
        }
        for (int i = 0; i + 1 < qt_v4Breakpoints.size(); ++i) {
            if (qt_v4Breakpoints.at(i).matches(fullName, lineNumber)) {
                qt_v4Breakpoints[i] = qt_v4Breakpoints.takeLast();
                return Success; // Ok.
            }
        }
        return -NoSuchBreakpoint; // Failure
    }

    if (command == "prepareStep") {
        qt_v4IsStepping = true;
        return Success; // Ok.
    }


    return -NoSuchCommand; // Failure.
}

static void qt_v4CheckForBreak(QV4::ExecutionContext *context, QV4::Value **scopes, int scopeDepth)
{
    Q_UNUSED(scopes);
    Q_UNUSED(scopeDepth);
    const int lineNumber = context->d()->lineNumber;
    QV4::Function *function = qt_v4ExtractFunction(context);
    QString engineName = function->sourceFile();

    if (engineName.isEmpty())
        return;

    if (qt_v4IsStepping) {
        if (qt_v4LastStop.lineNumber != lineNumber
                || qt_v4LastStop.engineName != engineName) {
            qt_v4IsStepping = false;
            Breakpoint bp;
            bp.bpNumber = 0;
            bp.lineNumber = lineNumber;
            bp.engineName = engineName;
            qt_v4TriggerBreakpoint(bp, function);
            return;
        }
    }

    for (int i = qt_v4Breakpoints.size(); --i >= 0; ) {
        const Breakpoint &bp = qt_v4Breakpoints.at(i);
        if (bp.lineNumber != lineNumber)
            continue;
        if (bp.engineName != engineName)
            continue;

        qt_v4TriggerBreakpoint(bp, function);
    }
}

#endif // QT_NO_QML_DEBUGGER
// End of debugger interface

using namespace QV4;
using namespace QV4::Moth;

#define MOTH_BEGIN_INSTR_COMMON(I) { \
    const InstrMeta<(int)Instr::I>::DataType &instr = InstrMeta<(int)Instr::I>::data(*genericInstr); \
    code += InstrMeta<(int)Instr::I>::Size; \
    Q_UNUSED(instr); \
    TRACE_INSTR(I)

#ifdef MOTH_THREADED_INTERPRETER

#  define MOTH_BEGIN_INSTR(I) op_##I: \
    MOTH_BEGIN_INSTR_COMMON(I)

#  define MOTH_END_INSTR(I) } \
    genericInstr = reinterpret_cast<const Instr *>(code); \
    goto *genericInstr->common.code; \

#else

#  define MOTH_BEGIN_INSTR(I) \
    case Instr::I: \
    MOTH_BEGIN_INSTR_COMMON(I)

#  define MOTH_END_INSTR(I) } \
    continue;

#endif

#ifdef DO_TRACE_INSTR
Param traceParam(const Param &param)
{
    if (param.isConstant()) {
        qDebug("    constant\n");
    } else if (param.isArgument()) {
        qDebug("    argument %d@%d\n", param.index, param.scope);
    } else if (param.isLocal()) {
        qDebug("    local %d\n", param.index);
    } else if (param.isTemp()) {
        qDebug("    temp %d\n", param.index);
    } else if (param.isScopedLocal()) {
        qDebug("    temp %d@%d\n", param.index, param.scope);
    } else {
        Q_ASSERT(!"INVALID");
    }
    return param;
}
# define VALUE(param) (*VALUEPTR(param))
# define VALUEPTR(param) (scopes[traceParam(param).scope] + param.index)
#else
# define VALUE(param) (*VALUEPTR(param))
# define VALUEPTR(param) (scopes[param.scope] + param.index)
#endif

#define STOREVALUE(param, value) { \
    QV4::ReturnedValue tmp = (value); \
    if (engine->hasException) \
        goto catchException; \
    VALUE(param) = tmp; \
    }
#define CHECK_EXCEPTION \
    if (engine->hasException) \
        goto catchException

QV4::ReturnedValue VME::run(ExecutionEngine *engine, const uchar *code
#ifdef MOTH_THREADED_INTERPRETER
        , void ***storeJumpTable
#endif
        )
{
#ifdef DO_TRACE_INSTR
    qDebug("Starting VME with context=%p and code=%p", context, code);
#endif // DO_TRACE_INSTR

    qt_v4ResolvePendingBreakpointsHook();

#ifdef MOTH_THREADED_INTERPRETER
    if (storeJumpTable) {
#define MOTH_INSTR_ADDR(I, FMT) &&op_##I,
        static void *jumpTable[] = {
            FOR_EACH_MOTH_INSTR(MOTH_INSTR_ADDR)
        };
#undef MOTH_INSTR_ADDR
        *storeJumpTable = jumpTable;
        return QV4::Primitive::undefinedValue().asReturnedValue();
    }
#endif

    QV4::Value *stack = 0;
    unsigned stackSize = 0;

    const uchar *exceptionHandler = 0;

    QV4::Scope scope(engine);
    QV4::ExecutionContext *context = engine->currentContext;
    engine->current->lineNumber = -1;

#ifdef DO_TRACE_INSTR
    qDebug("Starting VME with context=%p and code=%p", context, code);
#endif // DO_TRACE_INSTR

    // setup lookup scopes
    int scopeDepth = 0;
    {
        QV4::Heap::ExecutionContext *scope = context->d();
        while (scope) {
            ++scopeDepth;
            scope = scope->outer;
        }
    }

    QV4::Value **scopes = static_cast<QV4::Value **>(alloca(sizeof(QV4::Value *)*(2 + 2*scopeDepth)));
    {
        scopes[0] = const_cast<QV4::Value *>(context->d()->compilationUnit->constants);
        // stack gets setup in push instruction
        scopes[1] = 0;
        QV4::Heap::ExecutionContext *scope = context->d();
        int i = 0;
        while (scope) {
            if (scope->type >= QV4::Heap::ExecutionContext::Type_SimpleCallContext) {
                QV4::Heap::CallContext *cc = static_cast<QV4::Heap::CallContext *>(scope);
                scopes[2*i + 2] = cc->callData->args;
                scopes[2*i + 3] = cc->locals;
            } else {
                scopes[2*i + 2] = 0;
                scopes[2*i + 3] = 0;
            }
            ++i;
            scope = scope->outer;
        }
    }


    for (;;) {
        const Instr *genericInstr = reinterpret_cast<const Instr *>(code);
#ifdef MOTH_THREADED_INTERPRETER
        goto *genericInstr->common.code;
#else
        switch (genericInstr->common.instructionType) {
#endif

    MOTH_BEGIN_INSTR(Move)
        VALUE(instr.result) = VALUE(instr.source);
    MOTH_END_INSTR(Move)

    MOTH_BEGIN_INSTR(MoveConst)
        VALUE(instr.result) = instr.source;
    MOTH_END_INSTR(MoveConst)

    MOTH_BEGIN_INSTR(SwapTemps)
        qSwap(VALUE(instr.left),  VALUE(instr.right));
    MOTH_END_INSTR(MoveTemp)

    MOTH_BEGIN_INSTR(LoadRuntimeString)
//        TRACE(value, "%s", instr.value.toString(context)->toQString().toUtf8().constData());
        VALUE(instr.result) = context->d()->compilationUnit->runtimeStrings[instr.stringId];
    MOTH_END_INSTR(LoadRuntimeString)

    MOTH_BEGIN_INSTR(LoadRegExp)
//        TRACE(value, "%s", instr.value.toString(context)->toQString().toUtf8().constData());
        VALUE(instr.result) = context->d()->compilationUnit->runtimeRegularExpressions[instr.regExpId];
    MOTH_END_INSTR(LoadRegExp)

    MOTH_BEGIN_INSTR(LoadClosure)
        STOREVALUE(instr.result, engine->runtime.closure(engine, instr.value));
    MOTH_END_INSTR(LoadClosure)

    MOTH_BEGIN_INSTR(LoadName)
        TRACE(inline, "property name = %s", runtimeStrings[instr.name]->toQString().toUtf8().constData());
        STOREVALUE(instr.result, engine->runtime.getActivationProperty(engine, instr.name));
    MOTH_END_INSTR(LoadName)

    MOTH_BEGIN_INSTR(GetGlobalLookup)
        QV4::Lookup *l = context->d()->lookups + instr.index;
        STOREVALUE(instr.result, l->globalGetter(l, engine));
    MOTH_END_INSTR(GetGlobalLookup)

    MOTH_BEGIN_INSTR(StoreName)
        TRACE(inline, "property name = %s", runtimeStrings[instr.name]->toQString().toUtf8().constData());
        engine->runtime.setActivationProperty(engine, instr.name, VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreName)

    MOTH_BEGIN_INSTR(LoadElement)
        STOREVALUE(instr.result, engine->runtime.getElement(engine, VALUE(instr.base), VALUE(instr.index)));
    MOTH_END_INSTR(LoadElement)

    MOTH_BEGIN_INSTR(LoadElementLookup)
        QV4::Lookup *l = context->d()->lookups + instr.lookup;
        STOREVALUE(instr.result, l->indexedGetter(l, VALUE(instr.base), VALUE(instr.index)));
    MOTH_END_INSTR(LoadElementLookup)

    MOTH_BEGIN_INSTR(StoreElement)
        engine->runtime.setElement(engine, VALUE(instr.base), VALUE(instr.index), VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreElement)

    MOTH_BEGIN_INSTR(StoreElementLookup)
        QV4::Lookup *l = context->d()->lookups + instr.lookup;
        l->indexedSetter(l, VALUE(instr.base), VALUE(instr.index), VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreElementLookup)

    MOTH_BEGIN_INSTR(LoadProperty)
        STOREVALUE(instr.result, engine->runtime.getProperty(engine, VALUE(instr.base), instr.name));
    MOTH_END_INSTR(LoadProperty)

    MOTH_BEGIN_INSTR(GetLookup)
        QV4::Lookup *l = context->d()->lookups + instr.index;
        STOREVALUE(instr.result, l->getter(l, engine, VALUE(instr.base)));
    MOTH_END_INSTR(GetLookup)

    MOTH_BEGIN_INSTR(StoreProperty)
        engine->runtime.setProperty(engine, VALUE(instr.base), instr.name, VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreProperty)

    MOTH_BEGIN_INSTR(SetLookup)
        QV4::Lookup *l = context->d()->lookups + instr.index;
        l->setter(l, engine, VALUE(instr.base), VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(SetLookup)

    MOTH_BEGIN_INSTR(StoreQObjectProperty)
        engine->runtime.setQmlQObjectProperty(engine, VALUE(instr.base), instr.propertyIndex, VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreQObjectProperty)

    MOTH_BEGIN_INSTR(LoadQObjectProperty)
        STOREVALUE(instr.result, engine->runtime.getQmlQObjectProperty(engine, VALUE(instr.base), instr.propertyIndex, instr.captureRequired));
    MOTH_END_INSTR(LoadQObjectProperty)

    MOTH_BEGIN_INSTR(StoreScopeObjectProperty)
        engine->runtime.setQmlScopeObjectProperty(engine, VALUE(instr.base), instr.propertyIndex, VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreScopeObjectProperty)

    MOTH_BEGIN_INSTR(LoadScopeObjectProperty)
        STOREVALUE(instr.result, engine->runtime.getQmlScopeObjectProperty(engine, VALUE(instr.base), instr.propertyIndex, instr.captureRequired));
    MOTH_END_INSTR(LoadScopeObjectProperty)

    MOTH_BEGIN_INSTR(StoreContextObjectProperty)
        engine->runtime.setQmlContextObjectProperty(engine, VALUE(instr.base), instr.propertyIndex, VALUE(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreContextObjectProperty)

    MOTH_BEGIN_INSTR(LoadContextObjectProperty)
        STOREVALUE(instr.result, engine->runtime.getQmlContextObjectProperty(engine, VALUE(instr.base), instr.propertyIndex, instr.captureRequired));
    MOTH_END_INSTR(LoadContextObjectProperty)

    MOTH_BEGIN_INSTR(LoadIdObject)
        STOREVALUE(instr.result, engine->runtime.getQmlIdObject(engine, VALUE(instr.base), instr.index));
    MOTH_END_INSTR(LoadIdObject)

    MOTH_BEGIN_INSTR(LoadAttachedQObjectProperty)
        STOREVALUE(instr.result, engine->runtime.getQmlAttachedProperty(engine, instr.attachedPropertiesId, instr.propertyIndex));
    MOTH_END_INSTR(LoadAttachedQObjectProperty)

    MOTH_BEGIN_INSTR(LoadSingletonQObjectProperty)
        STOREVALUE(instr.result, engine->runtime.getQmlSingletonQObjectProperty(engine, VALUE(instr.base), instr.propertyIndex, instr.captureRequired));
    MOTH_END_INSTR(LoadSingletonQObjectProperty)

    MOTH_BEGIN_INSTR(Push)
        TRACE(inline, "stack size: %u", instr.value);
        stackSize = instr.value;
        stack = scope.alloc(stackSize);
        scopes[1] = stack;
    MOTH_END_INSTR(Push)

    MOTH_BEGIN_INSTR(CallValue)
#if 0 //def DO_TRACE_INSTR
        if (Debugging::Debugger *debugger = context->engine()->debugger) {
            if (QV4::FunctionObject *o = (VALUE(instr.dest)).asFunctionObject()) {
                if (Debugging::FunctionDebugInfo *info = debugger->debugInfo(o)) {
                    QString n = debugger->name(o);
                    std::cerr << "*** Call to \"" << (n.isNull() ? "<no name>" : qPrintable(n)) << "\" defined @" << info->startLine << ":" << info->startColumn << std::endl;
                }
            }
        }
#endif // DO_TRACE_INSTR
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, engine->runtime.callValue(engine, VALUE(instr.dest), callData));
    MOTH_END_INSTR(CallValue)

    MOTH_BEGIN_INSTR(CallProperty)
        TRACE(property name, "%s, args=%u, argc=%u, this=%s", qPrintable(runtimeStrings[instr.name]->toQString()), instr.callData, instr.argc, (VALUE(instr.base)).toString(context)->toQString().toUtf8().constData());
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, engine->runtime.callProperty(engine, instr.name, callData));
    MOTH_END_INSTR(CallProperty)

    MOTH_BEGIN_INSTR(CallPropertyLookup)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, engine->runtime.callPropertyLookup(engine, instr.lookupIndex, callData));
    MOTH_END_INSTR(CallPropertyLookup)

    MOTH_BEGIN_INSTR(CallScopeObjectProperty)
        TRACE(property name, "%s, args=%u, argc=%u, this=%s", qPrintable(runtimeStrings[instr.name]->toQString()), instr.callData, instr.argc, (VALUE(instr.base)).toString(context)->toQString().toUtf8().constData());
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, engine->runtime.callQmlScopeObjectProperty(engine, instr.index, callData));
    MOTH_END_INSTR(CallScopeObjectProperty)

    MOTH_BEGIN_INSTR(CallContextObjectProperty)
        TRACE(property name, "%s, args=%u, argc=%u, this=%s", qPrintable(runtimeStrings[instr.name]->toQString()), instr.callData, instr.argc, (VALUE(instr.base)).toString(context)->toQString().toUtf8().constData());
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, engine->runtime.callQmlContextObjectProperty(engine, instr.index, callData));
    MOTH_END_INSTR(CallContextObjectProperty)

    MOTH_BEGIN_INSTR(CallElement)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, engine->runtime.callElement(engine, VALUE(instr.index), callData));
    MOTH_END_INSTR(CallElement)

    MOTH_BEGIN_INSTR(CallActivationProperty)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, engine->runtime.callActivationProperty(engine, instr.name, callData));
    MOTH_END_INSTR(CallActivationProperty)

    MOTH_BEGIN_INSTR(CallGlobalLookup)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, Runtime::method_callGlobalLookup(engine, instr.index, callData));
    MOTH_END_INSTR(CallGlobalLookup)

    MOTH_BEGIN_INSTR(SetExceptionHandler)
        exceptionHandler = instr.offset ? ((const uchar *)&instr.offset) + instr.offset : 0;
    MOTH_END_INSTR(SetExceptionHandler)

    MOTH_BEGIN_INSTR(CallBuiltinThrow)
        engine->runtime.throwException(engine, VALUE(instr.arg));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(CallBuiltinThrow)

    MOTH_BEGIN_INSTR(CallBuiltinUnwindException)
        STOREVALUE(instr.result, engine->runtime.unwindException(engine));
    MOTH_END_INSTR(CallBuiltinUnwindException)

    MOTH_BEGIN_INSTR(CallBuiltinPushCatchScope)
        engine->runtime.pushCatchScope(static_cast<QV4::NoThrowEngine*>(engine), instr.name);
        context = engine->currentContext;
    MOTH_END_INSTR(CallBuiltinPushCatchScope)

    MOTH_BEGIN_INSTR(CallBuiltinPushScope)
        engine->runtime.pushWithScope(VALUE(instr.arg), static_cast<QV4::NoThrowEngine*>(engine));
        context = engine->currentContext;
        CHECK_EXCEPTION;
    MOTH_END_INSTR(CallBuiltinPushScope)

    MOTH_BEGIN_INSTR(CallBuiltinPopScope)
        engine->runtime.popScope(static_cast<QV4::NoThrowEngine*>(engine));
        context = engine->currentContext;
    MOTH_END_INSTR(CallBuiltinPopScope)

    MOTH_BEGIN_INSTR(CallBuiltinForeachIteratorObject)
        STOREVALUE(instr.result, engine->runtime.foreachIterator(engine, VALUE(instr.arg)));
    MOTH_END_INSTR(CallBuiltinForeachIteratorObject)

    MOTH_BEGIN_INSTR(CallBuiltinForeachNextPropertyName)
        STOREVALUE(instr.result, engine->runtime.foreachNextPropertyName(VALUE(instr.arg)));
    MOTH_END_INSTR(CallBuiltinForeachNextPropertyName)

    MOTH_BEGIN_INSTR(CallBuiltinDeleteMember)
        STOREVALUE(instr.result, engine->runtime.deleteMember(engine, VALUE(instr.base), instr.member));
    MOTH_END_INSTR(CallBuiltinDeleteMember)

    MOTH_BEGIN_INSTR(CallBuiltinDeleteSubscript)
        STOREVALUE(instr.result, engine->runtime.deleteElement(engine, VALUE(instr.base), VALUE(instr.index)));
    MOTH_END_INSTR(CallBuiltinDeleteSubscript)

    MOTH_BEGIN_INSTR(CallBuiltinDeleteName)
        STOREVALUE(instr.result, engine->runtime.deleteName(engine, instr.name));
    MOTH_END_INSTR(CallBuiltinDeleteName)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofScopeObjectProperty)
        STOREVALUE(instr.result, engine->runtime.typeofScopeObjectProperty(engine, VALUE(instr.base), instr.index));
    MOTH_END_INSTR(CallBuiltinTypeofMember)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofContextObjectProperty)
        STOREVALUE(instr.result, engine->runtime.typeofContextObjectProperty(engine, VALUE(instr.base), instr.index));
    MOTH_END_INSTR(CallBuiltinTypeofMember)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofMember)
        STOREVALUE(instr.result, engine->runtime.typeofMember(engine, VALUE(instr.base), instr.member));
    MOTH_END_INSTR(CallBuiltinTypeofMember)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofSubscript)
        STOREVALUE(instr.result, engine->runtime.typeofElement(engine, VALUE(instr.base), VALUE(instr.index)));
    MOTH_END_INSTR(CallBuiltinTypeofSubscript)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofName)
        STOREVALUE(instr.result, engine->runtime.typeofName(engine, instr.name));
    MOTH_END_INSTR(CallBuiltinTypeofName)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofValue)
        STOREVALUE(instr.result, engine->runtime.typeofValue(engine, VALUE(instr.value)));
    MOTH_END_INSTR(CallBuiltinTypeofValue)

    MOTH_BEGIN_INSTR(CallBuiltinDeclareVar)
        engine->runtime.declareVar(engine, instr.isDeletable, instr.varName);
    MOTH_END_INSTR(CallBuiltinDeclareVar)

    MOTH_BEGIN_INSTR(CallBuiltinDefineArray)
        Q_ASSERT(instr.args + instr.argc <= stackSize);
        QV4::Value *args = stack + instr.args;
        STOREVALUE(instr.result, engine->runtime.arrayLiteral(engine, args, instr.argc));
    MOTH_END_INSTR(CallBuiltinDefineArray)

    MOTH_BEGIN_INSTR(CallBuiltinDefineObjectLiteral)
        QV4::Value *args = stack + instr.args;
    STOREVALUE(instr.result, engine->runtime.objectLiteral(engine, args, instr.internalClassId, instr.arrayValueCount, instr.arrayGetterSetterCountAndFlags));
    MOTH_END_INSTR(CallBuiltinDefineObjectLiteral)

    MOTH_BEGIN_INSTR(CallBuiltinSetupArgumentsObject)
        STOREVALUE(instr.result, engine->runtime.setupArgumentsObject(engine));
    MOTH_END_INSTR(CallBuiltinSetupArgumentsObject)

    MOTH_BEGIN_INSTR(CallBuiltinConvertThisToObject)
        engine->runtime.convertThisToObject(engine);
        CHECK_EXCEPTION;
    MOTH_END_INSTR(CallBuiltinConvertThisToObject)

    MOTH_BEGIN_INSTR(CreateValue)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, engine->runtime.constructValue(engine, VALUE(instr.func), callData));
    MOTH_END_INSTR(CreateValue)

    MOTH_BEGIN_INSTR(CreateProperty)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, engine->runtime.constructProperty(engine, instr.name, callData));
    MOTH_END_INSTR(CreateProperty)

    MOTH_BEGIN_INSTR(ConstructPropertyLookup)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, engine->runtime.constructPropertyLookup(engine, instr.index, callData));
    MOTH_END_INSTR(ConstructPropertyLookup)

    MOTH_BEGIN_INSTR(CreateActivationProperty)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, engine->runtime.constructActivationProperty(engine, instr.name, callData));
    MOTH_END_INSTR(CreateActivationProperty)

    MOTH_BEGIN_INSTR(ConstructGlobalLookup)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type_Internal;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, engine->runtime.constructGlobalLookup(engine, instr.index, callData));
    MOTH_END_INSTR(ConstructGlobalLookup)

    MOTH_BEGIN_INSTR(Jump)
        code = ((const uchar *)&instr.offset) + instr.offset;
    MOTH_END_INSTR(Jump)

    MOTH_BEGIN_INSTR(JumpEq)
        bool cond = VALUEPTR(instr.condition)->toBoolean();
        TRACE(condition, "%s", cond ? "TRUE" : "FALSE");
        if (cond)
            code = ((const uchar *)&instr.offset) + instr.offset;
    MOTH_END_INSTR(JumpEq)

    MOTH_BEGIN_INSTR(JumpNe)
        bool cond = VALUEPTR(instr.condition)->toBoolean();
        TRACE(condition, "%s", cond ? "TRUE" : "FALSE");
        if (!cond)
            code = ((const uchar *)&instr.offset) + instr.offset;
    MOTH_END_INSTR(JumpNe)

    MOTH_BEGIN_INSTR(UNot)
        STOREVALUE(instr.result, engine->runtime.uNot(VALUE(instr.source)));
    MOTH_END_INSTR(UNot)

    MOTH_BEGIN_INSTR(UNotBool)
        bool b = VALUE(instr.source).booleanValue();
        VALUE(instr.result) = QV4::Encode(!b);
    MOTH_END_INSTR(UNotBool)

    MOTH_BEGIN_INSTR(UPlus)
        STOREVALUE(instr.result, engine->runtime.uPlus(VALUE(instr.source)));
    MOTH_END_INSTR(UPlus)

    MOTH_BEGIN_INSTR(UMinus)
        STOREVALUE(instr.result, engine->runtime.uMinus(VALUE(instr.source)));
    MOTH_END_INSTR(UMinus)

    MOTH_BEGIN_INSTR(UCompl)
        STOREVALUE(instr.result, engine->runtime.complement(VALUE(instr.source)));
    MOTH_END_INSTR(UCompl)

    MOTH_BEGIN_INSTR(UComplInt)
        VALUE(instr.result) = QV4::Encode((int)~VALUE(instr.source).integerValue());
    MOTH_END_INSTR(UComplInt)

    MOTH_BEGIN_INSTR(Increment)
        STOREVALUE(instr.result, engine->runtime.increment(VALUE(instr.source)));
    MOTH_END_INSTR(Increment)

    MOTH_BEGIN_INSTR(Decrement)
        STOREVALUE(instr.result, engine->runtime.decrement(VALUE(instr.source)));
    MOTH_END_INSTR(Decrement)

    MOTH_BEGIN_INSTR(Binop)
        QV4::Runtime::BinaryOperation op = *reinterpret_cast<QV4::Runtime::BinaryOperation *>(reinterpret_cast<char *>(&engine->runtime) + instr.alu);
        STOREVALUE(instr.result, op(VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(Binop)

    MOTH_BEGIN_INSTR(Add)
        STOREVALUE(instr.result, engine->runtime.add(engine, VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(Add)

    MOTH_BEGIN_INSTR(BitAnd)
        STOREVALUE(instr.result, engine->runtime.bitAnd(VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(BitAnd)

    MOTH_BEGIN_INSTR(BitOr)
        STOREVALUE(instr.result, engine->runtime.bitOr(VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(BitOr)

    MOTH_BEGIN_INSTR(BitXor)
        STOREVALUE(instr.result, engine->runtime.bitXor(VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(BitXor)

    MOTH_BEGIN_INSTR(Shr)
        STOREVALUE(instr.result, QV4::Encode((int)(VALUEPTR(instr.lhs)->toInt32() >> (VALUEPTR(instr.rhs)->toInt32() & 0x1f))));
    MOTH_END_INSTR(Shr)

    MOTH_BEGIN_INSTR(Shl)
        STOREVALUE(instr.result, QV4::Encode((int)(VALUEPTR(instr.lhs)->toInt32() << (VALUEPTR(instr.rhs)->toInt32() & 0x1f))));
    MOTH_END_INSTR(Shl)

    MOTH_BEGIN_INSTR(BitAndConst)
        int lhs = VALUEPTR(instr.lhs)->toInt32();
        STOREVALUE(instr.result, QV4::Encode((int)(lhs & instr.rhs)));
    MOTH_END_INSTR(BitAnd)

    MOTH_BEGIN_INSTR(BitOrConst)
        int lhs = VALUEPTR(instr.lhs)->toInt32();
        STOREVALUE(instr.result, QV4::Encode((int)(lhs | instr.rhs)));
    MOTH_END_INSTR(BitOr)

    MOTH_BEGIN_INSTR(BitXorConst)
        int lhs = VALUEPTR(instr.lhs)->toInt32();
        STOREVALUE(instr.result, QV4::Encode((int)(lhs ^ instr.rhs)));
    MOTH_END_INSTR(BitXor)

    MOTH_BEGIN_INSTR(ShrConst)
        STOREVALUE(instr.result, QV4::Encode((int)(VALUEPTR(instr.lhs)->toInt32() >> instr.rhs)));
    MOTH_END_INSTR(ShrConst)

    MOTH_BEGIN_INSTR(ShlConst)
        STOREVALUE(instr.result, QV4::Encode((int)(VALUEPTR(instr.lhs)->toInt32() << instr.rhs)));
    MOTH_END_INSTR(ShlConst)

    MOTH_BEGIN_INSTR(Mul)
        STOREVALUE(instr.result, engine->runtime.mul(VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(Mul)

    MOTH_BEGIN_INSTR(Sub)
        STOREVALUE(instr.result, engine->runtime.sub(VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(Sub)

    MOTH_BEGIN_INSTR(BinopContext)
        QV4::Runtime::BinaryOperationContext op = *reinterpret_cast<QV4::Runtime::BinaryOperationContext *>(reinterpret_cast<char *>(&engine->runtime) + instr.alu);
        STOREVALUE(instr.result, op(engine, VALUE(instr.lhs), VALUE(instr.rhs)));
    MOTH_END_INSTR(BinopContext)

    MOTH_BEGIN_INSTR(Ret)
//        TRACE(Ret, "returning value %s", result.toString(context)->toQString().toUtf8().constData());
        return VALUE(instr.result).asReturnedValue();
    MOTH_END_INSTR(Ret)

#ifndef QT_NO_QML_DEBUGGER
    MOTH_BEGIN_INSTR(Debug)
        engine->current->lineNumber = instr.lineNumber;
        QV4::Debugging::Debugger *debugger = context->engine()->debugger();
        if (debugger && debugger->pauseAtNextOpportunity())
            debugger->maybeBreakAtInstruction();
        if (qt_v4IsDebugging)
            qt_v4CheckForBreak(context, scopes, scopeDepth);
    MOTH_END_INSTR(Debug)

    MOTH_BEGIN_INSTR(Line)
        engine->current->lineNumber = instr.lineNumber;
        if (qt_v4IsDebugging)
            qt_v4CheckForBreak(context, scopes, scopeDepth);
    MOTH_END_INSTR(Line)
#endif // QT_NO_QML_DEBUGGER

    MOTH_BEGIN_INSTR(LoadThis)
        VALUE(instr.result) = context->thisObject();
    MOTH_END_INSTR(LoadThis)

    MOTH_BEGIN_INSTR(LoadQmlContext)
        VALUE(instr.result) = engine->runtime.getQmlContext(static_cast<QV4::NoThrowEngine*>(engine));
    MOTH_END_INSTR(LoadQmlContext)

    MOTH_BEGIN_INSTR(LoadQmlImportedScripts)
        VALUE(instr.result) = engine->runtime.getQmlImportedScripts(static_cast<QV4::NoThrowEngine*>(engine));
    MOTH_END_INSTR(LoadQmlImportedScripts)

    MOTH_BEGIN_INSTR(LoadQmlSingleton)
        VALUE(instr.result) = engine->runtime.getQmlSingleton(static_cast<QV4::NoThrowEngine*>(engine), instr.name);
    MOTH_END_INSTR(LoadQmlSingleton)

#ifdef MOTH_THREADED_INTERPRETER
    // nothing to do
#else
        default:
            qFatal("QQmlJS::Moth::VME: Internal error - unknown instruction %d", genericInstr->common.instructionType);
            break;
        }
#endif

        Q_ASSERT(false);
    catchException:
        Q_ASSERT(context->engine()->hasException);
        if (!exceptionHandler)
            return QV4::Encode::undefined();
        code = exceptionHandler;
    }


}

#ifdef MOTH_THREADED_INTERPRETER
void **VME::instructionJumpTable()
{
    static void **jumpTable = 0;
    if (!jumpTable) {
        const uchar *code = 0;
        VME().run(0, code, &jumpTable);
    }
    return jumpTable;
}
#endif

QV4::ReturnedValue VME::exec(ExecutionEngine *engine, const uchar *code)
{
    VME vme;
    QV4::Debugging::Debugger *debugger = engine->debugger();
    if (debugger)
        debugger->enteringFunction();
    QV4::ReturnedValue retVal = vme.run(engine, code);
    if (debugger)
        debugger->leavingFunction(retVal);
    return retVal;
}
