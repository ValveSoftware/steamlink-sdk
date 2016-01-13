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

#include "qv4global_p.h"
#include "qv4runtime_p.h"
#ifndef V4_BOOTSTRAP
#include "qv4object_p.h"
#include "qv4jsir_p.h"
#include "qv4objectproto_p.h"
#include "qv4globalobject_p.h"
#include "qv4stringobject_p.h"
#include "qv4argumentsobject_p.h"
#include "qv4lookup_p.h"
#include "qv4function_p.h"
#include "private/qlocale_tools_p.h"
#include "qv4scopedvalue_p.h"
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmltypewrapper_p.h>
#include "qv4qobjectwrapper_p.h"
#include <private/qv8engine_p.h>
#endif

#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>
#include <QtCore/QDebug>
#include <cstdio>
#include <cassert>
#include <typeinfo>
#include <stdlib.h>

#include "../../3rdparty/double-conversion/double-conversion.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

#ifdef QV4_COUNT_RUNTIME_FUNCTIONS
struct RuntimeCounters::Data {
    enum Type {
        None = 0,
        Undefined = 1,
        Null = 2,
        Boolean = 3,
        Integer = 4,
        Managed = 5,
        Double = 7
    };

    static const char *pretty(Type t) {
        switch (t) {
        case None: return "";
        case Undefined: return "Undefined";
        case Null: return "Null";
        case Boolean: return "Boolean";
        case Integer: return "Integer";
        case Managed: return "Managed";
        case Double: return "Double";
        default: return "Unknown";
        }
    }

    static unsigned mangle(unsigned tag) {
        switch (tag) {
        case Value::Undefined_Type: return Undefined;
        case Value::Null_Type: return Null;
        case Value::Boolean_Type: return Boolean;
        case Value::Integer_Type: return Integer;
        case Value::Managed_Type: return Managed;
        default: return Double;
        }
    }

    static unsigned mangle(unsigned tag1, unsigned tag2) {
        return (mangle(tag1) << 3) | mangle(tag2);
    }

    static void unmangle(unsigned signature, Type &tag1, Type &tag2) {
        tag1 = Type((signature >> 3) & 7);
        tag2 = Type(signature & 7);
    }

    typedef QVector<quint64> Counters;
    QHash<const char *, Counters> counters;

    inline void count(const char *func) {
        QVector<quint64> &cnt = counters[func];
        if (cnt.isEmpty())
            cnt.resize(64);
        cnt[0] += 1;
    }

    inline void count(const char *func, unsigned tag) {
        QVector<quint64> &cnt = counters[func];
        if (cnt.isEmpty())
            cnt.resize(64);
        cnt[mangle(tag)] += 1;
    }

    inline void count(const char *func, unsigned tag1, unsigned tag2) {
        QVector<quint64> &cnt = counters[func];
        if (cnt.isEmpty())
            cnt.resize(64);
        cnt[mangle(tag1, tag2)] += 1;
    }

    struct Line {
        const char *func;
        Type tag1, tag2;
        quint64 count;

        static bool less(const Line &line1, const Line &line2) {
            return line1.count > line2.count;
        }
    };

    void dump() const {
        QTextStream outs(stderr, QIODevice::WriteOnly);
        QList<Line> lines;
        foreach (const char *func, counters.keys()) {
            const Counters &fCount = counters[func];
            for (int i = 0, ei = fCount.size(); i != ei; ++i) {
                quint64 count = fCount[i];
                if (!count)
                    continue;
                Line line;
                line.func = func;
                unmangle(i, line.tag1, line.tag2);
                line.count = count;
                lines.append(line);
            }
        }
        qSort(lines.begin(), lines.end(), Line::less);
        outs << lines.size() << " counters:" << endl;
        foreach (const Line &line, lines)
            outs << qSetFieldWidth(10) << line.count << qSetFieldWidth(0)
                 << " | " << line.func
                 << " | " << pretty(line.tag1)
                 << " | " << pretty(line.tag2)
                 << endl;
    }
};

RuntimeCounters *RuntimeCounters::instance = 0;
static RuntimeCounters runtimeCountersInstance;
RuntimeCounters::RuntimeCounters()
    : d(new Data)
{
    if (!instance)
        instance = this;
}

RuntimeCounters::~RuntimeCounters()
{
    d->dump();
    delete d;
}

void RuntimeCounters::count(const char *func)
{
    d->count(func);
}

void RuntimeCounters::count(const char *func, uint tag)
{
    d->count(func, tag);
}

void RuntimeCounters::count(const char *func, uint tag1, uint tag2)
{
    d->count(func, tag1, tag2);
}

#endif // QV4_COUNT_RUNTIME_FUNCTIONS

