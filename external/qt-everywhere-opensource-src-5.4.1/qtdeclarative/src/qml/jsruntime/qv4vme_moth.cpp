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

#include "qv4vme_moth_p.h"
#include "qv4instr_moth_p.h"
#include <private/qv4value_inl_p.h>
#include <private/qv4debugging_p.h>
#include <private/qv4math_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4lookup_p.h>
#include <iostream>

#include "qv4alloca_p.h"

#ifdef DO_TRACE_INSTR
#  define TRACE_INSTR(I) fprintf(stderr, "executing a %s\n", #I);
#  define TRACE(n, str, ...) { char buf[4096]; snprintf(buf, 4096, str, __VA_ARGS__); fprintf(stderr, "    %s : %s\n", #n, buf); }
#else
#  define TRACE_INSTR(I)
#  define TRACE(n, str, ...)
#endif // DO_TRACE_INSTR

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

#ifdef WITH_STATS
namespace {
struct VMStats {
    quint64 paramIsValue;
    quint64 paramIsArg;
    quint64 paramIsLocal;
    quint64 paramIsTemp;
    quint64 paramIsScopedLocal;

    VMStats()
        : paramIsValue(0)
        , paramIsArg(0)
        , paramIsLocal(0)
        , paramIsTemp(0)
        , paramIsScopedLocal(0)
    {}

    ~VMStats()
    { show(); }

    void show() {
        fprintf(stderr, "VM stats:\n");
        fprintf(stderr, "         value: %lu\n", paramIsValue);
        fprintf(stderr, "           arg: %lu\n", paramIsArg);
        fprintf(stderr, "         local: %lu\n", paramIsLocal);
        fprintf(stderr, "          temp: %lu\n", paramIsTemp);
        fprintf(stderr, "  scoped local: %lu\n", paramIsScopedLocal);
    }
};
static VMStats vmStats;
#define VMSTATS(what) ++vmStats.what
}
#else // !WITH_STATS
#define VMSTATS(what) {}
#endif // WITH_STATS

