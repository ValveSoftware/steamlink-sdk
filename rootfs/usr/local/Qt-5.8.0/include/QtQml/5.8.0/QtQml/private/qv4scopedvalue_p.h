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
#ifndef QV4SCOPEDVALUE_P_H
#define QV4SCOPEDVALUE_P_H

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

#include "qv4engine_p.h"
#include "qv4value_p.h"
#include "qv4persistent_p.h"
#include "qv4property_p.h"

#ifdef V4_USE_VALGRIND
#include <valgrind/memcheck.h>
#endif

QT_BEGIN_NAMESPACE

#define SAVE_JS_STACK(ctx) Value *__jsStack = ctx->engine->jsStackTop
#define CHECK_JS_STACK(ctx) Q_ASSERT(__jsStack == ctx->engine->jsStackTop)

namespace QV4 {

struct ScopedValue;

struct Scope {
    inline Scope(ExecutionContext *ctx)
        : engine(ctx->d()->engine)
        , mark(engine->jsStackTop)
        , result(*engine->jsAlloca(1))
    {
        result = Encode::undefined();
    }

    explicit Scope(ExecutionEngine *e)
        : engine(e)
        , mark(engine->jsStackTop)
        , result(*engine->jsAlloca(1))
    {
        result = Encode::undefined();
    }

    ~Scope() {
#ifndef QT_NO_DEBUG
        Q_ASSERT(engine->jsStackTop >= mark);
        Q_ASSERT(engine->currentContext < mark);
        memset(mark, 0, (engine->jsStackTop - mark)*sizeof(Value));
#endif
#ifdef V4_USE_VALGRIND
        VALGRIND_MAKE_MEM_UNDEFINED(mark, (engine->jsStackLimit - mark) * sizeof(Value));
#endif
        engine->jsStackTop = mark;
    }

    Value *alloc(int nValues) const {
        return engine->jsAlloca(nValues);
    }

    bool hasException() const {
        return engine->hasException;
    }

    ExecutionEngine *engine;
    Value *mark;
    Value &result;

private:
    Q_DISABLE_COPY(Scope)
};

struct ScopedValue
{
    ScopedValue(const Scope &scope)
    {
        ptr = scope.engine->jsStackTop++;
        ptr->setRawValue(0);
    }

    ScopedValue(const Scope &scope, const Value &v)
    {
        ptr = scope.engine->jsStackTop++;
        *ptr = v;
    }

    ScopedValue(const Scope &scope, Heap::Base *o)
    {
        ptr = scope.engine->jsStackTop++;
        ptr->setM(o);
    }

    ScopedValue(const Scope &scope, Managed *m)
    {
        ptr = scope.engine->jsStackTop++;
        ptr->setRawValue(m->asReturnedValue());
    }

    ScopedValue(const Scope &scope, const ReturnedValue &v)
    {
        ptr = scope.engine->jsStackTop++;
        ptr->setRawValue(v);
    }

    ScopedValue &operator=(const Value &v) {
        *ptr = v;
        return *this;
    }

    ScopedValue &operator=(Heap::Base *o) {
        ptr->setM(o);
        return *this;
    }

    ScopedValue &operator=(Managed *m) {
        *ptr = *m;
        return *this;
    }

    ScopedValue &operator=(const ReturnedValue &v) {
        ptr->setRawValue(v);
        return *this;
    }

    ScopedValue &operator=(const ScopedValue &other) {
        *ptr = *other.ptr;
        return *this;
    }

    Value *operator->() {
        return ptr;
    }

    const Value *operator->() const {
        return ptr;
    }

    operator Value *() { return ptr; }
    operator const Value &() const { return *ptr; }

    Value *ptr;
};

template<typename T>
struct Scoped
{
    enum ConvertType { Convert };

    inline void setPointer(const Managed *p) {
        ptr->setM(p ? p->m() : 0);
    }

    Scoped(const Scope &scope)
    {
        ptr = scope.engine->jsAlloca(1);
    }

    Scoped(const Scope &scope, const Value &v)
    {
        ptr = scope.engine->jsAlloca(1);
        setPointer(v.as<T>());
    }
    Scoped(const Scope &scope, Heap::Base *o)
    {
        Value v;
        v = o;
        ptr = scope.engine->jsAlloca(1);
        setPointer(v.as<T>());
    }
    Scoped(const Scope &scope, const ScopedValue &v)
    {
        ptr = scope.engine->jsAlloca(1);
        setPointer(v.ptr->as<T>());
    }

