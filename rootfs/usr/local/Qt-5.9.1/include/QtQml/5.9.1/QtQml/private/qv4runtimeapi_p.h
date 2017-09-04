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
#ifndef QV4RUNTIMEAPI_P_H
#define QV4RUNTIMEAPI_P_H

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

#include <private/qv4global_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

typedef uint Bool;
struct NoThrowEngine;

namespace {
template <typename T>
struct ExceptionCheck {
    enum { NeedsCheck = 1 };
};
// push_catch and pop context methods shouldn't check for exceptions
template <>
struct ExceptionCheck<void (*)(QV4::NoThrowEngine *)> {
    enum { NeedsCheck = 0 };
};
template <typename A>
struct ExceptionCheck<void (*)(A, QV4::NoThrowEngine)> {
    enum { NeedsCheck = 0 };
};
template <>
struct ExceptionCheck<QV4::ReturnedValue (*)(QV4::NoThrowEngine *)> {
    enum { NeedsCheck = 0 };
};
template <typename A>
struct ExceptionCheck<QV4::ReturnedValue (*)(QV4::NoThrowEngine *, A)> {
    enum { NeedsCheck = 0 };
};
template <typename A, typename B>
struct ExceptionCheck<QV4::ReturnedValue (*)(QV4::NoThrowEngine *, A, B)> {
    enum { NeedsCheck = 0 };
};
template <typename A, typename B, typename C>
struct ExceptionCheck<void (*)(QV4::NoThrowEngine *, A, B, C)> {
    enum { NeedsCheck = 0 };
};
} // anonymous namespace