#ifndef V4_BOOTSTRAP
void RuntimeHelpers::numberToString(QString *result, double num, int radix)
{
    Q_ASSERT(result);

    if (std::isnan(num)) {
        *result = QStringLiteral("NaN");
        return;
    } else if (qIsInf(num)) {
        *result = num < 0 ? QStringLiteral("-Infinity") : QStringLiteral("Infinity");
        return;
    }

    if (radix == 10) {
        char str[100];
        double_conversion::StringBuilder builder(str, sizeof(str));
        double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToShortest(num, &builder);
        *result = QString::fromLatin1(builder.Finalize());
        return;
    }

    result->clear();
    bool negative = false;

    if (num < 0) {
        negative = true;
        num = -num;
    }

    double frac = num - ::floor(num);
    num = Primitive::toInteger(num);

    do {
        char c = (char)::fmod(num, radix);
        c = (c < 10) ? (c + '0') : (c - 10 + 'a');
        result->prepend(QLatin1Char(c));
        num = ::floor(num / radix);
    } while (num != 0);

    if (frac != 0) {
        result->append(QLatin1Char('.'));
        do {
            frac = frac * radix;
            char c = (char)::floor(frac);
            c = (c < 10) ? (c + '0') : (c - 10 + 'a');
            result->append(QLatin1Char(c));
            frac = frac - ::floor(frac);
        } while (frac != 0);
    }

    if (negative)
        result->prepend(QLatin1Char('-'));
}

ReturnedValue Runtime::closure(ExecutionContext *ctx, int functionId)
{
    QV4::Function *clos = ctx->d()->compilationUnit->runtimeFunctions[functionId];
    Q_ASSERT(clos);
    return FunctionObject::createScriptFunction(ctx, clos)->asReturnedValue();
}

ReturnedValue Runtime::deleteElement(ExecutionContext *ctx, const ValueRef base, const ValueRef index)
{
    Scope scope(ctx);
    ScopedObject o(scope, base);
    if (o) {
        uint n = index->asArrayIndex();
        if (n < UINT_MAX) {
            return Encode((bool)o->deleteIndexedProperty(n));
        }
    }

    ScopedString name(scope, index->toString(ctx));
    return Runtime::deleteMember(ctx, base, name.getPointer());
}

ReturnedValue Runtime::deleteMember(ExecutionContext *ctx, const ValueRef base, String *name)
{
    Scope scope(ctx);
    ScopedObject obj(scope, base->toObject(ctx));
    if (scope.engine->hasException)
        return Encode::undefined();
    return Encode(obj->deleteProperty(name));
}

ReturnedValue Runtime::deleteName(ExecutionContext *ctx, String *name)
{
    Scope scope(ctx);
    return Encode(ctx->deleteProperty(name));
}

QV4::ReturnedValue Runtime::instanceof(ExecutionContext *ctx, const ValueRef left, const ValueRef right)
{
    // As nothing in this method can call into the memory manager, avoid using a Scope
    // for performance reasons

    FunctionObject *f = right->asFunctionObject();
    if (!f)
        return ctx->throwTypeError();

    if (f->subtype() == FunctionObject::BoundFunction)
        f = static_cast<BoundFunction *>(f)->target();

    Object *v = left->asObject();
    if (!v)
        return Encode(false);

    Object *o = QV4::Value::fromReturnedValue(f->protoProperty()).asObject();
    if (!o)
        return ctx->throwTypeError();

    while (v) {
        v = v->prototype();

        if (!v)
            break;
        else if (o == v)
            return Encode(true);
    }

    return Encode(false);
}

QV4::ReturnedValue Runtime::in(ExecutionContext *ctx, const ValueRef left, const ValueRef right)
{
    if (!right->isObject())
        return ctx->throwTypeError();
    Scope scope(ctx);
    ScopedString s(scope, left->toString(ctx));
    if (scope.hasException())
        return Encode::undefined();
    bool r = right->objectValue()->hasProperty(s.getPointer());
    return Encode(r);
}

double RuntimeHelpers::stringToNumber(const QString &string)
{
    QString s = string.trimmed();
    if (s.startsWith(QLatin1String("0x")) || s.startsWith(QLatin1String("0X")))
        return s.toLong(0, 16);
    bool ok;
    QByteArray ba = s.toLatin1();
    const char *begin = ba.constData();
    const char *end = 0;
    double d = qstrtod(begin, &end, &ok);
    if (end - begin != ba.size()) {
        if (ba == "Infinity" || ba == "+Infinity")
            d = Q_INFINITY;
        else if (ba == "-Infinity")
            d = -Q_INFINITY;
        else
            d = std::numeric_limits<double>::quiet_NaN();
    }
    return d;
}

Returned<String> *RuntimeHelpers::stringFromNumber(ExecutionContext *ctx, double number)
{
    QString qstr;
    RuntimeHelpers::numberToString(&qstr, number, 10);
    return ctx->engine()->newString(qstr);
}

ReturnedValue RuntimeHelpers::objectDefaultValue(Object *object, int typeHint)
{
    if (typeHint == PREFERREDTYPE_HINT) {
        if (object->asDateObject())
            typeHint = STRING_HINT;
        else
            typeHint = NUMBER_HINT;
    }

    ExecutionEngine *engine = object->internalClass()->engine;
    if (engine->hasException)
        return Encode::undefined();

    StringValue *meth1 = &engine->id_toString;
    StringValue *meth2 = &engine->id_valueOf;

    if (typeHint == NUMBER_HINT)
        qSwap(meth1, meth2);

    ExecutionContext *ctx = engine->currentContext();
    Scope scope(ctx);
    ScopedCallData callData(scope, 0);
    callData->thisObject = object;

    ScopedValue conv(scope, object->get(*meth1));
    if (FunctionObject *o = conv->asFunctionObject()) {
        ScopedValue r(scope, o->call(callData));
        if (r->isPrimitive())
            return r->asReturnedValue();
    }

    if (engine->hasException)
        return Encode::undefined();

    conv = object->get(*meth2);
    if (FunctionObject *o = conv->asFunctionObject()) {
        ScopedValue r(scope, o->call(callData));
        if (r->isPrimitive())
            return r->asReturnedValue();
    }

    return ctx->throwTypeError();
}