    Scoped(const Scope &scope, const Value &v, ConvertType)
    {
        ptr = scope.engine->jsAlloca(1);
        ptr->setRawValue(value_convert<T>(scope.engine, v));
    }

    Scoped(const Scope &scope, const Value *v)
    {
        ptr = scope.engine->jsAlloca(1);
        setPointer(v ? v->as<T>() : 0);
    }

    Scoped(const Scope &scope, T *t)
    {
        ptr = scope.engine->jsAlloca(1);
        setPointer(t);
    }
    Scoped(const Scope &scope, typename T::Data *t)
    {
        ptr = scope.engine->jsAlloca(1);
        *ptr = t;
    }

    Scoped(const Scope &scope, const ReturnedValue &v)
    {
        ptr = scope.engine->jsAlloca(1);
        setPointer(QV4::Value::fromReturnedValue(v).as<T>());
    }

    Scoped(const Scope &scope, const ReturnedValue &v, ConvertType)
    {
        ptr = scope.engine->jsAlloca(1);
        ptr->setRawValue(value_convert<T>(scope.engine, QV4::Value::fromReturnedValue(v)));
    }

    Scoped<T> &operator=(Heap::Base *o) {
        setPointer(Value::fromHeapObject(o).as<T>());
        return *this;
    }
    Scoped<T> &operator=(typename T::Data *t) {
        *ptr = t;
        return *this;
    }
    Scoped<T> &operator=(const Value &v) {
        setPointer(v.as<T>());
        return *this;
    }
    Scoped<T> &operator=(Value *v) {
        setPointer(v ? v->as<T>() : 0);
        return *this;
    }

    Scoped<T> &operator=(const ReturnedValue &v) {
        setPointer(QV4::Value::fromReturnedValue(v).as<T>());
        return *this;
    }

    Scoped<T> &operator=(const Scoped<T> &other) {
        *ptr = *other.ptr;
        return *this;
    }

    Scoped<T> &operator=(T *t) {
        setPointer(t);
        return *this;
    }

    operator T *() {
        return static_cast<T *>(ptr->managed());
    }
    operator const Value &() const {
        return *ptr;
    }

    T *operator->() {
        return ptr->cast<T>();
    }

    const T *operator->() const {
        return ptr->cast<T>();
    }

    bool operator!() const {
        return !ptr->m();
    }
    operator void *() const {
        return ptr->m();
    }

    T *getPointer() {
        return ptr->cast<T>();
    }
    Value *getRef() {
        return ptr;
    }

    ReturnedValue asReturnedValue() const {
        return ptr->m() ? ptr->rawValue() : Encode::undefined();
    }

    Value *ptr;
};

struct ScopedCallData {
    ScopedCallData(const Scope &scope, int argc = 0)
    {
        int size = qMax(argc, (int)QV4::Global::ReservedArgumentCount) + qOffsetOf(QV4::CallData, args)/sizeof(QV4::Value);
        ptr = reinterpret_cast<CallData *>(scope.alloc(size));
        ptr->tag = QV4::Value::Integer_Type_Internal;
        ptr->argc = argc;
    }

    CallData *operator->() {
        return ptr;
    }

    operator CallData *() const {
        return ptr;
    }

    CallData *ptr;
};


inline Value &Value::operator =(const ScopedValue &v)
{
    _val = v.ptr->rawValue();
    return *this;
}

template<typename T>
inline Value &Value::operator=(const Scoped<T> &t)
{
    _val = t.ptr->rawValue();
    return *this;
}

struct ScopedProperty
{
    ScopedProperty(Scope &scope)
    {
        property = reinterpret_cast<Property*>(scope.alloc(sizeof(Property) / sizeof(Value)));
        property->value = Encode::undefined();
        property->set = Encode::undefined();
    }

    Property *operator->() { return property; }
    operator const Property *() const { return property; }
    operator Property *() { return property; }

    Property *property;
};

struct ExecutionContextSaver
{
    Scope scope; // this makes sure that a reference to context on the JS stack goes out of scope as soon as the context is not used anymore.
    ExecutionContext *savedContext;

    ExecutionContextSaver(const Scope &scope)
        : scope(scope.engine)
    {
        savedContext = scope.engine->currentContext;
    }
    ~ExecutionContextSaver()
    {
        Q_ASSERT(scope.engine->jsStackTop > scope.engine->currentContext);
        scope.engine->currentContext = savedContext;
        scope.engine->current = savedContext->d();
    }
};


}

QT_END_NAMESPACE

#endif
