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

#define RUNTIME_METHOD(returnvalue, name, args) \
    typedef returnvalue (*Method_##name)args; \
    enum { Method_##name##_NeedsExceptionCheck = ExceptionCheck<Method_##name>::NeedsCheck }; \
    static returnvalue method_##name args; \
    const Method_##name name

#define INIT_RUNTIME_METHOD(name) \
    name(method_##name)

struct Q_QML_PRIVATE_EXPORT Runtime {
    Runtime()
        : INIT_RUNTIME_METHOD(callGlobalLookup)
        , INIT_RUNTIME_METHOD(callActivationProperty)
        , INIT_RUNTIME_METHOD(callQmlScopeObjectProperty)
        , INIT_RUNTIME_METHOD(callQmlContextObjectProperty)
        , INIT_RUNTIME_METHOD(callProperty)
        , INIT_RUNTIME_METHOD(callPropertyLookup)
        , INIT_RUNTIME_METHOD(callElement)
        , INIT_RUNTIME_METHOD(callValue)
        , INIT_RUNTIME_METHOD(constructGlobalLookup)
        , INIT_RUNTIME_METHOD(constructActivationProperty)
        , INIT_RUNTIME_METHOD(constructProperty)
        , INIT_RUNTIME_METHOD(constructPropertyLookup)
        , INIT_RUNTIME_METHOD(constructValue)
        , INIT_RUNTIME_METHOD(setActivationProperty)
        , INIT_RUNTIME_METHOD(setProperty)
        , INIT_RUNTIME_METHOD(setElement)
        , INIT_RUNTIME_METHOD(getProperty)
        , INIT_RUNTIME_METHOD(getActivationProperty)
        , INIT_RUNTIME_METHOD(getElement)
        , INIT_RUNTIME_METHOD(typeofValue)
        , INIT_RUNTIME_METHOD(typeofName)
        , INIT_RUNTIME_METHOD(typeofScopeObjectProperty)
        , INIT_RUNTIME_METHOD(typeofContextObjectProperty)
        , INIT_RUNTIME_METHOD(typeofMember)
        , INIT_RUNTIME_METHOD(typeofElement)
        , INIT_RUNTIME_METHOD(deleteElement)
        , INIT_RUNTIME_METHOD(deleteMember)
        , INIT_RUNTIME_METHOD(deleteMemberString)
        , INIT_RUNTIME_METHOD(deleteName)
        , INIT_RUNTIME_METHOD(throwException)
        , INIT_RUNTIME_METHOD(unwindException)
        , INIT_RUNTIME_METHOD(pushWithScope)
        , INIT_RUNTIME_METHOD(pushCatchScope)
        , INIT_RUNTIME_METHOD(popScope)
        , INIT_RUNTIME_METHOD(closure)
        , INIT_RUNTIME_METHOD(declareVar)
        , INIT_RUNTIME_METHOD(setupArgumentsObject)
        , INIT_RUNTIME_METHOD(convertThisToObject)
        , INIT_RUNTIME_METHOD(arrayLiteral)
        , INIT_RUNTIME_METHOD(objectLiteral)
        , INIT_RUNTIME_METHOD(regexpLiteral)
        , INIT_RUNTIME_METHOD(foreachIterator)
        , INIT_RUNTIME_METHOD(foreachNextPropertyName)
        , INIT_RUNTIME_METHOD(uPlus)
        , INIT_RUNTIME_METHOD(uMinus)
        , INIT_RUNTIME_METHOD(uNot)
        , INIT_RUNTIME_METHOD(complement)
        , INIT_RUNTIME_METHOD(increment)
        , INIT_RUNTIME_METHOD(decrement)
        , INIT_RUNTIME_METHOD(instanceof)
        , INIT_RUNTIME_METHOD(in)
        , INIT_RUNTIME_METHOD(add)
        , INIT_RUNTIME_METHOD(addString)
        , INIT_RUNTIME_METHOD(bitOr)
        , INIT_RUNTIME_METHOD(bitXor)
        , INIT_RUNTIME_METHOD(bitAnd)
        , INIT_RUNTIME_METHOD(sub)
        , INIT_RUNTIME_METHOD(mul)
        , INIT_RUNTIME_METHOD(div)
        , INIT_RUNTIME_METHOD(mod)
        , INIT_RUNTIME_METHOD(shl)
        , INIT_RUNTIME_METHOD(shr)
        , INIT_RUNTIME_METHOD(ushr)
        , INIT_RUNTIME_METHOD(greaterThan)
        , INIT_RUNTIME_METHOD(lessThan)
        , INIT_RUNTIME_METHOD(greaterEqual)
        , INIT_RUNTIME_METHOD(lessEqual)
        , INIT_RUNTIME_METHOD(equal)
        , INIT_RUNTIME_METHOD(notEqual)
        , INIT_RUNTIME_METHOD(strictEqual)
        , INIT_RUNTIME_METHOD(strictNotEqual)
        , INIT_RUNTIME_METHOD(compareGreaterThan)
        , INIT_RUNTIME_METHOD(compareLessThan)
        , INIT_RUNTIME_METHOD(compareGreaterEqual)
        , INIT_RUNTIME_METHOD(compareLessEqual)
        , INIT_RUNTIME_METHOD(compareEqual)
        , INIT_RUNTIME_METHOD(compareNotEqual)
        , INIT_RUNTIME_METHOD(compareStrictEqual)
        , INIT_RUNTIME_METHOD(compareStrictNotEqual)
        , INIT_RUNTIME_METHOD(compareInstanceof)
        , INIT_RUNTIME_METHOD(compareIn)
        , INIT_RUNTIME_METHOD(toBoolean)
        , INIT_RUNTIME_METHOD(toDouble)
        , INIT_RUNTIME_METHOD(toInt)
        , INIT_RUNTIME_METHOD(doubleToInt)
        , INIT_RUNTIME_METHOD(toUInt)
        , INIT_RUNTIME_METHOD(doubleToUInt)
        , INIT_RUNTIME_METHOD(getQmlContext)
        , INIT_RUNTIME_METHOD(getQmlImportedScripts)
        , INIT_RUNTIME_METHOD(getQmlSingleton)
        , INIT_RUNTIME_METHOD(getQmlAttachedProperty)
        , INIT_RUNTIME_METHOD(getQmlScopeObjectProperty)
        , INIT_RUNTIME_METHOD(getQmlContextObjectProperty)
        , INIT_RUNTIME_METHOD(getQmlQObjectProperty)
        , INIT_RUNTIME_METHOD(getQmlSingletonQObjectProperty)
        , INIT_RUNTIME_METHOD(getQmlIdObject)
        , INIT_RUNTIME_METHOD(setQmlScopeObjectProperty)
        , INIT_RUNTIME_METHOD(setQmlContextObjectProperty)
        , INIT_RUNTIME_METHOD(setQmlQObjectProperty)
    { }

    // call
    RUNTIME_METHOD(ReturnedValue, callGlobalLookup, (ExecutionEngine *engine, uint index, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, callActivationProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, callQmlScopeObjectProperty, (ExecutionEngine *engine, int propertyIndex, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, callQmlContextObjectProperty, (ExecutionEngine *engine, int propertyIndex, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, callProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, callPropertyLookup, (ExecutionEngine *engine, uint index, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, callElement, (ExecutionEngine *engine, const Value &index, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, callValue, (ExecutionEngine *engine, const Value &func, CallData *callData));

    // construct
    RUNTIME_METHOD(ReturnedValue, constructGlobalLookup, (ExecutionEngine *engine, uint index, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, constructActivationProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, constructProperty, (ExecutionEngine *engine, int nameIndex, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, constructPropertyLookup, (ExecutionEngine *engine, uint index, CallData *callData));
    RUNTIME_METHOD(ReturnedValue, constructValue, (ExecutionEngine *engine, const Value &func, CallData *callData));

    // set & get
    RUNTIME_METHOD(void, setActivationProperty, (ExecutionEngine *engine, int nameIndex, const Value &value));
    RUNTIME_METHOD(void, setProperty, (ExecutionEngine *engine, const Value &object, int nameIndex, const Value &value));
    RUNTIME_METHOD(void, setElement, (ExecutionEngine *engine, const Value &object, const Value &index, const Value &value));
    RUNTIME_METHOD(ReturnedValue, getProperty, (ExecutionEngine *engine, const Value &object, int nameIndex));
    RUNTIME_METHOD(ReturnedValue, getActivationProperty, (ExecutionEngine *engine, int nameIndex));
    RUNTIME_METHOD(ReturnedValue, getElement, (ExecutionEngine *engine, const Value &object, const Value &index));

    // typeof
    RUNTIME_METHOD(ReturnedValue, typeofValue, (ExecutionEngine *engine, const Value &val));
    RUNTIME_METHOD(ReturnedValue, typeofName, (ExecutionEngine *engine, int nameIndex));
    RUNTIME_METHOD(ReturnedValue, typeofScopeObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex));
    RUNTIME_METHOD(ReturnedValue, typeofContextObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex));
    RUNTIME_METHOD(ReturnedValue, typeofMember, (ExecutionEngine *engine, const Value &base, int nameIndex));
    RUNTIME_METHOD(ReturnedValue, typeofElement, (ExecutionEngine *engine, const Value &base, const Value &index));

    // delete
    RUNTIME_METHOD(ReturnedValue, deleteElement, (ExecutionEngine *engine, const Value &base, const Value &index));
    RUNTIME_METHOD(ReturnedValue, deleteMember, (ExecutionEngine *engine, const Value &base, int nameIndex));
    RUNTIME_METHOD(ReturnedValue, deleteMemberString, (ExecutionEngine *engine, const Value &base, String *name));
    RUNTIME_METHOD(ReturnedValue, deleteName, (ExecutionEngine *engine, int nameIndex));

    // exceptions & scopes
    RUNTIME_METHOD(void, throwException, (ExecutionEngine *engine, const Value &value));
    RUNTIME_METHOD(ReturnedValue, unwindException, (ExecutionEngine *engine));
    RUNTIME_METHOD(void, pushWithScope, (const Value &o, NoThrowEngine *engine));
    RUNTIME_METHOD(void, pushCatchScope, (NoThrowEngine *engine, int exceptionVarNameIndex));
    RUNTIME_METHOD(void, popScope, (NoThrowEngine *engine));

    // closures
    RUNTIME_METHOD(ReturnedValue, closure, (ExecutionEngine *engine, int functionId));

    // function header
    RUNTIME_METHOD(void, declareVar, (ExecutionEngine *engine, bool deletable, int nameIndex));
    RUNTIME_METHOD(ReturnedValue, setupArgumentsObject, (ExecutionEngine *engine));
    RUNTIME_METHOD(void, convertThisToObject, (ExecutionEngine *engine));

    // literals
    RUNTIME_METHOD(ReturnedValue, arrayLiteral, (ExecutionEngine *engine, Value *values, uint length));
    RUNTIME_METHOD(ReturnedValue, objectLiteral, (ExecutionEngine *engine, const Value *args, int classId, int arrayValueCount, int arrayGetterSetterCountAndFlags));
    RUNTIME_METHOD(ReturnedValue, regexpLiteral, (ExecutionEngine *engine, int id));

    // foreach
    RUNTIME_METHOD(ReturnedValue, foreachIterator, (ExecutionEngine *engine, const Value &in));
    RUNTIME_METHOD(ReturnedValue, foreachNextPropertyName, (const Value &foreach_iterator));

    // unary operators
    typedef ReturnedValue (*UnaryOperation)(const Value &value);
    RUNTIME_METHOD(ReturnedValue, uPlus, (const Value &value));
    RUNTIME_METHOD(ReturnedValue, uMinus, (const Value &value));
    RUNTIME_METHOD(ReturnedValue, uNot, (const Value &value));
    RUNTIME_METHOD(ReturnedValue, complement, (const Value &value));
    RUNTIME_METHOD(ReturnedValue, increment, (const Value &value));
    RUNTIME_METHOD(ReturnedValue, decrement, (const Value &value));

    // binary operators
    typedef ReturnedValue (*BinaryOperation)(const Value &left, const Value &right);
    typedef ReturnedValue (*BinaryOperationContext)(ExecutionEngine *engine, const Value &left, const Value &right);

    RUNTIME_METHOD(ReturnedValue, instanceof, (ExecutionEngine *engine, const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, in, (ExecutionEngine *engine, const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, add, (ExecutionEngine *engine, const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, addString, (ExecutionEngine *engine, const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, bitOr, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, bitXor, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, bitAnd, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, sub, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, mul, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, div, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, mod, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, shl, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, shr, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, ushr, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, greaterThan, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, lessThan, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, greaterEqual, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, lessEqual, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, equal, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, notEqual, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, strictEqual, (const Value &left, const Value &right));
    RUNTIME_METHOD(ReturnedValue, strictNotEqual, (const Value &left, const Value &right));

    // comparisons
    RUNTIME_METHOD(Bool, compareGreaterThan, (const Value &l, const Value &r));
    RUNTIME_METHOD(Bool, compareLessThan, (const Value &l, const Value &r));
    RUNTIME_METHOD(Bool, compareGreaterEqual, (const Value &l, const Value &r));
    RUNTIME_METHOD(Bool, compareLessEqual, (const Value &l, const Value &r));
    RUNTIME_METHOD(Bool, compareEqual, (const Value &left, const Value &right));
    RUNTIME_METHOD(Bool, compareNotEqual, (const Value &left, const Value &right));
    RUNTIME_METHOD(Bool, compareStrictEqual, (const Value &left, const Value &right));
    RUNTIME_METHOD(Bool, compareStrictNotEqual, (const Value &left, const Value &right));

    RUNTIME_METHOD(Bool, compareInstanceof, (ExecutionEngine *engine, const Value &left, const Value &right));
    RUNTIME_METHOD(Bool, compareIn, (ExecutionEngine *engine, const Value &left, const Value &right));

    // conversions
    RUNTIME_METHOD(Bool, toBoolean, (const Value &value));
    RUNTIME_METHOD(ReturnedValue, toDouble, (const Value &value));
    RUNTIME_METHOD(int, toInt, (const Value &value));
    RUNTIME_METHOD(int, doubleToInt, (const double &d));
    RUNTIME_METHOD(unsigned, toUInt, (const Value &value));
    RUNTIME_METHOD(unsigned, doubleToUInt, (const double &d));

    // qml
    RUNTIME_METHOD(ReturnedValue, getQmlContext, (NoThrowEngine *engine));
    RUNTIME_METHOD(ReturnedValue, getQmlImportedScripts, (NoThrowEngine *engine));
    RUNTIME_METHOD(ReturnedValue, getQmlSingleton, (NoThrowEngine *engine, int nameIndex));
    RUNTIME_METHOD(ReturnedValue, getQmlAttachedProperty, (ExecutionEngine *engine, int attachedPropertiesId, int propertyIndex));
    RUNTIME_METHOD(ReturnedValue, getQmlScopeObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, bool captureRequired));
    RUNTIME_METHOD(ReturnedValue, getQmlContextObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, bool captureRequired));
    RUNTIME_METHOD(ReturnedValue, getQmlQObjectProperty, (ExecutionEngine *engine, const Value &object, int propertyIndex, bool captureRequired));
    RUNTIME_METHOD(ReturnedValue, getQmlSingletonQObjectProperty, (ExecutionEngine *engine, const Value &object, int propertyIndex, bool captureRequired));
    RUNTIME_METHOD(ReturnedValue, getQmlIdObject, (ExecutionEngine *engine, const Value &context, uint index));

    RUNTIME_METHOD(void, setQmlScopeObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, const Value &value));
    RUNTIME_METHOD(void, setQmlContextObjectProperty, (ExecutionEngine *engine, const Value &context, int propertyIndex, const Value &value));
    RUNTIME_METHOD(void, setQmlQObjectProperty, (ExecutionEngine *engine, const Value &object, int propertyIndex, const Value &value));
};

#undef RUNTIME_METHOD
#undef INIT_RUNTIME_METHOD

} // namespace QV4

QT_END_NAMESPACE

#endif // QV4RUNTIMEAPI_P_H