Returned<Object> *RuntimeHelpers::convertToObject(ExecutionContext *ctx, const ValueRef value)
{
    assert(!value->isObject());
    switch (value->type()) {
    case Value::Undefined_Type:
    case Value::Null_Type:
        ctx->throwTypeError();
        return 0;
    case Value::Boolean_Type:
        return ctx->engine()->newBooleanObject(value);
    case Value::Managed_Type:
        Q_ASSERT(value->isString());
        return ctx->engine()->newStringObject(value);
    case Value::Integer_Type:
    default: // double
        return ctx->engine()->newNumberObject(value);
    }
}

Returned<String> *RuntimeHelpers::convertToString(ExecutionContext *ctx, const ValueRef value)
{
    switch (value->type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
    case Value::Undefined_Type:
        return ctx->engine()->id_undefined.ret();
    case Value::Null_Type:
        return ctx->engine()->id_null.ret();
    case Value::Boolean_Type:
        if (value->booleanValue())
            return ctx->engine()->id_true.ret();
        else
            return ctx->engine()->id_false.ret();
    case Value::Managed_Type:
        if (value->isString())
            return value->stringValue()->asReturned<String>();
        {
            Scope scope(ctx);
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(value, STRING_HINT));
            return RuntimeHelpers::convertToString(ctx, prim);
        }
    case Value::Integer_Type:
        return RuntimeHelpers::stringFromNumber(ctx, value->int_32);
    default: // double
        return RuntimeHelpers::stringFromNumber(ctx, value->doubleValue());
    } // switch
}

// This is slightly different from the method above, as
// the + operator requires a slightly different conversion
static Returned<String> *convert_to_string_add(ExecutionContext *ctx, const ValueRef value)
{
    switch (value->type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
    case Value::Undefined_Type:
        return ctx->engine()->id_undefined.ret();
    case Value::Null_Type:
        return ctx->engine()->id_null.ret();
    case Value::Boolean_Type:
        if (value->booleanValue())
            return ctx->engine()->id_true.ret();
        else
            return ctx->engine()->id_false.ret();
    case Value::Managed_Type:
        if (value->isString())
            return value->stringValue()->asReturned<String>();
        {
            Scope scope(ctx);
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(value, PREFERREDTYPE_HINT));
            return RuntimeHelpers::convertToString(ctx, prim);
        }
    case Value::Integer_Type:
        return RuntimeHelpers::stringFromNumber(ctx, value->int_32);
    default: // double
        return RuntimeHelpers::stringFromNumber(ctx, value->doubleValue());
    } // switch
}

QV4::ReturnedValue RuntimeHelpers::addHelper(ExecutionContext *ctx, const ValueRef left, const ValueRef right)
{
    Scope scope(ctx);

    ScopedValue pleft(scope, RuntimeHelpers::toPrimitive(left, PREFERREDTYPE_HINT));
    ScopedValue pright(scope, RuntimeHelpers::toPrimitive(right, PREFERREDTYPE_HINT));
    if (pleft->isString() || pright->isString()) {
        if (!pleft->isString())
            pleft = convert_to_string_add(ctx, pleft);
        if (!pright->isString())
            pright = convert_to_string_add(ctx, pright);
        if (scope.engine->hasException)
            return Encode::undefined();
        if (!pleft->stringValue()->d()->length())
            return pright->asReturnedValue();
        if (!pright->stringValue()->d()->length())
            return pleft->asReturnedValue();
        return (ctx->engine()->memoryManager->alloc<String>(ctx->d()->engine, pleft->stringValue(), pright->stringValue()))->asReturnedValue();
    }
    double x = RuntimeHelpers::toNumber(pleft);
    double y = RuntimeHelpers::toNumber(pright);
    return Encode(x + y);
}

QV4::ReturnedValue Runtime::addString(QV4::ExecutionContext *ctx, const QV4::ValueRef left, const QV4::ValueRef right)
{
    Q_ASSERT(left->isString() || right->isString());

    if (left->isString() && right->isString()) {
        if (!left->stringValue()->d()->length())
            return right->asReturnedValue();
        if (!right->stringValue()->d()->length())
            return left->asReturnedValue();
        return (ctx->engine()->memoryManager->alloc<String>(ctx->d()->engine, left->stringValue(), right->stringValue()))->asReturnedValue();
    }

    Scope scope(ctx);
    ScopedValue pleft(scope, *left);
    ScopedValue pright(scope, *right);

    if (!pleft->isString())
        pleft = convert_to_string_add(ctx, left);
    if (!pright->isString())
        pright = convert_to_string_add(ctx, right);
    if (scope.engine->hasException)
        return Encode::undefined();
    if (!pleft->stringValue()->d()->length())
        return pright->asReturnedValue();
    if (!pright->stringValue()->d()->length())
        return pleft->asReturnedValue();
    return (ctx->engine()->memoryManager->alloc<String>(ctx->d()->engine, pleft->stringValue(), pright->stringValue()))->asReturnedValue();
}