#ifdef DO_TRACE_INSTR
Param traceParam(const Param &param)
{
    if (param.isConstant()) {
        fprintf(stderr, "    constant\n");
    } else if (param.isArgument()) {
        fprintf(stderr, "    argument %d@%d\n", param.index, param.scope);
    } else if (param.isLocal()) {
        fprintf(stderr, "    local %d\n", param.index);
    } else if (param.isTemp()) {
        fprintf(stderr, "    temp %d\n", param.index);
    } else if (param.isScopedLocal()) {
        fprintf(stderr, "    temp %d@%d\n", param.index, param.scope);
    } else {
        Q_ASSERT(!"INVALID");
    }
    return Param
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

QV4::ReturnedValue VME::run(QV4::ExecutionContext *context, const uchar *code
#ifdef MOTH_THREADED_INTERPRETER
        , void ***storeJumpTable
#endif
        )
{
#ifdef DO_TRACE_INSTR
    qDebug("Starting VME with context=%p and code=%p", context, code);
#endif // DO_TRACE_INSTR

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

    context->d()->lineNumber = -1;
    QV4::ExecutionEngine *engine = context->d()->engine;

#ifdef DO_TRACE_INSTR
    qDebug("Starting VME with context=%p and code=%p", context, code);
#endif // DO_TRACE_INSTR

    QV4::StringValue * const runtimeStrings = context->d()->compilationUnit->runtimeStrings;

    // setup lookup scopes
    int scopeDepth = 0;
    {
        QV4::ExecutionContext *scope = context;
        while (scope) {
            ++scopeDepth;
            scope = scope->d()->outer;
        }
    }

    QV4::Value **scopes = static_cast<QV4::Value **>(alloca(sizeof(QV4::Value *)*(2 + 2*scopeDepth)));
    {
        scopes[0] = const_cast<QV4::Value *>(context->d()->compilationUnit->data->constants());
        // stack gets setup in push instruction
        scopes[1] = 0;
        QV4::ExecutionContext *scope = context;
        int i = 0;
        while (scope) {
            if (scope->d()->type >= QV4::ExecutionContext::Type_SimpleCallContext) {
                QV4::CallContext *cc = static_cast<QV4::CallContext *>(scope);
                scopes[2*i + 2] = cc->d()->callData->args;
                scopes[2*i + 3] = cc->d()->locals;
            } else {
                scopes[2*i + 2] = 0;
                scopes[2*i + 3] = 0;
            }
            ++i;
            scope = scope->d()->outer;
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
        VALUE(instr.result) = runtimeStrings[instr.stringId].asReturnedValue();
    MOTH_END_INSTR(LoadRuntimeString)

    MOTH_BEGIN_INSTR(LoadRegExp)
//        TRACE(value, "%s", instr.value.toString(context)->toQString().toUtf8().constData());
        VALUE(instr.result) = context->d()->compilationUnit->runtimeRegularExpressions[instr.regExpId];
    MOTH_END_INSTR(LoadRegExp)

    MOTH_BEGIN_INSTR(LoadClosure)
        STOREVALUE(instr.result, Runtime::closure(context, instr.value));
    MOTH_END_INSTR(LoadClosure)

    MOTH_BEGIN_INSTR(LoadName)
        TRACE(inline, "property name = %s", runtimeStrings[instr.name]->toQString().toUtf8().constData());
        STOREVALUE(instr.result, Runtime::getActivationProperty(context, runtimeStrings[instr.name]));
    MOTH_END_INSTR(LoadName)

    MOTH_BEGIN_INSTR(GetGlobalLookup)
        TRACE(inline, "property name = %s", runtimeStrings[instr.name]->toQString().toUtf8().constData());
        QV4::Lookup *l = context->d()->lookups + instr.index;
        STOREVALUE(instr.result, l->globalGetter(l, context));
    MOTH_END_INSTR(GetGlobalLookup)

    MOTH_BEGIN_INSTR(StoreName)
        TRACE(inline, "property name = %s", runtimeStrings[instr.name]->toQString().toUtf8().constData());
        Runtime::setActivationProperty(context, runtimeStrings[instr.name], VALUEPTR(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreName)

    MOTH_BEGIN_INSTR(LoadElement)
        STOREVALUE(instr.result, Runtime::getElement(context, VALUEPTR(instr.base), VALUEPTR(instr.index)));
    MOTH_END_INSTR(LoadElement)

    MOTH_BEGIN_INSTR(LoadElementLookup)
        QV4::Lookup *l = context->d()->lookups + instr.lookup;
        STOREVALUE(instr.result, l->indexedGetter(l, VALUEPTR(instr.base), VALUEPTR(instr.index)));
    MOTH_END_INSTR(LoadElementLookup)

    MOTH_BEGIN_INSTR(StoreElement)
        Runtime::setElement(context, VALUEPTR(instr.base), VALUEPTR(instr.index), VALUEPTR(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreElement)

    MOTH_BEGIN_INSTR(StoreElementLookup)
        QV4::Lookup *l = context->d()->lookups + instr.lookup;
        l->indexedSetter(l, VALUEPTR(instr.base), VALUEPTR(instr.index), VALUEPTR(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreElementLookup)

    MOTH_BEGIN_INSTR(LoadProperty)
        STOREVALUE(instr.result, Runtime::getProperty(context, VALUEPTR(instr.base), runtimeStrings[instr.name]));
    MOTH_END_INSTR(LoadProperty)

    MOTH_BEGIN_INSTR(GetLookup)
        QV4::Lookup *l = context->d()->lookups + instr.index;
        STOREVALUE(instr.result, l->getter(l, VALUEPTR(instr.base)));
    MOTH_END_INSTR(GetLookup)

    MOTH_BEGIN_INSTR(StoreProperty)
        Runtime::setProperty(context, VALUEPTR(instr.base), runtimeStrings[instr.name], VALUEPTR(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreProperty)

    MOTH_BEGIN_INSTR(SetLookup)
        QV4::Lookup *l = context->d()->lookups + instr.index;
        l->setter(l, VALUEPTR(instr.base), VALUEPTR(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(SetLookup)

    MOTH_BEGIN_INSTR(StoreQObjectProperty)
        Runtime::setQmlQObjectProperty(context, VALUEPTR(instr.base), instr.propertyIndex, VALUEPTR(instr.source));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(StoreQObjectProperty)

    MOTH_BEGIN_INSTR(LoadQObjectProperty)
        STOREVALUE(instr.result, Runtime::getQmlQObjectProperty(context, VALUEPTR(instr.base), instr.propertyIndex, instr.captureRequired));
    MOTH_END_INSTR(LoadQObjectProperty)

    MOTH_BEGIN_INSTR(LoadAttachedQObjectProperty)
        STOREVALUE(instr.result, Runtime::getQmlAttachedProperty(context, instr.attachedPropertiesId, instr.propertyIndex));
    MOTH_END_INSTR(LoadAttachedQObjectProperty)

    MOTH_BEGIN_INSTR(LoadSingletonQObjectProperty)
        STOREVALUE(instr.result, Runtime::getQmlSingletonQObjectProperty(context, VALUEPTR(instr.base), instr.propertyIndex, instr.captureRequired));
    MOTH_END_INSTR(LoadSingletonQObjectProperty)

    MOTH_BEGIN_INSTR(Push)
        TRACE(inline, "stack size: %u", instr.value);
        stackSize = instr.value;
        stack = context->engine()->stackPush(stackSize);
#ifndef QT_NO_DEBUG
        memset(stack, 0, stackSize * sizeof(QV4::Value));
#endif
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
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, Runtime::callValue(context, VALUEPTR(instr.dest), callData));
    MOTH_END_INSTR(CallValue)

    MOTH_BEGIN_INSTR(CallProperty)
        TRACE(property name, "%s, args=%u, argc=%u, this=%s", qPrintable(runtimeStrings[instr.name]->toQString()), instr.callData, instr.argc, (VALUE(instr.base)).toString(context)->toQString().toUtf8().constData());
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, Runtime::callProperty(context, runtimeStrings[instr.name], callData));
    MOTH_END_INSTR(CallProperty)

    MOTH_BEGIN_INSTR(CallPropertyLookup)
        TRACE(property name, "%s, args=%u, argc=%u, this=%s", qPrintable(runtimeStrings[instr.name]->toQString()), instr.callData, instr.argc, (VALUE(instr.base)).toString(context)->toQString().toUtf8().constData());
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, Runtime::callPropertyLookup(context, instr.lookupIndex, callData));
    MOTH_END_INSTR(CallPropertyLookup)

    MOTH_BEGIN_INSTR(CallElement)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, Runtime::callElement(context, VALUEPTR(instr.index), callData));
    MOTH_END_INSTR(CallElement)

    MOTH_BEGIN_INSTR(CallActivationProperty)
        TRACE(args, "starting at %d, length %d", instr.args, instr.argc);
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, Runtime::callActivationProperty(context, runtimeStrings[instr.name], callData));
    MOTH_END_INSTR(CallActivationProperty)

    MOTH_BEGIN_INSTR(CallGlobalLookup)
        TRACE(args, "starting at %d, length %d", instr.args, instr.argc);
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, Runtime::callGlobalLookup(context, instr.index, callData));
    MOTH_END_INSTR(CallGlobalLookup)

    MOTH_BEGIN_INSTR(SetExceptionHandler)
        exceptionHandler = instr.offset ? ((uchar *)&instr.offset) + instr.offset : 0;
    MOTH_END_INSTR(SetExceptionHandler)

    MOTH_BEGIN_INSTR(CallBuiltinThrow)
        Runtime::throwException(context, VALUEPTR(instr.arg));
        CHECK_EXCEPTION;
    MOTH_END_INSTR(CallBuiltinThrow)

    MOTH_BEGIN_INSTR(CallBuiltinUnwindException)
        STOREVALUE(instr.result, Runtime::unwindException(context));
    MOTH_END_INSTR(CallBuiltinUnwindException)

    MOTH_BEGIN_INSTR(CallBuiltinPushCatchScope)
        context = Runtime::pushCatchScope(context, runtimeStrings[instr.name]);
    MOTH_END_INSTR(CallBuiltinPushCatchScope)

    MOTH_BEGIN_INSTR(CallBuiltinPushScope)
        context = Runtime::pushWithScope(VALUEPTR(instr.arg), context);
    MOTH_END_INSTR(CallBuiltinPushScope)

    MOTH_BEGIN_INSTR(CallBuiltinPopScope)
        context = Runtime::popScope(context);
    MOTH_END_INSTR(CallBuiltinPopScope)

    MOTH_BEGIN_INSTR(CallBuiltinForeachIteratorObject)
        STOREVALUE(instr.result, Runtime::foreachIterator(context, VALUEPTR(instr.arg)));
    MOTH_END_INSTR(CallBuiltinForeachIteratorObject)

    MOTH_BEGIN_INSTR(CallBuiltinForeachNextPropertyName)
        STOREVALUE(instr.result, Runtime::foreachNextPropertyName(VALUEPTR(instr.arg)));
    MOTH_END_INSTR(CallBuiltinForeachNextPropertyName)

    MOTH_BEGIN_INSTR(CallBuiltinDeleteMember)
        STOREVALUE(instr.result, Runtime::deleteMember(context, VALUEPTR(instr.base), runtimeStrings[instr.member]));
    MOTH_END_INSTR(CallBuiltinDeleteMember)

    MOTH_BEGIN_INSTR(CallBuiltinDeleteSubscript)
        STOREVALUE(instr.result, Runtime::deleteElement(context, VALUEPTR(instr.base), VALUEPTR(instr.index)));
    MOTH_END_INSTR(CallBuiltinDeleteSubscript)

    MOTH_BEGIN_INSTR(CallBuiltinDeleteName)
        STOREVALUE(instr.result, Runtime::deleteName(context, runtimeStrings[instr.name]));
    MOTH_END_INSTR(CallBuiltinDeleteName)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofMember)
        STOREVALUE(instr.result, Runtime::typeofMember(context, VALUEPTR(instr.base), runtimeStrings[instr.member]));
    MOTH_END_INSTR(CallBuiltinTypeofMember)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofSubscript)
        STOREVALUE(instr.result, Runtime::typeofElement(context, VALUEPTR(instr.base), VALUEPTR(instr.index)));
    MOTH_END_INSTR(CallBuiltinTypeofSubscript)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofName)
        STOREVALUE(instr.result, Runtime::typeofName(context, runtimeStrings[instr.name]));
    MOTH_END_INSTR(CallBuiltinTypeofName)

    MOTH_BEGIN_INSTR(CallBuiltinTypeofValue)
        STOREVALUE(instr.result, Runtime::typeofValue(context, VALUEPTR(instr.value)));
    MOTH_END_INSTR(CallBuiltinTypeofValue)

    MOTH_BEGIN_INSTR(CallBuiltinDeclareVar)
        Runtime::declareVar(context, instr.isDeletable, runtimeStrings[instr.varName]);
    MOTH_END_INSTR(CallBuiltinDeclareVar)

    MOTH_BEGIN_INSTR(CallBuiltinDefineArray)
        Q_ASSERT(instr.args + instr.argc <= stackSize);
        QV4::Value *args = stack + instr.args;
        STOREVALUE(instr.result, Runtime::arrayLiteral(context, args, instr.argc));
    MOTH_END_INSTR(CallBuiltinDefineArray)

    MOTH_BEGIN_INSTR(CallBuiltinDefineObjectLiteral)
        QV4::Value *args = stack + instr.args;
    STOREVALUE(instr.result, Runtime::objectLiteral(context, args, instr.internalClassId, instr.arrayValueCount, instr.arrayGetterSetterCountAndFlags));
    MOTH_END_INSTR(CallBuiltinDefineObjectLiteral)

    MOTH_BEGIN_INSTR(CallBuiltinSetupArgumentsObject)
        STOREVALUE(instr.result, Runtime::setupArgumentsObject(context));
    MOTH_END_INSTR(CallBuiltinSetupArgumentsObject)

    MOTH_BEGIN_INSTR(CallBuiltinConvertThisToObject)
        Runtime::convertThisToObject(context);
        CHECK_EXCEPTION;
    MOTH_END_INSTR(CallBuiltinConvertThisToObject)

    MOTH_BEGIN_INSTR(CreateValue)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, Runtime::constructValue(context, VALUEPTR(instr.func), callData));
    MOTH_END_INSTR(CreateValue)

    MOTH_BEGIN_INSTR(CreateProperty)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, Runtime::constructProperty(context, runtimeStrings[instr.name], callData));
    MOTH_END_INSTR(CreateProperty)

    MOTH_BEGIN_INSTR(ConstructPropertyLookup)
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = VALUE(instr.base);
        STOREVALUE(instr.result, Runtime::constructPropertyLookup(context, instr.index, callData));
    MOTH_END_INSTR(ConstructPropertyLookup)

    MOTH_BEGIN_INSTR(CreateActivationProperty)
        TRACE(inline, "property name = %s, args = %d, argc = %d", runtimeStrings[instr.name]->toQString().toUtf8().constData(), instr.args, instr.argc);
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, Runtime::constructActivationProperty(context, runtimeStrings[instr.name], callData));
    MOTH_END_INSTR(CreateActivationProperty)

    MOTH_BEGIN_INSTR(ConstructGlobalLookup)
        TRACE(inline, "property name = %s, args = %d, argc = %d", runtimeStrings[instr.name]->toQString().toUtf8().constData(), instr.args, instr.argc);
        Q_ASSERT(instr.callData + instr.argc + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value) <= stackSize);
        QV4::CallData *callData = reinterpret_cast<QV4::CallData *>(stack + instr.callData);
        callData->tag = QV4::Value::Integer_Type;
        callData->argc = instr.argc;
        callData->thisObject = QV4::Primitive::undefinedValue();
        STOREVALUE(instr.result, Runtime::constructGlobalLookup(context, instr.index, callData));
    MOTH_END_INSTR(ConstructGlobalLookup)

    MOTH_BEGIN_INSTR(Jump)
        code = ((uchar *)&instr.offset) + instr.offset;
    MOTH_END_INSTR(Jump)

    MOTH_BEGIN_INSTR(JumpEq)
        bool cond = VALUEPTR(instr.condition)->toBoolean();
        TRACE(condition, "%s", cond ? "TRUE" : "FALSE");
        if (cond)
            code = ((uchar *)&instr.offset) + instr.offset;
    MOTH_END_INSTR(JumpEq)

    MOTH_BEGIN_INSTR(JumpNe)
        bool cond = VALUEPTR(instr.condition)->toBoolean();
        TRACE(condition, "%s", cond ? "TRUE" : "FALSE");
        if (!cond)
            code = ((uchar *)&instr.offset) + instr.offset;
    MOTH_END_INSTR(JumpNe)

    MOTH_BEGIN_INSTR(UNot)
        STOREVALUE(instr.result, Runtime::uNot(VALUEPTR(instr.source)));
    MOTH_END_INSTR(UNot)

    MOTH_BEGIN_INSTR(UNotBool)
        bool b = VALUE(instr.source).booleanValue();
        VALUE(instr.result) = QV4::Encode(!b);
    MOTH_END_INSTR(UNotBool)

    MOTH_BEGIN_INSTR(UPlus)
        STOREVALUE(instr.result, Runtime::uPlus(VALUEPTR(instr.source)));
    MOTH_END_INSTR(UPlus)

    MOTH_BEGIN_INSTR(UMinus)
        STOREVALUE(instr.result, Runtime::uMinus(VALUEPTR(instr.source)));
    MOTH_END_INSTR(UMinus)

    MOTH_BEGIN_INSTR(UCompl)
        STOREVALUE(instr.result, Runtime::complement(VALUEPTR(instr.source)));
    MOTH_END_INSTR(UCompl)

    MOTH_BEGIN_INSTR(UComplInt)
        VALUE(instr.result) = QV4::Encode((int)~VALUE(instr.source).integerValue());
    MOTH_END_INSTR(UComplInt)

    MOTH_BEGIN_INSTR(Increment)
        STOREVALUE(instr.result, Runtime::increment(VALUEPTR(instr.source)));
    MOTH_END_INSTR(Increment)

    MOTH_BEGIN_INSTR(Decrement)
        STOREVALUE(instr.result, Runtime::decrement(VALUEPTR(instr.source)));
    MOTH_END_INSTR(Decrement)

    MOTH_BEGIN_INSTR(Binop)
        STOREVALUE(instr.result, instr.alu(VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
    MOTH_END_INSTR(Binop)

    MOTH_BEGIN_INSTR(Add)
        STOREVALUE(instr.result, Runtime::add(context, VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
    MOTH_END_INSTR(Add)

    MOTH_BEGIN_INSTR(BitAnd)
        STOREVALUE(instr.result, Runtime::bitAnd(VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
    MOTH_END_INSTR(BitAnd)

    MOTH_BEGIN_INSTR(BitOr)
        STOREVALUE(instr.result, Runtime::bitOr(VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
    MOTH_END_INSTR(BitOr)

    MOTH_BEGIN_INSTR(BitXor)
        STOREVALUE(instr.result, Runtime::bitXor(VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
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
        STOREVALUE(instr.result, Runtime::mul(VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
    MOTH_END_INSTR(Mul)

    MOTH_BEGIN_INSTR(Sub)
        STOREVALUE(instr.result, Runtime::sub(VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
    MOTH_END_INSTR(Sub)

    MOTH_BEGIN_INSTR(BinopContext)
        STOREVALUE(instr.result, instr.alu(context, VALUEPTR(instr.lhs), VALUEPTR(instr.rhs)));
    MOTH_END_INSTR(BinopContext)

    MOTH_BEGIN_INSTR(Ret)
        context->engine()->stackPop(stackSize);
//        TRACE(Ret, "returning value %s", result.toString(context)->toQString().toUtf8().constData());
        return VALUE(instr.result).asReturnedValue();
    MOTH_END_INSTR(Ret)

    MOTH_BEGIN_INSTR(Debug)
        context->d()->lineNumber = instr.lineNumber;
        QV4::Debugging::Debugger *debugger = context->engine()->debugger;
        if (debugger && debugger->pauseAtNextOpportunity())
            debugger->maybeBreakAtInstruction();
    MOTH_END_INSTR(Debug)

    MOTH_BEGIN_INSTR(Line)
        context->d()->lineNumber = instr.lineNumber;
    MOTH_END_INSTR(Debug)

    MOTH_BEGIN_INSTR(LoadThis)
        VALUE(instr.result) = context->d()->callData->thisObject;
    MOTH_END_INSTR(LoadThis)

    MOTH_BEGIN_INSTR(LoadQmlIdArray)
        VALUE(instr.result) = Runtime::getQmlIdArray(static_cast<QV4::NoThrowContext*>(context));
    MOTH_END_INSTR(LoadQmlIdArray)

    MOTH_BEGIN_INSTR(LoadQmlImportedScripts)
        VALUE(instr.result) = Runtime::getQmlImportedScripts(static_cast<QV4::NoThrowContext*>(context));
    MOTH_END_INSTR(LoadQmlImportedScripts)

    MOTH_BEGIN_INSTR(LoadQmlContextObject)
        VALUE(instr.result) = Runtime::getQmlContextObject(static_cast<QV4::NoThrowContext*>(context));
    MOTH_END_INSTR(LoadContextObject)

    MOTH_BEGIN_INSTR(LoadQmlScopeObject)
        VALUE(instr.result) = Runtime::getQmlScopeObject(static_cast<QV4::NoThrowContext*>(context));
    MOTH_END_INSTR(LoadScopeObject)

    MOTH_BEGIN_INSTR(LoadQmlSingleton)
        VALUE(instr.result) = Runtime::getQmlSingleton(static_cast<QV4::NoThrowContext*>(context), runtimeStrings[instr.name]);
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
        if (!exceptionHandler) {
            context->engine()->stackPop(stackSize);
            return QV4::Encode::undefined();
        }
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

QV4::ReturnedValue VME::exec(QV4::ExecutionContext *ctxt, const uchar *code)
{
    VME vme;
    QV4::Debugging::Debugger *debugger = ctxt->engine()->debugger;
    if (debugger)
        debugger->enteringFunction();
    QV4::ReturnedValue retVal = vme.run(ctxt, code);
    if (debugger)
        debugger->leavingFunction(retVal);
    return retVal;
}