#define FOR_EACH_RUNTIME_METHOD(F) \
    /* call */ \
    F(ReturnedValue, callGlobalLookup, (ExecutionEngine *engine, uint index, CallData *callData)) \
    F(ReturnedValue, callActivationProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData)) \
    F(ReturnedValue, callQmlScopeObjectProperty, (ExecutionEngine *engine, int propertyIndex, CallData *callData)) \
    F(ReturnedValue, callQmlContextObjectProperty, (ExecutionEngine *engine, int propertyIndex, CallData *callData)) \
    F(ReturnedValue, callProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData)) \
    F(ReturnedValue, callPropertyLookup, (ExecutionEngine *engine, uint index, CallData *callData)) \
    F(ReturnedValue, callElement, (ExecutionEngine *engine, const Value &index, CallData *callData)) \
    F(ReturnedValue, callValue, (ExecutionEngine *engine, const Value &func, CallData *callData)) \
    \
    /* construct */ \
    F(ReturnedValue, constructGlobalLookup, (ExecutionEngine *engine, uint index, CallData *callData)) \
    F(ReturnedValue, constructActivationProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData)) \
    F(ReturnedValue, constructProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData)) \
    F(ReturnedValue, constructPropertyLookup, (ExecutionEngine *engine, uint index, CallData *callData)) \
    F(ReturnedValue, constructValue, (ExecutionEngine *engine, const Value &func, CallData *callData)) \
    \
    /* set & get */ \
    F(void, setActivationProperty, (ExecutionEngine *engine, int nameIndex, const Value &value)) \
    F(void, setProperty, (ExecutionEngine *engine, const Value &object, int nameIndex, const Value &value)) \
    F(void, setElement, (ExecutionEngine *engine, const Value &object, const Value &index, const Value &value)) \
    F(ReturnedValue, getProperty, (ExecutionEngine *engine, const Value &object, int nameIndex)) \
    F(ReturnedValue, getActivationProperty, (ExecutionEngine *engine, int nameIndex)) \
    F(ReturnedValue, getElement, (ExecutionEngine *engine, const Value &object, const Value &index)) \
    \
    /* typeof */  \
    F(ReturnedValue, typeofValue, (ExecutionEngine *engine, const Value &val)) \
    F(ReturnedValue, typeofName, (ExecutionEngine *engine, int nameIndex)) \
    F(ReturnedValue, typeofScopeObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex)) \
    F(ReturnedValue, typeofContextObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex)) \
    F(ReturnedValue, typeofMember, (ExecutionEngine *engine, const Value &base, int nameIndex)) \
    F(ReturnedValue, typeofElement, (ExecutionEngine *engine, const Value &base, const Value &index)) \
    \
    /* delete */ \
    F(ReturnedValue, deleteElement, (ExecutionEngine *engine, const Value &base, const Value &index)) \
    F(ReturnedValue, deleteMember, (ExecutionEngine *engine, const Value &base, int nameIndex)) \
    F(ReturnedValue, deleteMemberString, (ExecutionEngine *engine, const Value &base, String *name)) \
    F(ReturnedValue, deleteName, (ExecutionEngine *engine, int nameIndex)) \
    \
    /* exceptions & scopes */ \
    F(void, throwException, (ExecutionEngine *engine, const Value &value)) \
    F(ReturnedValue, unwindException, (ExecutionEngine *engine)) \
    F(void, pushWithScope, (const Value &o, NoThrowEngine *engine)) \
    F(void, pushCatchScope, (NoThrowEngine *engine, int exceptionVarNameIndex)) \
    F(void, popScope, (NoThrowEngine *engine)) \
    \
    /* closures */ \
    F(ReturnedValue, closure, (ExecutionEngine *engine, int functionId)) \
    \
    /* function header */ \
    F(void, declareVar, (ExecutionEngine *engine, bool deletable, int nameIndex)) \
    F(ReturnedValue, setupArgumentsObject, (ExecutionEngine *engine)) \
    F(void, convertThisToObject, (ExecutionEngine *engine)) \
    \
    /* literals */ \
    F(ReturnedValue, arrayLiteral, (ExecutionEngine *engine, Value *values, uint length)) \
    F(ReturnedValue, objectLiteral, (ExecutionEngine *engine, const Value *args, int classId, int arrayValueCount, int arrayGetterSetterCountAndFlags)) \
    F(ReturnedValue, regexpLiteral, (ExecutionEngine *engine, int id)) \
    \
    /* foreach */ \
    F(ReturnedValue, foreachIterator, (ExecutionEngine *engine, const Value &in)) \
    F(ReturnedValue, foreachNextPropertyName, (const Value &foreach_iterator)) \
    \
    /* unary operators */ \
    F(ReturnedValue, uPlus, (const Value &value)) \
    F(ReturnedValue, uMinus, (const Value &value)) \
    F(ReturnedValue, uNot, (const Value &value)) \
    F(ReturnedValue, complement, (const Value &value)) \
    F(ReturnedValue, increment, (const Value &value)) \
    F(ReturnedValue, decrement, (const Value &value)) \
    \
    /* binary operators */ \
    F(ReturnedValue, instanceof, (ExecutionEngine *engine, const Value &left, const Value &right)) \
    F(ReturnedValue, in, (ExecutionEngine *engine, const Value &left, const Value &right)) \
    F(ReturnedValue, add, (ExecutionEngine *engine, const Value &left, const Value &right)) \
    F(ReturnedValue, addString, (ExecutionEngine *engine, const Value &left, const Value &right)) \
    F(ReturnedValue, bitOr, (const Value &left, const Value &right)) \
    F(ReturnedValue, bitXor, (const Value &left, const Value &right)) \
    F(ReturnedValue, bitAnd, (const Value &left, const Value &right)) \
    F(ReturnedValue, sub, (const Value &left, const Value &right)) \
    F(ReturnedValue, mul, (const Value &left, const Value &right)) \
    F(ReturnedValue, div, (const Value &left, const Value &right)) \
    F(ReturnedValue, mod, (const Value &left, const Value &right)) \
    F(ReturnedValue, shl, (const Value &left, const Value &right)) \
    F(ReturnedValue, shr, (const Value &left, const Value &right)) \
    F(ReturnedValue, ushr, (const Value &left, const Value &right)) \
    F(ReturnedValue, greaterThan, (const Value &left, const Value &right)) \
    F(ReturnedValue, lessThan, (const Value &left, const Value &right)) \
    F(ReturnedValue, greaterEqual, (const Value &left, const Value &right)) \
    F(ReturnedValue, lessEqual, (const Value &left, const Value &right)) \
    F(ReturnedValue, equal, (const Value &left, const Value &right)) \
    F(ReturnedValue, notEqual, (const Value &left, const Value &right)) \
    F(ReturnedValue, strictEqual, (const Value &left, const Value &right)) \
    F(ReturnedValue, strictNotEqual, (const Value &left, const Value &right)) \
    \
    /* comparisons */ \
    F(Bool, compareGreaterThan, (const Value &l, const Value &r)) \
    F(Bool, compareLessThan, (const Value &l, const Value &r)) \
    F(Bool, compareGreaterEqual, (const Value &l, const Value &r)) \
    F(Bool, compareLessEqual, (const Value &l, const Value &r)) \
    F(Bool, compareEqual, (const Value &left, const Value &right)) \
    F(Bool, compareNotEqual, (const Value &left, const Value &right)) \
    F(Bool, compareStrictEqual, (const Value &left, const Value &right)) \
    F(Bool, compareStrictNotEqual, (const Value &left, const Value &right)) \
    \
    F(Bool, compareInstanceof, (ExecutionEngine *engine, const Value &left, const Value &right)) \
    F(Bool, compareIn, (ExecutionEngine *engine, const Value &left, const Value &right)) \
    \
    /* conversions */ \
    F(Bool, toBoolean, (const Value &value)) \
    F(ReturnedValue, toDouble, (const Value &value)) \
    F(int, toInt, (const Value &value)) \
    F(int, doubleToInt, (const double &d)) \
    F(unsigned, toUInt, (const Value &value)) \
    F(unsigned, doubleToUInt, (const double &d)) \
    \
    /* qml */ \
    F(ReturnedValue, getQmlContext, (NoThrowEngine *engine)) \
    F(ReturnedValue, getQmlImportedScripts, (NoThrowEngine *engine)) \
    F(ReturnedValue, getQmlSingleton, (NoThrowEngine *engine, int nameIndex)) \
    F(ReturnedValue, getQmlAttachedProperty, (ExecutionEngine *engine, int attachedPropertiesId, int propertyIndex)) \
    F(ReturnedValue, getQmlScopeObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, bool captureRequired)) \
    F(ReturnedValue, getQmlContextObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, bool captureRequired)) \
    F(ReturnedValue, getQmlQObjectProperty, (ExecutionEngine *engine, const Value &object, int propertyIndex, bool captureRequired)) \
    F(ReturnedValue, getQmlSingletonQObjectProperty, (ExecutionEngine *engine, const Value &object, int propertyIndex, bool captureRequired)) \
    F(ReturnedValue, getQmlIdObject, (ExecutionEngine *engine, const Value &context, uint index)) \
    \
    F(void, setQmlScopeObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, const Value &value)) \
    F(void, setQmlContextObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, const Value &value)) \
    F(void, setQmlQObjectProperty, (ExecutionEngine *engine, const Value &object, int propertyIndex, const Value &value))