void Runtime::setProperty(ExecutionContext *ctx, const ValueRef object, String *name, const ValueRef value)
{
    Scope scope(ctx);
    ScopedObject o(scope, object->toObject(ctx));
    if (!o)
        return;
    o->put(name, value);
}

ReturnedValue Runtime::getElement(ExecutionContext *ctx, const ValueRef object, const ValueRef index)
{
    Scope scope(ctx);
    uint idx = index->asArrayIndex();

    Scoped<Object> o(scope, object);
    if (!o) {
        if (idx < UINT_MAX) {
            if (String *str = object->asString()) {
                if (idx >= (uint)str->toQString().length()) {
                    return Encode::undefined();
                }
                const QString s = str->toQString().mid(idx, 1);
                return scope.engine->newString(s)->asReturnedValue();
            }
        }

        if (object->isNullOrUndefined()) {
            QString message = QStringLiteral("Cannot read property '%1' of %2").arg(index->toQStringNoThrow()).arg(object->toQStringNoThrow());
            return ctx->throwTypeError(message);
        }

        o = RuntimeHelpers::convertToObject(ctx, object);
        if (!o) // type error
            return Encode::undefined();
    }

    if (idx < UINT_MAX) {
        if (o->arrayData() && !o->arrayData()->hasAttributes()) {
            ScopedValue v(scope, o->arrayData()->get(idx));
            if (!v->isEmpty())
                return v->asReturnedValue();
        }

        return o->getIndexed(idx);
    }

    ScopedString name(scope, index->toString(ctx));
    if (scope.hasException())
        return Encode::undefined();
    return o->get(name.getPointer());
}

void Runtime::setElement(ExecutionContext *ctx, const ValueRef object, const ValueRef index, const ValueRef value)
{
    Scope scope(ctx);
    ScopedObject o(scope, object->toObject(ctx));
    if (scope.engine->hasException)
        return;

    uint idx = index->asArrayIndex();
    if (idx < UINT_MAX) {
        if (o->arrayType() == ArrayData::Simple) {
            SimpleArrayData *s = static_cast<SimpleArrayData *>(o->arrayData());
            if (s && idx < s->len() && !s->data(idx).isEmpty()) {
                s->data(idx) = value;
                return;
            }
        }
        o->putIndexed(idx, value);
        return;
    }

    ScopedString name(scope, index->toString(ctx));
    o->put(name.getPointer(), value);
}

ReturnedValue Runtime::foreachIterator(ExecutionContext *ctx, const ValueRef in)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, (Object *)0);
    if (!in->isNullOrUndefined())
        o = in->toObject(ctx);
    Scoped<Object> it(scope, ctx->engine()->newForEachIteratorObject(ctx, o));
    return it.asReturnedValue();
}

ReturnedValue Runtime::foreachNextPropertyName(const ValueRef foreach_iterator)
{
    Q_ASSERT(foreach_iterator->isObject());

    ForEachIteratorObject *it = static_cast<ForEachIteratorObject *>(foreach_iterator->objectValue());
    Q_ASSERT(it->as<ForEachIteratorObject>());

    return it->nextPropertyName();
}


void Runtime::setActivationProperty(ExecutionContext *ctx, String *name, const ValueRef value)
{
    ctx->setProperty(name, value);
}

ReturnedValue Runtime::getProperty(ExecutionContext *ctx, const ValueRef object, String *name)
{
    Scope scope(ctx);

    Scoped<Object> o(scope, object);
    if (o)
        return o->get(name);

    if (object->isNullOrUndefined()) {
        QString message = QStringLiteral("Cannot read property '%1' of %2").arg(name->toQString()).arg(object->toQStringNoThrow());
        return ctx->throwTypeError(message);
    }

    o = RuntimeHelpers::convertToObject(ctx, object);
    if (!o) // type error
        return Encode::undefined();
    return o->get(name);
}

ReturnedValue Runtime::getActivationProperty(ExecutionContext *ctx, String *name)
{
    return ctx->getProperty(name);
}

#endif // V4_BOOTSTRAP

uint RuntimeHelpers::equalHelper(const ValueRef x, const ValueRef y)
{
    Q_ASSERT(x->type() != y->type() || (x->isManaged() && (x->isString() != y->isString())));

    if (x->isNumber() && y->isNumber())
        return x->asDouble() == y->asDouble();
    if (x->isNull() && y->isUndefined()) {
        return true;
    } else if (x->isUndefined() && y->isNull()) {
        return true;
    } else if (x->isNumber() && y->isString()) {
        double dy = RuntimeHelpers::toNumber(y);
        return x->asDouble() == dy;
    } else if (x->isString() && y->isNumber()) {
        double dx = RuntimeHelpers::toNumber(x);
        return dx == y->asDouble();
    } else if (x->isBoolean()) {
        return Runtime::compareEqual(Primitive::fromDouble((double) x->booleanValue()), y);
    } else if (y->isBoolean()) {
        return Runtime::compareEqual(x, Primitive::fromDouble((double) y->booleanValue()));
    } else {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        if ((x->isNumber() || x->isString()) && y->isObject()) {
            Scope scope(y->objectValue()->engine());
            ScopedValue py(scope, RuntimeHelpers::toPrimitive(y, PREFERREDTYPE_HINT));
            return Runtime::compareEqual(x, py);
        } else if (x->isObject() && (y->isNumber() || y->isString())) {
            Scope scope(x->objectValue()->engine());
            ScopedValue px(scope, RuntimeHelpers::toPrimitive(x, PREFERREDTYPE_HINT));
            return Runtime::compareEqual(px, y);
        }
#endif
    }

    return false;
}

Bool RuntimeHelpers::strictEqual(const ValueRef x, const ValueRef y)
{
    TRACE2(x, y);

    if (x->rawValue() == y->rawValue())
        // NaN != NaN
        return !x->isNaN();

    if (x->isNumber())
        return y->isNumber() && x->asDouble() == y->asDouble();
    if (x->isManaged())
        return y->isManaged() && x->managed()->isEqualTo(y->managed());
    return false;
}

QV4::Bool Runtime::compareGreaterThan(const QV4::ValueRef l, const QV4::ValueRef r)
{
    TRACE2(l, r);
    if (l->isInteger() && r->isInteger())
        return l->integerValue() > r->integerValue();
    if (l->isNumber() && r->isNumber())
        return l->asDouble() > r->asDouble();
    if (l->isString() && r->isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return r->stringValue()->compare(l->stringValue());
#endif
    }

    if (l->isObject() || r->isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l->isObject() ? l->objectValue() : r->objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::compareGreaterThan(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl > dr;
}

QV4::Bool Runtime::compareLessThan(const QV4::ValueRef l, const QV4::ValueRef r)
{
    TRACE2(l, r);
    if (l->isInteger() && r->isInteger())
        return l->integerValue() < r->integerValue();
    if (l->isNumber() && r->isNumber())
        return l->asDouble() < r->asDouble();
    if (l->isString() && r->isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return l->stringValue()->compare(r->stringValue());
#endif
    }

    if (l->isObject() || r->isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l->isObject() ? l->objectValue() : r->objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::compareLessThan(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl < dr;
}

QV4::Bool Runtime::compareGreaterEqual(const QV4::ValueRef l, const QV4::ValueRef r)
{
    TRACE2(l, r);
    if (l->isInteger() && r->isInteger())
        return l->integerValue() >= r->integerValue();
    if (l->isNumber() && r->isNumber())
        return l->asDouble() >= r->asDouble();
    if (l->isString() && r->isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return !l->stringValue()->compare(r->stringValue());
#endif
    }

    if (l->isObject() || r->isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l->isObject() ? l->objectValue() : r->objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::compareGreaterEqual(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl >= dr;
}

QV4::Bool Runtime::compareLessEqual(const QV4::ValueRef l, const QV4::ValueRef r)
{
    TRACE2(l, r);
    if (l->isInteger() && r->isInteger())
        return l->integerValue() <= r->integerValue();
    if (l->isNumber() && r->isNumber())
        return l->asDouble() <= r->asDouble();
    if (l->isString() && r->isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return !r->stringValue()->compare(l->stringValue());
#endif
    }

    if (l->isObject() || r->isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l->isObject() ? l->objectValue() : r->objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::compareLessEqual(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl <= dr;
}

#ifndef V4_BOOTSTRAP
ReturnedValue Runtime::callGlobalLookup(ExecutionContext *context, uint index, CallData *callData)
{
    Scope scope(context);
    Q_ASSERT(callData->thisObject.isUndefined());

    Lookup *l = context->d()->lookups + index;
    Scoped<FunctionObject> o(scope, l->globalGetter(l, context));
    if (!o)
        return context->throwTypeError();

    if (o.getPointer() == scope.engine->evalFunction && l->name->equals(scope.engine->id_eval))
        return static_cast<EvalFunction *>(o.getPointer())->evalCall(callData, true);

    return o->call(callData);
}


ReturnedValue Runtime::callActivationProperty(ExecutionContext *context, String *name, CallData *callData)
{
    Q_ASSERT(callData->thisObject.isUndefined());
    Scope scope(context);

    ScopedObject base(scope);
    ScopedValue func(scope, context->getPropertyAndBase(name, base.ptr->o));
    if (scope.engine->hasException)
        return Encode::undefined();

    if (base)
        callData->thisObject = base;

    FunctionObject *o = func->asFunctionObject();
    if (!o) {
        QString objectAsString = QStringLiteral("[null]");
        if (base)
            objectAsString = ScopedValue(scope, base.asReturnedValue())->toQStringNoThrow();
        QString msg = QStringLiteral("Property '%1' of object %2 is not a function").arg(name->toQString()).arg(objectAsString);
        return context->throwTypeError(msg);
    }

    if (o == scope.engine->evalFunction && name->equals(scope.engine->id_eval)) {
        return static_cast<EvalFunction *>(o)->evalCall(callData, true);
    }

    return o->call(callData);
}

ReturnedValue Runtime::callProperty(ExecutionContext *context, String *name, CallData *callData)
{
    Scope scope(context);
    Scoped<Object> baseObject(scope, callData->thisObject);
    if (!baseObject) {
        Q_ASSERT(!callData->thisObject.isEmpty());
        if (callData->thisObject.isNullOrUndefined()) {
            QString message = QStringLiteral("Cannot call method '%1' of %2").arg(name->toQString()).arg(callData->thisObject.toQStringNoThrow());
            return context->throwTypeError(message);
        }

        baseObject = RuntimeHelpers::convertToObject(context, ValueRef(&callData->thisObject));
        if (!baseObject) // type error
            return Encode::undefined();
        callData->thisObject = baseObject.asReturnedValue();
    }

    Scoped<FunctionObject> o(scope, baseObject->get(name));
    if (!o) {
        QString error = QStringLiteral("Property '%1' of object %2 is not a function").arg(name->toQString(), callData->thisObject.toQStringNoThrow());
        return context->throwTypeError(error);
    }

    return o->call(callData);
}

ReturnedValue Runtime::callPropertyLookup(ExecutionContext *context, uint index, CallData *callData)
{
    Lookup *l = context->d()->lookups + index;
    Value v;
    v = l->getter(l, callData->thisObject);
    if (!v.isObject())
        return context->throwTypeError();

    return v.objectValue()->call(callData);
}

ReturnedValue Runtime::callElement(ExecutionContext *context, const ValueRef index, CallData *callData)
{
    Scope scope(context);
    ScopedObject baseObject(scope, callData->thisObject.toObject(context));
    ScopedString s(scope, index->toString(context));

    if (scope.engine->hasException)
        return Encode::undefined();
    callData->thisObject = baseObject;

    ScopedObject o(scope, baseObject->get(s.getPointer()));
    if (!o)
        return context->throwTypeError();

    return o->call(callData);
}

ReturnedValue Runtime::callValue(ExecutionContext *context, const ValueRef func, CallData *callData)
{
    if (!func->isObject())
        return context->throwTypeError(QStringLiteral("%1 is not a function").arg(func->toQStringNoThrow()));

    return func->objectValue()->call(callData);
}


ReturnedValue Runtime::constructGlobalLookup(ExecutionContext *context, uint index, CallData *callData)
{
    Scope scope(context);
    Q_ASSERT(callData->thisObject.isUndefined());

    Lookup *l = context->d()->lookups + index;
    Scoped<Object> f(scope, l->globalGetter(l, context));
    if (!f)
        return context->throwTypeError();

    return f->construct(callData);
}


ReturnedValue Runtime::constructActivationProperty(ExecutionContext *context, String *name, CallData *callData)
{
    Scope scope(context);
    ScopedValue func(scope, context->getProperty(name));
    if (scope.engine->hasException)
        return Encode::undefined();

    Object *f = func->asObject();
    if (!f)
        return context->throwTypeError();

    return f->construct(callData);
}

ReturnedValue Runtime::constructValue(ExecutionContext *context, const ValueRef func, CallData *callData)
{
    Object *f = func->asObject();
    if (!f)
        return context->throwTypeError();

    return f->construct(callData);
}

ReturnedValue Runtime::constructProperty(ExecutionContext *context, String *name, CallData *callData)
{
    Scope scope(context);
    ScopedObject thisObject(scope, callData->thisObject.toObject(context));
    if (scope.engine->hasException)
        return Encode::undefined();

    Scoped<Object> f(scope, thisObject->get(name));
    if (!f)
        return context->throwTypeError();

    return f->construct(callData);
}

ReturnedValue Runtime::constructPropertyLookup(ExecutionContext *context, uint index, CallData *callData)
{
    Lookup *l = context->d()->lookups + index;
    Value v;
    v = l->getter(l, callData->thisObject);
    if (!v.isObject())
        return context->throwTypeError();

    return v.objectValue()->construct(callData);
}


void Runtime::throwException(ExecutionContext *context, const ValueRef value)
{
    if (!value->isEmpty())
        context->throwError(value);
}

ReturnedValue Runtime::typeofValue(ExecutionContext *ctx, const ValueRef value)
{
    Scope scope(ctx);
    ScopedString res(scope);
    switch (value->type()) {
    case Value::Undefined_Type:
        res = ctx->engine()->id_undefined;
        break;
    case Value::Null_Type:
        res = ctx->engine()->id_object;
        break;
    case Value::Boolean_Type:
        res = ctx->engine()->id_boolean;
        break;
    case Value::Managed_Type:
        if (value->isString())
            res = ctx->engine()->id_string;
        else if (value->objectValue()->asFunctionObject())
            res = ctx->engine()->id_function;
        else
            res = ctx->engine()->id_object; // ### implementation-defined
        break;
    default:
        res = ctx->engine()->id_number;
        break;
    }
    return res.asReturnedValue();
}

QV4::ReturnedValue Runtime::typeofName(ExecutionContext *context, String *name)
{
    Scope scope(context);
    ScopedValue prop(scope, context->getProperty(name));
    // typeof doesn't throw. clear any possible exception
    scope.engine->hasException = false;
    return Runtime::typeofValue(context, prop);
}

QV4::ReturnedValue Runtime::typeofMember(ExecutionContext *context, const ValueRef base, String *name)
{
    Scope scope(context);
    ScopedObject obj(scope, base->toObject(context));
    if (scope.engine->hasException)
        return Encode::undefined();
    ScopedValue prop(scope, obj->get(name));
    return Runtime::typeofValue(context, prop);
}

QV4::ReturnedValue Runtime::typeofElement(ExecutionContext *context, const ValueRef base, const ValueRef index)
{
    Scope scope(context);
    ScopedString name(scope, index->toString(context));
    ScopedObject obj(scope, base->toObject(context));
    if (scope.engine->hasException)
        return Encode::undefined();
    ScopedValue prop(scope, obj->get(name.getPointer()));
    return Runtime::typeofValue(context, prop);
}

ExecutionContext *Runtime::pushWithScope(const ValueRef o, ExecutionContext *ctx)
{
    Scope scope(ctx);
    ScopedObject obj(scope, o->toObject(ctx));
    return reinterpret_cast<ExecutionContext *>(ctx->newWithContext(obj));
}

ReturnedValue Runtime::unwindException(ExecutionContext *ctx)
{
    if (!ctx->engine()->hasException)
        return Primitive::emptyValue().asReturnedValue();
    return ctx->engine()->catchException(ctx, 0);
}

ExecutionContext *Runtime::pushCatchScope(ExecutionContext *ctx, String *exceptionVarName)
{
    Scope scope(ctx);
    ScopedValue v(scope, ctx->engine()->catchException(ctx, 0));
    return reinterpret_cast<ExecutionContext *>(ctx->newCatchContext(exceptionVarName, v));
}

ExecutionContext *Runtime::popScope(ExecutionContext *ctx)
{
    return ctx->engine()->popContext();
}

void Runtime::declareVar(ExecutionContext *ctx, bool deletable, String *name)
{
    ctx->createMutableBinding(name, deletable);
}

ReturnedValue Runtime::arrayLiteral(ExecutionContext *ctx, Value *values, uint length)
{
    Scope scope(ctx);
    Scoped<ArrayObject> a(scope, ctx->engine()->newArrayObject());

    if (length) {
        a->arrayReserve(length);
        a->arrayPut(0, values, length);
        a->setArrayLengthUnchecked(length);
    }
    return a.asReturnedValue();
}

ReturnedValue Runtime::objectLiteral(QV4::ExecutionContext *ctx, const QV4::Value *args, int classId, int arrayValueCount, int arrayGetterSetterCountAndFlags)
{
    Scope scope(ctx);
    QV4::InternalClass *klass = ctx->d()->compilationUnit->runtimeClasses[classId];
    Scoped<Object> o(scope, ctx->engine()->newObject(klass));

    {
        bool needSparseArray = arrayGetterSetterCountAndFlags >> 30;
        if (needSparseArray)
            o->initSparseArray();
    }

    for (uint i = 0; i < klass->size; ++i)
        o->memberData()[i] = *args++;

    if (arrayValueCount > 0) {
        ScopedValue entry(scope);
        for (int i = 0; i < arrayValueCount; ++i) {
            uint idx = args->toUInt32();
            ++args;
            entry = *args++;
            o->arraySet(idx, entry);
        }
    }

    uint arrayGetterSetterCount = arrayGetterSetterCountAndFlags & ((1 << 30) - 1);
    if (arrayGetterSetterCount > 0) {
        ScopedProperty pd(scope);
        for (uint i = 0; i < arrayGetterSetterCount; ++i) {
            uint idx = args->toUInt32();
            ++args;
            pd->value = *args;
            ++args;
            pd->set = *args;
            ++args;
            o->arraySet(idx, pd, Attr_Accessor);
        }
    }

    return o.asReturnedValue();
}

QV4::ReturnedValue Runtime::setupArgumentsObject(ExecutionContext *ctx)
{
    Q_ASSERT(ctx->d()->type >= ExecutionContext::Type_CallContext);
    CallContext *c = static_cast<CallContext *>(ctx);
    return (c->engine()->memoryManager->alloc<ArgumentsObject>(c))->asReturnedValue();
}

#endif // V4_BOOTSTRAP

QV4::ReturnedValue Runtime::increment(const QV4::ValueRef value)
{
    TRACE1(value);

    if (value->isInteger() && value->integerValue() < INT_MAX)
        return Encode(value->integerValue() + 1);
    else {
        double d = value->toNumber();
        return Encode(d + 1.);
    }
}

QV4::ReturnedValue Runtime::decrement(const QV4::ValueRef value)
{
    TRACE1(value);

    if (value->isInteger() && value->integerValue() > INT_MIN)
        return Encode(value->integerValue() - 1);
    else {
        double d = value->toNumber();
        return Encode(d - 1.);
    }
}

#ifndef V4_BOOTSTRAP

QV4::ReturnedValue RuntimeHelpers::toString(QV4::ExecutionContext *ctx, const QV4::ValueRef value)
{
    if (value->isString())
        return value.asReturnedValue();
    return RuntimeHelpers::convertToString(ctx, value)->asReturnedValue();
}

QV4::ReturnedValue RuntimeHelpers::toObject(QV4::ExecutionContext *ctx, const QV4::ValueRef value)
{
    if (value->isObject())
        return value.asReturnedValue();

    Returned<Object> *o = RuntimeHelpers::convertToObject(ctx, value);
    if (!o) // type error
        return Encode::undefined();

    return Encode(o);
}

#endif // V4_BOOTSTRAP

ReturnedValue Runtime::toDouble(const ValueRef value)
{
    TRACE1(value);
    return Encode(value->toNumber());
}

int Runtime::toInt(const ValueRef value)
{
    TRACE1(value);
    return value->toInt32();
}

int Runtime::doubleToInt(const double &d)
{
    TRACE0();
    return Primitive::toInt32(d);
}

unsigned Runtime::toUInt(const ValueRef value)
{
    TRACE1(value);
    return value->toUInt32();
}

unsigned Runtime::doubleToUInt(const double &d)
{
    TRACE0();
    return Primitive::toUInt32(d);
}

#ifndef V4_BOOTSTRAP

ReturnedValue Runtime::regexpLiteral(ExecutionContext *ctx, int id)
{
    return ctx->d()->compilationUnit->runtimeRegularExpressions[id].asReturnedValue();
}

ReturnedValue Runtime::getQmlIdArray(NoThrowContext *ctx)
{
    Q_ASSERT(ctx->engine()->qmlContextObject()->getPointer()->as<QmlContextWrapper>());
    return static_cast<QmlContextWrapper *>(ctx->engine()->qmlContextObject()->getPointer())->idObjectsArray();
}

ReturnedValue Runtime::getQmlContextObject(NoThrowContext *ctx)
{
    QQmlContextData *context = QmlContextWrapper::callingContext(ctx->engine());
    if (!context)
        return Encode::undefined();
    return QObjectWrapper::wrap(ctx->d()->engine, context->contextObject);
}

ReturnedValue Runtime::getQmlScopeObject(NoThrowContext *ctx)
{
    Scope scope(ctx);
    QV4::Scoped<QmlContextWrapper> c(scope, ctx->engine()->qmlContextObject(), Scoped<QmlContextWrapper>::Cast);
    return QObjectWrapper::wrap(ctx->d()->engine, c->getScopeObject());
}

ReturnedValue Runtime::getQmlQObjectProperty(ExecutionContext *ctx, const ValueRef object, int propertyIndex, bool captureRequired)
{
    Scope scope(ctx);
    QV4::Scoped<QObjectWrapper> wrapper(scope, object);
    if (!wrapper) {
        ctx->throwTypeError(QStringLiteral("Cannot read property of null"));
        return Encode::undefined();
    }
    return QV4::QObjectWrapper::getProperty(wrapper->object(), ctx, propertyIndex, captureRequired);
}

QV4::ReturnedValue Runtime::getQmlAttachedProperty(ExecutionContext *ctx, int attachedPropertiesId, int propertyIndex)
{
    Scope scope(ctx);
    QV4::Scoped<QmlContextWrapper> c(scope, ctx->engine()->qmlContextObject(), Scoped<QmlContextWrapper>::Cast);
    QObject *scopeObject = c->getScopeObject();
    QObject *attachedObject = qmlAttachedPropertiesObjectById(attachedPropertiesId, scopeObject);

    QQmlEngine *qmlEngine = ctx->engine()->v8Engine->engine();
    QQmlData::ensurePropertyCache(qmlEngine, attachedObject);
    return QV4::QObjectWrapper::getProperty(attachedObject, ctx, propertyIndex, /*captureRequired*/true);
}

ReturnedValue Runtime::getQmlSingletonQObjectProperty(ExecutionContext *ctx, const ValueRef object, int propertyIndex, bool captureRequired)
{
    Scope scope(ctx);
    QV4::Scoped<QmlTypeWrapper> wrapper(scope, object);
    if (!wrapper) {
        ctx->throwTypeError(QStringLiteral("Cannot read property of null"));
        return Encode::undefined();
    }
    return QV4::QObjectWrapper::getProperty(wrapper->singletonObject(), ctx, propertyIndex, captureRequired);
}

void Runtime::setQmlQObjectProperty(ExecutionContext *ctx, const ValueRef object, int propertyIndex, const ValueRef value)
{
    Scope scope(ctx);
    QV4::Scoped<QObjectWrapper> wrapper(scope, object);
    if (!wrapper) {
        ctx->throwTypeError(QStringLiteral("Cannot write property of null"));
        return;
    }
    wrapper->setProperty(ctx, propertyIndex, value);
}

ReturnedValue Runtime::getQmlImportedScripts(NoThrowContext *ctx)
{
    QQmlContextData *context = QmlContextWrapper::callingContext(ctx->engine());
    if (!context)
        return Encode::undefined();
    return context->importedScripts.value();
}

QV4::ReturnedValue Runtime::getQmlSingleton(QV4::NoThrowContext *ctx, String *name)
{
    return static_cast<QmlContextWrapper *>(ctx->engine()->qmlContextObject()->getPointer())->qmlSingletonWrapper(ctx->engine()->v8Engine, name);
}

void Runtime::convertThisToObject(ExecutionContext *ctx)
{
    Value *t = &ctx->d()->callData->thisObject;
    if (t->isObject())
        return;
    if (t->isNullOrUndefined()) {
        *t = ctx->engine()->globalObject->asReturnedValue();
    } else {
        *t = t->toObject(ctx)->asReturnedValue();
    }
}

#endif // V4_BOOTSTRAP

} // namespace QV4

QT_END_NAMESPACE