struct Q_QML_PRIVATE_EXPORT Runtime {
    Runtime();

    typedef ReturnedValue (*UnaryOperation)(const Value &value);
    typedef ReturnedValue (*BinaryOperation)(const Value &left, const Value &right);
    typedef ReturnedValue (*BinaryOperationContext)(ExecutionEngine *engine, const Value &left, const Value &right);

#define DEFINE_RUNTIME_METHOD_ENUM(returnvalue, name, args) name,
    enum RuntimeMethods {
        FOR_EACH_RUNTIME_METHOD(DEFINE_RUNTIME_METHOD_ENUM)
        RuntimeMethodCount,
        InvalidRuntimeMethod = RuntimeMethodCount
    };
#undef DEFINE_RUNTIME_METHOD_ENUM

    void *runtimeMethods[RuntimeMethodCount];

    static uint runtimeMethodOffset(RuntimeMethods method) { return method*QT_POINTER_SIZE; }

#define RUNTIME_METHOD(returnvalue, name, args) \
    typedef returnvalue (*Method_##name)args; \
    enum { Method_##name##_NeedsExceptionCheck = ExceptionCheck<Method_##name>::NeedsCheck }; \
    static returnvalue method_##name args;
    FOR_EACH_RUNTIME_METHOD(RUNTIME_METHOD)
#undef RUNTIME_METHOD

};

static_assert(std::is_standard_layout<Runtime>::value, "Runtime needs to be standard layout in order for us to be able to use offsetof");
static_assert(offsetof(Runtime, runtimeMethods) == 0, "JIT expects this to be the first member");
static_assert(sizeof(Runtime::BinaryOperation) == sizeof(void*), "JIT expects a function pointer to fit into a regular pointer, for cross-compilation offset translation");

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4RUNTIMEAPI_P_H
