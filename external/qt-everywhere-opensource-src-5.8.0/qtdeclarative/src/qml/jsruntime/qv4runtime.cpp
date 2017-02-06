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

#include "qv4global_p.h"
#include "qv4engine_p.h"
#include "qv4runtime_p.h"
#ifndef V4_BOOTSTRAP
#include "qv4object_p.h"
#include "qv4objectproto_p.h"
#include "qv4globalobject_p.h"
#include "qv4stringobject_p.h"
#include "qv4argumentsobject_p.h"
#include "qv4objectiterator_p.h"
#include "qv4dateobject_p.h"
#include "qv4lookup_p.h"
#include "qv4function_p.h"
#include "qv4numberobject_p.h"
#include "private/qlocale_tools_p.h"
#include "qv4scopedvalue_p.h"
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmltypewrapper_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmljavascriptexpression_p.h>
#include "qv4qobjectwrapper_p.h"
#include <private/qv8engine_p.h>
#endif

#include <QtCore/QDebug>
#include <cassert>
#include <cstdio>
#include <stdlib.h>

#include <wtf/MathExtras.h>

#ifdef QV4_COUNT_RUNTIME_FUNCTIONS
#  include <QtCore/QBuffer>
#  include <QtCore/QDebug>
#endif // QV4_COUNT_RUNTIME_FUNCTIONS

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
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QTextStream outs(&buf);
        QList<Line> lines;
        for (auto it = counters.cbegin(), end = counters.cend(); it != end; ++it) {
            const Counters &fCount = it.value();
            for (int i = 0, ei = fCount.size(); i != ei; ++i) {
                quint64 count = fCount[i];
                if (!count)
                    continue;
                Line line;
                line.func = it.key();
                unmangle(i, line.tag1, line.tag2);
                line.count = count;
                lines.append(line);
            }
        }
        std::sort(lines.begin(), lines.end(), Line::less);
        outs << lines.size() << " counters:" << endl;
        foreach (const Line &line, lines)
            outs << qSetFieldWidth(10) << line.count << qSetFieldWidth(0)
                 << " | " << line.func
                 << " | " << pretty(line.tag1)
                 << " | " << pretty(line.tag2)
                 << endl;
        qDebug("%s", buf.data().constData());
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
    } else if (qt_is_inf(num)) {
        *result = num < 0 ? QStringLiteral("-Infinity") : QStringLiteral("Infinity");
        return;
    }

    if (radix == 10) {
        // We cannot use our usual locale->toString(...) here, because EcmaScript has special rules
        // about the longest permissible number, depending on if it's <0 or >0.
        const int ecma_shortest_low = -6;
        const int ecma_shortest_high = 21;

        const QLatin1Char zero('0');
        const QLatin1Char dot('.');

        int decpt = 0;
        int sign = 0;
        *result = qdtoa(num, &decpt, &sign);

        if (decpt <= ecma_shortest_low || decpt > ecma_shortest_high) {
            if (result->length() > 1)
                result->insert(1, dot);
            result->append(QLatin1Char('e'));
            if (decpt > 0)
                result->append(QLatin1Char('+'));
            result->append(QString::number(decpt - 1));
        } else if (decpt <= 0) {
            result->prepend(QLatin1String("0.") + QString(-decpt, zero));
        } else if (decpt < result->length()) {
            result->insert(decpt, dot);
        } else {
            result->append(QString(decpt - result->length(), zero));
        }

        if (sign && num)
            result->prepend(QLatin1Char('-'));

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

ReturnedValue Runtime::method_closure(ExecutionEngine *engine, int functionId)
{
    QV4::Function *clos = engine->current->compilationUnit->runtimeFunctions[functionId];
    Q_ASSERT(clos);
    return FunctionObject::createScriptFunction(engine->currentContext, clos)->asReturnedValue();
}

ReturnedValue Runtime::method_deleteElement(ExecutionEngine *engine, const Value &base, const Value &index)
{
    Scope scope(engine);
    ScopedObject o(scope, base);
    if (o) {
        uint n = index.asArrayIndex();
        if (n < UINT_MAX) {
            return Encode((bool)o->deleteIndexedProperty(n));
        }
    }

    ScopedString name(scope, index.toString(engine));
    return method_deleteMemberString(engine, base, name);
}

ReturnedValue Runtime::method_deleteMember(ExecutionEngine *engine, const Value &base, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    return method_deleteMemberString(engine, base, name);
}

ReturnedValue Runtime::method_deleteMemberString(ExecutionEngine *engine, const Value &base, String *name)
{
    Scope scope(engine);
    ScopedObject obj(scope, base.toObject(engine));
    if (scope.engine->hasException)
        return Encode::undefined();
    return Encode(obj->deleteProperty(name));
}

ReturnedValue Runtime::method_deleteName(ExecutionEngine *engine, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    return Encode(engine->currentContext->deleteProperty(name));
}

QV4::ReturnedValue Runtime::method_instanceof(ExecutionEngine *engine, const Value &left, const Value &right)
{
    Scope scope(engine);
    ScopedFunctionObject f(scope, right.as<FunctionObject>());
    if (!f)
        return engine->throwTypeError();

    if (f->isBoundFunction())
        f = static_cast<BoundFunction *>(f.getPointer())->target();

    ScopedObject v(scope, left.as<Object>());
    if (!v)
        return Encode(false);

    ScopedObject o(scope, f->protoProperty());
    if (!o)
        return engine->throwTypeError();

    while (v) {
        v = v->prototype();

        if (!v)
            break;
        else if (o->d() == v->d())
            return Encode(true);
    }

    return Encode(false);
}

QV4::ReturnedValue Runtime::method_in(ExecutionEngine *engine, const Value &left, const Value &right)
{
    if (!right.isObject())
        return engine->throwTypeError();
    Scope scope(engine);
    ScopedString s(scope, left.toString(engine));
    if (scope.hasException())
        return Encode::undefined();
    bool r = right.objectValue()->hasProperty(s);
    return Encode(r);
}

double RuntimeHelpers::stringToNumber(const QString &string)
{
    const QStringRef s = QStringRef(&string).trimmed();
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

Heap::String *RuntimeHelpers::stringFromNumber(ExecutionEngine *engine, double number)
{
    QString qstr;
    RuntimeHelpers::numberToString(&qstr, number, 10);
    return engine->newString(qstr);
}

ReturnedValue RuntimeHelpers::objectDefaultValue(const Object *object, int typeHint)
{
    if (typeHint == PREFERREDTYPE_HINT) {
        if (object->as<DateObject>())
            typeHint = STRING_HINT;
        else
            typeHint = NUMBER_HINT;
    }

    ExecutionEngine *engine = object->internalClass()->engine;
    if (engine->hasException)
        return Encode::undefined();

    String *meth1 = engine->id_toString();
    String *meth2 = engine->id_valueOf();

    if (typeHint == NUMBER_HINT)
        qSwap(meth1, meth2);

    Scope scope(engine);
    ScopedCallData callData(scope, 0);
    callData->thisObject = *object;

    ScopedValue conv(scope, object->get(meth1));
    if (FunctionObject *o = conv->as<FunctionObject>()) {
        o->call(scope, callData);
        if (scope.result.isPrimitive())
            return scope.result.asReturnedValue();
    }

    if (engine->hasException)
        return Encode::undefined();

    conv = object->get(meth2);
    if (FunctionObject *o = conv->as<FunctionObject>()) {
        o->call(scope, callData);
        if (scope.result.isPrimitive())
            return scope.result.asReturnedValue();
    }

    return engine->throwTypeError();
}



Heap::Object *RuntimeHelpers::convertToObject(ExecutionEngine *engine, const Value &value)
{
    Q_ASSERT(!value.isObject());
    switch (value.type()) {
    case Value::Undefined_Type:
    case Value::Null_Type:
        engine->throwTypeError();
        return 0;
    case Value::Boolean_Type:
        return engine->newBooleanObject(value.booleanValue());
    case Value::Managed_Type:
        Q_ASSERT(value.isString());
        return engine->newStringObject(value.stringValue());
    case Value::Integer_Type:
    default: // double
        return engine->newNumberObject(value.asDouble());
    }
}

Heap::String *RuntimeHelpers::convertToString(ExecutionEngine *engine, const Value &value)
{
    switch (value.type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
    case Value::Undefined_Type:
        return engine->id_undefined()->d();
    case Value::Null_Type:
        return engine->id_null()->d();
    case Value::Boolean_Type:
        if (value.booleanValue())
            return engine->id_true()->d();
        else
            return engine->id_false()->d();
    case Value::Managed_Type:
        if (value.isString())
            return value.stringValue()->d();
        {
            Scope scope(engine);
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(value, STRING_HINT));
            return RuntimeHelpers::convertToString(engine, prim);
        }
    case Value::Integer_Type:
        return RuntimeHelpers::stringFromNumber(engine, value.int_32());
    default: // double
        return RuntimeHelpers::stringFromNumber(engine, value.doubleValue());
    } // switch
}

// This is slightly different from the method above, as
// the + operator requires a slightly different conversion
static Heap::String *convert_to_string_add(ExecutionEngine *engine, const Value &value)
{
    switch (value.type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
    case Value::Undefined_Type:
        return engine->id_undefined()->d();
    case Value::Null_Type:
        return engine->id_null()->d();
    case Value::Boolean_Type:
        if (value.booleanValue())
            return engine->id_true()->d();
        else
            return engine->id_false()->d();
    case Value::Managed_Type:
        if (value.isString())
            return value.stringValue()->d();
        {
            Scope scope(engine);
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(value, PREFERREDTYPE_HINT));
            return RuntimeHelpers::convertToString(engine, prim);
        }
    case Value::Integer_Type:
        return RuntimeHelpers::stringFromNumber(engine, value.int_32());
    default: // double
        return RuntimeHelpers::stringFromNumber(engine, value.doubleValue());
    } // switch
}

QV4::ReturnedValue RuntimeHelpers::addHelper(ExecutionEngine *engine, const Value &left, const Value &right)
{
    Scope scope(engine);

    ScopedValue pleft(scope, RuntimeHelpers::toPrimitive(left, PREFERREDTYPE_HINT));
    ScopedValue pright(scope, RuntimeHelpers::toPrimitive(right, PREFERREDTYPE_HINT));
    if (pleft->isString() || pright->isString()) {
        if (!pleft->isString())
            pleft = convert_to_string_add(engine, pleft);
        if (!pright->isString())
            pright = convert_to_string_add(engine, pright);
        if (scope.engine->hasException)
            return Encode::undefined();
        if (!pleft->stringValue()->d()->length())
            return pright->asReturnedValue();
        if (!pright->stringValue()->d()->length())
            return pleft->asReturnedValue();
        MemoryManager *mm = engine->memoryManager;
        return (mm->alloc<String>(mm, pleft->stringValue()->d(), pright->stringValue()->d()))->asReturnedValue();
    }
    double x = RuntimeHelpers::toNumber(pleft);
    double y = RuntimeHelpers::toNumber(pright);
    return Encode(x + y);
}

QV4::ReturnedValue Runtime::method_addString(ExecutionEngine *engine, const Value &left, const Value &right)
{
    Q_ASSERT(left.isString() || right.isString());

    if (left.isString() && right.isString()) {
        if (!left.stringValue()->d()->length())
            return right.asReturnedValue();
        if (!right.stringValue()->d()->length())
            return left.asReturnedValue();
        MemoryManager *mm = engine->memoryManager;
        return (mm->alloc<String>(mm, left.stringValue()->d(), right.stringValue()->d()))->asReturnedValue();
    }

    Scope scope(engine);
    ScopedValue pleft(scope, left);
    ScopedValue pright(scope, right);

    if (!pleft->isString())
        pleft = convert_to_string_add(engine, left);
    if (!pright->isString())
        pright = convert_to_string_add(engine, right);
    if (scope.engine->hasException)
        return Encode::undefined();
    if (!pleft->stringValue()->d()->length())
        return pright->asReturnedValue();
    if (!pright->stringValue()->d()->length())
        return pleft->asReturnedValue();
    MemoryManager *mm = engine->memoryManager;
    return (mm->alloc<String>(mm, pleft->stringValue()->d(), pright->stringValue()->d()))->asReturnedValue();
}

void Runtime::method_setProperty(ExecutionEngine *engine, const Value &object, int nameIndex, const Value &value)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    ScopedObject o(scope, object.toObject(engine));
    if (!o)
        return;
    o->put(name, value);
}

ReturnedValue Runtime::method_getElement(ExecutionEngine *engine, const Value &object, const Value &index)
{
    Scope scope(engine);
    uint idx = index.asArrayIndex();

    ScopedObject o(scope, object);
    if (!o) {
        if (idx < UINT_MAX) {
            if (const String *str = object.as<String>()) {
                if (idx >= (uint)str->toQString().length()) {
                    return Encode::undefined();
                }
                const QString s = str->toQString().mid(idx, 1);
                return scope.engine->newString(s)->asReturnedValue();
            }
        }

        if (object.isNullOrUndefined()) {
            QString message = QStringLiteral("Cannot read property '%1' of %2").arg(index.toQStringNoThrow()).arg(object.toQStringNoThrow());
            return engine->throwTypeError(message);
        }

        o = RuntimeHelpers::convertToObject(scope.engine, object);
        if (!o) // type error
            return Encode::undefined();
    }

    if (idx < UINT_MAX) {
        if (o->arrayData() && !o->arrayData()->attrs) {
            ScopedValue v(scope, o->arrayData()->get(idx));
            if (!v->isEmpty())
                return v->asReturnedValue();
        }

        return o->getIndexed(idx);
    }

    ScopedString name(scope, index.toString(engine));
    if (scope.hasException())
        return Encode::undefined();
    return o->get(name);
}

void Runtime::method_setElement(ExecutionEngine *engine, const Value &object, const Value &index, const Value &value)
{
    Scope scope(engine);
    ScopedObject o(scope, object.toObject(engine));
    if (scope.engine->hasException)
        return;

    uint idx = index.asArrayIndex();
    if (idx < UINT_MAX) {
        if (o->arrayType() == Heap::ArrayData::Simple) {
            Heap::SimpleArrayData *s = static_cast<Heap::SimpleArrayData *>(o->arrayData());
            if (s && idx < s->len && !s->data(idx).isEmpty()) {
                s->data(idx) = value;
                return;
            }
        }
        o->putIndexed(idx, value);
        return;
    }

    ScopedString name(scope, index.toString(engine));
    o->put(name, value);
}

ReturnedValue Runtime::method_foreachIterator(ExecutionEngine *engine, const Value &in)
{
    Scope scope(engine);
    ScopedObject o(scope, (Object *)0);
    if (!in.isNullOrUndefined())
        o = in.toObject(engine);
    return engine->newForEachIteratorObject(o)->asReturnedValue();
}

ReturnedValue Runtime::method_foreachNextPropertyName(const Value &foreach_iterator)
{
    Q_ASSERT(foreach_iterator.isObject());

    ForEachIteratorObject *it = static_cast<ForEachIteratorObject *>(foreach_iterator.objectValue());
    Q_ASSERT(it->as<ForEachIteratorObject>());

    return it->nextPropertyName();
}


void Runtime::method_setActivationProperty(ExecutionEngine *engine, int nameIndex, const Value &value)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    engine->currentContext->setProperty(name, value);
}

ReturnedValue Runtime::method_getProperty(ExecutionEngine *engine, const Value &object, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);

    ScopedObject o(scope, object);
    if (o)
        return o->get(name);

    if (object.isNullOrUndefined()) {
        QString message = QStringLiteral("Cannot read property '%1' of %2").arg(name->toQString()).arg(object.toQStringNoThrow());
        return engine->throwTypeError(message);
    }

    o = RuntimeHelpers::convertToObject(scope.engine, object);
    if (!o) // type error
        return Encode::undefined();
    return o->get(name);
}

ReturnedValue Runtime::method_getActivationProperty(ExecutionEngine *engine, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    return engine->currentContext->getProperty(name);
}

#endif // V4_BOOTSTRAP

uint RuntimeHelpers::equalHelper(const Value &x, const Value &y)
{
    Q_ASSERT(x.type() != y.type() || (x.isManaged() && (x.isString() != y.isString())));

    if (x.isNumber() && y.isNumber())
        return x.asDouble() == y.asDouble();
    if (x.isNull() && y.isUndefined()) {
        return true;
    } else if (x.isUndefined() && y.isNull()) {
        return true;
    } else if (x.isNumber() && y.isString()) {
        double dy = RuntimeHelpers::toNumber(y);
        return x.asDouble() == dy;
    } else if (x.isString() && y.isNumber()) {
        double dx = RuntimeHelpers::toNumber(x);
        return dx == y.asDouble();
    } else if (x.isBoolean()) {
        return Runtime::method_compareEqual(Primitive::fromDouble((double) x.booleanValue()), y);
    } else if (y.isBoolean()) {
        return Runtime::method_compareEqual(x, Primitive::fromDouble((double) y.booleanValue()));
    } else {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        if ((x.isNumber() || x.isString()) && y.isObject()) {
            Scope scope(y.objectValue()->engine());
            ScopedValue py(scope, RuntimeHelpers::toPrimitive(y, PREFERREDTYPE_HINT));
            return Runtime::method_compareEqual(x, py);
        } else if (x.isObject() && (y.isNumber() || y.isString())) {
            Scope scope(x.objectValue()->engine());
            ScopedValue px(scope, RuntimeHelpers::toPrimitive(x, PREFERREDTYPE_HINT));
            return Runtime::method_compareEqual(px, y);
        }
#endif
    }

    return false;
}

Bool RuntimeHelpers::strictEqual(const Value &x, const Value &y)
{
    TRACE2(x, y);

    if (x.rawValue() == y.rawValue())
        // NaN != NaN
        return !x.isNaN();

    if (x.isNumber())
        return y.isNumber() && x.asDouble() == y.asDouble();
    if (x.isManaged())
        return y.isManaged() && x.cast<Managed>()->isEqualTo(y.cast<Managed>());
    return false;
}

QV4::Bool Runtime::method_compareGreaterThan(const Value &l, const Value &r)
{
    TRACE2(l, r);
    if (l.isInteger() && r.isInteger())
        return l.integerValue() > r.integerValue();
    if (l.isNumber() && r.isNumber())
        return l.asDouble() > r.asDouble();
    if (l.isString() && r.isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return r.stringValue()->compare(l.stringValue());
#endif
    }

    if (l.isObject() || r.isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l.isObject() ? l.objectValue() : r.objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::method_compareGreaterThan(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl > dr;
}

QV4::Bool Runtime::method_compareLessThan(const Value &l, const Value &r)
{
    TRACE2(l, r);
    if (l.isInteger() && r.isInteger())
        return l.integerValue() < r.integerValue();
    if (l.isNumber() && r.isNumber())
        return l.asDouble() < r.asDouble();
    if (l.isString() && r.isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return l.stringValue()->compare(r.stringValue());
#endif
    }

    if (l.isObject() || r.isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l.isObject() ? l.objectValue() : r.objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::method_compareLessThan(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl < dr;
}

QV4::Bool Runtime::method_compareGreaterEqual(const Value &l, const Value &r)
{
    TRACE2(l, r);
    if (l.isInteger() && r.isInteger())
        return l.integerValue() >= r.integerValue();
    if (l.isNumber() && r.isNumber())
        return l.asDouble() >= r.asDouble();
    if (l.isString() && r.isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return !l.stringValue()->compare(r.stringValue());
#endif
    }

    if (l.isObject() || r.isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l.isObject() ? l.objectValue() : r.objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::method_compareGreaterEqual(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl >= dr;
}

QV4::Bool Runtime::method_compareLessEqual(const Value &l, const Value &r)
{
    TRACE2(l, r);
    if (l.isInteger() && r.isInteger())
        return l.integerValue() <= r.integerValue();
    if (l.isNumber() && r.isNumber())
        return l.asDouble() <= r.asDouble();
    if (l.isString() && r.isString()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
        return false;
#else
        return !r.stringValue()->compare(l.stringValue());
#endif
    }

    if (l.isObject() || r.isObject()) {
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        QV4::ExecutionEngine *e = (l.isObject() ? l.objectValue() : r.objectValue())->engine();
        QV4::Scope scope(e);
        QV4::ScopedValue pl(scope, RuntimeHelpers::toPrimitive(l, QV4::NUMBER_HINT));
        QV4::ScopedValue pr(scope, RuntimeHelpers::toPrimitive(r, QV4::NUMBER_HINT));
        return Runtime::method_compareLessEqual(pl, pr);
#endif
    }

    double dl = RuntimeHelpers::toNumber(l);
    double dr = RuntimeHelpers::toNumber(r);
    return dl <= dr;
}

#ifndef V4_BOOTSTRAP
Bool Runtime::method_compareInstanceof(ExecutionEngine *engine, const Value &left, const Value &right)
{
    TRACE2(left, right);

    Scope scope(engine);
    ScopedValue v(scope, method_instanceof(engine, left, right));
    return v->booleanValue();
}

uint Runtime::method_compareIn(ExecutionEngine *engine, const Value &left, const Value &right)
{
    TRACE2(left, right);

    Scope scope(engine);
    ScopedValue v(scope, method_in(engine, left, right));
    return v->booleanValue();
}


ReturnedValue Runtime::method_callGlobalLookup(ExecutionEngine *engine, uint index, CallData *callData)
{
    Scope scope(engine);
    Q_ASSERT(callData->thisObject.isUndefined());

    Lookup *l = engine->current->lookups + index;
    ScopedFunctionObject o(scope, l->globalGetter(l, engine));
    if (!o)
        return engine->throwTypeError();

    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[l->nameIndex]);
    if (o->d() == scope.engine->evalFunction()->d() && name->equals(scope.engine->id_eval())) {
        static_cast<EvalFunction *>(o.getPointer())->evalCall(scope, callData, true);
    } else {
        o->call(scope, callData);
    }

    return scope.result.asReturnedValue();
}


ReturnedValue Runtime::method_callActivationProperty(ExecutionEngine *engine, int nameIndex, CallData *callData)
{
    Q_ASSERT(callData->thisObject.isUndefined());
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);

    ScopedObject base(scope);
    ScopedValue func(scope, engine->currentContext->getPropertyAndBase(name, base.getRef()));
    if (scope.engine->hasException)
        return Encode::undefined();

    if (base)
        callData->thisObject = base;

    FunctionObject *o = func->as<FunctionObject>();
    if (!o) {
        QString objectAsString = QStringLiteral("[null]");
        if (base)
            objectAsString = ScopedValue(scope, base.asReturnedValue())->toQStringNoThrow();
        QString msg = QStringLiteral("Property '%1' of object %2 is not a function").arg(name->toQString()).arg(objectAsString);
        return engine->throwTypeError(msg);
    }

    if (o->d() == scope.engine->evalFunction()->d() && name->equals(scope.engine->id_eval())) {
        static_cast<EvalFunction *>(o)->evalCall(scope, callData, true);
    } else {
        o->call(scope, callData);
    }

    return scope.result.asReturnedValue();
}

ReturnedValue Runtime::method_callQmlScopeObjectProperty(ExecutionEngine *engine, int propertyIndex, CallData *callData)
{
    Scope scope(engine);
    ScopedFunctionObject o(scope, method_getQmlScopeObjectProperty(engine, callData->thisObject, propertyIndex, /*captureRequired*/true));
    if (!o) {
        QString error = QStringLiteral("Property '%1' of scope object is not a function").arg(propertyIndex);
        return engine->throwTypeError(error);
    }

    o->call(scope, callData);
    return scope.result.asReturnedValue();
}

ReturnedValue Runtime::method_callQmlContextObjectProperty(ExecutionEngine *engine, int propertyIndex, CallData *callData)
{
    Scope scope(engine);
    ScopedFunctionObject o(scope, method_getQmlContextObjectProperty(engine, callData->thisObject, propertyIndex, /*captureRequired*/true));
    if (!o) {
        QString error = QStringLiteral("Property '%1' of context object is not a function").arg(propertyIndex);
        return engine->throwTypeError(error);
    }

    o->call(scope, callData);
    return scope.result.asReturnedValue();
}

ReturnedValue Runtime::method_callProperty(ExecutionEngine *engine, int nameIndex, CallData *callData)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    ScopedObject baseObject(scope, callData->thisObject);
    if (!baseObject) {
        Q_ASSERT(!callData->thisObject.isEmpty());
        if (callData->thisObject.isNullOrUndefined()) {
            QString message = QStringLiteral("Cannot call method '%1' of %2").arg(name->toQString()).arg(callData->thisObject.toQStringNoThrow());
            return engine->throwTypeError(message);
        }

        baseObject = RuntimeHelpers::convertToObject(scope.engine, callData->thisObject);
        if (!baseObject) // type error
            return Encode::undefined();
        callData->thisObject = baseObject.asReturnedValue();
    }

    ScopedFunctionObject o(scope, baseObject->get(name));
    if (o) {
        o->call(scope, callData);
        return scope.result.asReturnedValue();
    } else {
        QString error = QStringLiteral("Property '%1' of object %2 is not a function").arg(name->toQString(), callData->thisObject.toQStringNoThrow());
        return engine->throwTypeError(error);
    }

}

ReturnedValue Runtime::method_callPropertyLookup(ExecutionEngine *engine, uint index, CallData *callData)
{
    Lookup *l = engine->current->lookups + index;
    Value v;
    v = l->getter(l, engine, callData->thisObject);
    if (v.isObject()) {
        Scope scope(engine);
        v.objectValue()->call(scope, callData);
        return scope.result.asReturnedValue();
    } else {
        return engine->throwTypeError();
    }
}

ReturnedValue Runtime::method_callElement(ExecutionEngine *engine, const Value &index, CallData *callData)
{
    Scope scope(engine);
    ScopedObject baseObject(scope, callData->thisObject.toObject(engine));
    ScopedString s(scope, index.toString(engine));

    if (scope.engine->hasException)
        return Encode::undefined();
    callData->thisObject = baseObject;

    ScopedObject o(scope, baseObject->get(s));
    if (!o)
        return engine->throwTypeError();

    o->call(scope, callData);
    return scope.result.asReturnedValue();
}

ReturnedValue Runtime::method_callValue(ExecutionEngine *engine, const Value &func, CallData *callData)
{
    if (!func.isObject())
        return engine->throwTypeError(QStringLiteral("%1 is not a function").arg(func.toQStringNoThrow()));

    Scope scope(engine);
    func.objectValue()->call(scope, callData);
    return scope.result.asReturnedValue();
}


ReturnedValue Runtime::method_constructGlobalLookup(ExecutionEngine *engine, uint index, CallData *callData)
{
    Scope scope(engine);
    Q_ASSERT(callData->thisObject.isUndefined());

    Lookup *l = engine->current->lookups + index;
    ScopedObject f(scope, l->globalGetter(l, engine));
    if (f) {
        f->construct(scope, callData);
        return scope.result.asReturnedValue();
    } else {
        return engine->throwTypeError();
    }
}


ReturnedValue Runtime::method_constructActivationProperty(ExecutionEngine *engine, int nameIndex, CallData *callData)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    ScopedValue func(scope, engine->currentContext->getProperty(name));
    if (scope.engine->hasException)
        return Encode::undefined();

    Object *f = func->as<Object>();
    if (!f)
        return engine->throwTypeError();

    f->construct(scope, callData);
    return scope.result.asReturnedValue();
}

ReturnedValue Runtime::method_constructValue(ExecutionEngine *engine, const Value &func, CallData *callData)
{
    const Object *f = func.as<Object>();
    if (!f)
        return engine->throwTypeError();

    Scope scope(engine);
    f->construct(scope, callData);
    return scope.result.asReturnedValue();
}

ReturnedValue Runtime::method_constructProperty(ExecutionEngine *engine, int nameIndex, CallData *callData)
{
    Scope scope(engine);
    ScopedObject thisObject(scope, callData->thisObject.toObject(engine));
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedObject f(scope, thisObject->get(name));
    if (f) {
        Scope scope(engine);
        f->construct(scope, callData);
        return scope.result.asReturnedValue();
    } else {
        return engine->throwTypeError();
    }
}

ReturnedValue Runtime::method_constructPropertyLookup(ExecutionEngine *engine, uint index, CallData *callData)
{
    Lookup *l = engine->current->lookups + index;
    Value v;
    v = l->getter(l, engine, callData->thisObject);
    if (v.isObject()) {
        Scope scope(engine);
        ScopedValue result(scope);
        v.objectValue()->construct(scope, callData);
        return scope.result.asReturnedValue();
    } else {
        return engine->throwTypeError();
    }
}


void Runtime::method_throwException(ExecutionEngine *engine, const Value &value)
{
    if (!value.isEmpty())
        engine->throwError(value);
}

ReturnedValue Runtime::method_typeofValue(ExecutionEngine *engine, const Value &value)
{
    Scope scope(engine);
    ScopedString res(scope);
    switch (value.type()) {
    case Value::Undefined_Type:
        res = engine->id_undefined();
        break;
    case Value::Null_Type:
        res = engine->id_object();
        break;
    case Value::Boolean_Type:
        res = engine->id_boolean();
        break;
    case Value::Managed_Type:
        if (value.isString())
            res = engine->id_string();
        else if (value.objectValue()->as<FunctionObject>())
            res = engine->id_function();
        else
            res = engine->id_object(); // ### implementation-defined
        break;
    default:
        res = engine->id_number();
        break;
    }
    return res.asReturnedValue();
}

QV4::ReturnedValue Runtime::method_typeofName(ExecutionEngine *engine, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    ScopedValue prop(scope, engine->currentContext->getProperty(name));
    // typeof doesn't throw. clear any possible exception
    scope.engine->hasException = false;
    return method_typeofValue(engine, prop);
}

#ifndef V4_BOOTSTRAP
ReturnedValue Runtime::method_typeofScopeObjectProperty(ExecutionEngine *engine, const Value &context, int propertyIndex)
{
    Scope scope(engine);
    ScopedValue prop(scope, method_getQmlScopeObjectProperty(engine, context, propertyIndex, /*captureRequired*/true));
    if (scope.engine->hasException)
        return Encode::undefined();
    return method_typeofValue(engine, prop);
}

ReturnedValue Runtime::method_typeofContextObjectProperty(ExecutionEngine *engine, const Value &context, int propertyIndex)
{
    Scope scope(engine);
    ScopedValue prop(scope, method_getQmlContextObjectProperty(engine, context, propertyIndex, /*captureRequired*/true));
    if (scope.engine->hasException)
        return Encode::undefined();
    return method_typeofValue(engine, prop);
}
#endif // V4_BOOTSTRAP

QV4::ReturnedValue Runtime::method_typeofMember(ExecutionEngine *engine, const Value &base, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    ScopedObject obj(scope, base.toObject(engine));
    if (scope.engine->hasException)
        return Encode::undefined();
    ScopedValue prop(scope, obj->get(name));
    return method_typeofValue(engine, prop);
}

QV4::ReturnedValue Runtime::method_typeofElement(ExecutionEngine *engine, const Value &base, const Value &index)
{
    Scope scope(engine);
    ScopedString name(scope, index.toString(engine));
    ScopedObject obj(scope, base.toObject(engine));
    if (scope.engine->hasException)
        return Encode::undefined();
    ScopedValue prop(scope, obj->get(name));
    return method_typeofValue(engine, prop);
}

ReturnedValue Runtime::method_unwindException(ExecutionEngine *engine)
{
    if (!engine->hasException)
        return Primitive::emptyValue().asReturnedValue();
    return engine->catchException(0);
}

/* The next three methods are a bit tricky. They can't open up a Scope, as that
 * would mess up the pushing of the context.
 *
 * Instead the push/pop pair acts as a non local scope.
 */
void Runtime::method_pushWithScope(const Value &o, NoThrowEngine *engine)
{
    engine->pushContext(engine->currentContext->newWithContext(o.toObject(engine)));
    Q_ASSERT(engine->jsStackTop == engine->currentContext + 2);
}

void Runtime::method_pushCatchScope(NoThrowEngine *engine, int exceptionVarNameIndex)
{
    ExecutionContext *c = engine->currentContext;
    engine->pushContext(c->newCatchContext(c->d()->compilationUnit->runtimeStrings[exceptionVarNameIndex], engine->catchException(0)));
    Q_ASSERT(engine->jsStackTop == engine->currentContext + 2);
}

void Runtime::method_popScope(NoThrowEngine *engine)
{
    Q_ASSERT(engine->jsStackTop == engine->currentContext + 2);
    engine->popContext();
    engine->jsStackTop -= 2;
}

void Runtime::method_declareVar(ExecutionEngine *engine, bool deletable, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    engine->currentContext->createMutableBinding(name, deletable);
}

ReturnedValue Runtime::method_arrayLiteral(ExecutionEngine *engine, Value *values, uint length)
{
    return engine->newArrayObject(values, length)->asReturnedValue();
}

ReturnedValue Runtime::method_objectLiteral(ExecutionEngine *engine, const QV4::Value *args, int classId, int arrayValueCount, int arrayGetterSetterCountAndFlags)
{
    Scope scope(engine);
    QV4::InternalClass *klass = engine->current->compilationUnit->runtimeClasses[classId];
    ScopedObject o(scope, engine->newObject(klass, engine->objectPrototype()));

    {
        bool needSparseArray = arrayGetterSetterCountAndFlags >> 30;
        if (needSparseArray)
            o->initSparseArray();
    }

    for (uint i = 0; i < klass->size; ++i)
        *o->propertyData(i) = *args++;

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

QV4::ReturnedValue Runtime::method_setupArgumentsObject(ExecutionEngine *engine)
{
    Q_ASSERT(engine->current->type == Heap::ExecutionContext::Type_CallContext);
    QV4::CallContext *c = static_cast<QV4::CallContext *>(engine->currentContext);
    QV4::InternalClass *ic = c->d()->strictMode ? engine->strictArgumentsObjectClass : engine->argumentsObjectClass;
    return engine->memoryManager->allocObject<ArgumentsObject>(ic, engine->objectPrototype(), c)->asReturnedValue();
}

#endif // V4_BOOTSTRAP

QV4::ReturnedValue Runtime::method_increment(const Value &value)
{
    TRACE1(value);

    if (value.isInteger() && value.integerValue() < INT_MAX)
        return Encode(value.integerValue() + 1);
    else {
        double d = value.toNumber();
        return Encode(d + 1.);
    }
}

QV4::ReturnedValue Runtime::method_decrement(const Value &value)
{
    TRACE1(value);

    if (value.isInteger() && value.integerValue() > INT_MIN)
        return Encode(value.integerValue() - 1);
    else {
        double d = value.toNumber();
        return Encode(d - 1.);
    }
}

#ifndef V4_BOOTSTRAP

QV4::ReturnedValue RuntimeHelpers::toString(ExecutionEngine *engine, const Value &value)
{
    if (value.isString())
        return value.asReturnedValue();
    return RuntimeHelpers::convertToString(engine, value)->asReturnedValue();
}

QV4::ReturnedValue RuntimeHelpers::toObject(ExecutionEngine *engine, const Value &value)
{
    if (value.isObject())
        return value.asReturnedValue();

    Heap::Object *o = RuntimeHelpers::convertToObject(engine, value);
    if (!o) // type error
        return Encode::undefined();

    return Encode(o);
}

#endif // V4_BOOTSTRAP

ReturnedValue Runtime::method_toDouble(const Value &value)
{
    TRACE1(value);
    return Encode(value.toNumber());
}

int Runtime::method_toInt(const Value &value)
{
    TRACE1(value);
    return value.toInt32();
}

int Runtime::method_doubleToInt(const double &d)
{
    TRACE0();
    return Primitive::toInt32(d);
}

unsigned Runtime::method_toUInt(const Value &value)
{
    TRACE1(value);
    return value.toUInt32();
}

unsigned Runtime::method_doubleToUInt(const double &d)
{
    TRACE0();
    return Primitive::toUInt32(d);
}

#ifndef V4_BOOTSTRAP

ReturnedValue Runtime::method_getQmlContext(NoThrowEngine *engine)
{
    return engine->qmlContext()->asReturnedValue();
}

ReturnedValue Runtime::method_regexpLiteral(ExecutionEngine *engine, int id)
{
    return engine->current->compilationUnit->runtimeRegularExpressions[id].asReturnedValue();
}

ReturnedValue Runtime::method_getQmlQObjectProperty(ExecutionEngine *engine, const Value &object, int propertyIndex, bool captureRequired)
{
    Scope scope(engine);
    QV4::Scoped<QObjectWrapper> wrapper(scope, object);
    if (!wrapper) {
        engine->throwTypeError(QStringLiteral("Cannot read property of null"));
        return Encode::undefined();
    }
    return QV4::QObjectWrapper::getProperty(scope.engine, wrapper->object(), propertyIndex, captureRequired);
}

QV4::ReturnedValue Runtime::method_getQmlAttachedProperty(ExecutionEngine *engine, int attachedPropertiesId, int propertyIndex)
{
    QObject *scopeObject = engine->qmlScopeObject();
    QObject *attachedObject = qmlAttachedPropertiesObjectById(attachedPropertiesId, scopeObject);

    QJSEngine *jsEngine = engine->jsEngine();
    QQmlData::ensurePropertyCache(jsEngine, attachedObject);
    return QV4::QObjectWrapper::getProperty(engine, attachedObject, propertyIndex, /*captureRequired*/true);
}

ReturnedValue Runtime::method_getQmlScopeObjectProperty(ExecutionEngine *engine, const Value &context, int propertyIndex, bool captureRequired)
{
    const QmlContext &c = static_cast<const QmlContext &>(context);
    return QV4::QObjectWrapper::getProperty(engine, c.d()->qml->scopeObject, propertyIndex, captureRequired);
}

ReturnedValue Runtime::method_getQmlContextObjectProperty(ExecutionEngine *engine, const Value &context, int propertyIndex, bool captureRequired)
{
    const QmlContext &c = static_cast<const QmlContext &>(context);
    return QV4::QObjectWrapper::getProperty(engine, (*c.d()->qml->context)->contextObject, propertyIndex, captureRequired);
}

ReturnedValue Runtime::method_getQmlSingletonQObjectProperty(ExecutionEngine *engine, const Value &object, int propertyIndex, bool captureRequired)
{
    Scope scope(engine);
    QV4::Scoped<QmlTypeWrapper> wrapper(scope, object);
    if (!wrapper) {
        scope.engine->throwTypeError(QStringLiteral("Cannot read property of null"));
        return Encode::undefined();
    }
    return QV4::QObjectWrapper::getProperty(scope.engine, wrapper->singletonObject(), propertyIndex, captureRequired);
}

ReturnedValue Runtime::method_getQmlIdObject(ExecutionEngine *engine, const Value &c, uint index)
{
    Scope scope(engine);
    const QmlContext &qmlContext = static_cast<const QmlContext &>(c);
    QQmlContextData *context = *qmlContext.d()->qml->context;
    if (!context || index >= (uint)context->idValueCount)
        return Encode::undefined();

    QQmlEnginePrivate *ep = engine->qmlEngine() ? QQmlEnginePrivate::get(engine->qmlEngine()) : 0;
    if (ep && ep->propertyCapture)
        ep->propertyCapture->captureProperty(&context->idValues[index].bindings);

    return QObjectWrapper::wrap(engine, context->idValues[index].data());
}

void Runtime::method_setQmlScopeObjectProperty(ExecutionEngine *engine, const Value &context, int propertyIndex, const Value &value)
{
    const QmlContext &c = static_cast<const QmlContext &>(context);
    return QV4::QObjectWrapper::setProperty(engine, c.d()->qml->scopeObject, propertyIndex, value);
}

void Runtime::method_setQmlContextObjectProperty(ExecutionEngine *engine, const Value &context, int propertyIndex, const Value &value)
{
    const QmlContext &c = static_cast<const QmlContext &>(context);
    return QV4::QObjectWrapper::setProperty(engine, (*c.d()->qml->context)->contextObject, propertyIndex, value);
}

void Runtime::method_setQmlQObjectProperty(ExecutionEngine *engine, const Value &object, int propertyIndex, const Value &value)
{
    Scope scope(engine);
    QV4::Scoped<QObjectWrapper> wrapper(scope, object);
    if (!wrapper) {
        engine->throwTypeError(QStringLiteral("Cannot write property of null"));
        return;
    }
    wrapper->setProperty(engine, propertyIndex, value);
}

ReturnedValue Runtime::method_getQmlImportedScripts(NoThrowEngine *engine)
{
    QQmlContextData *context = engine->callingQmlContext();
    if (!context)
        return Encode::undefined();
    return context->importedScripts.value();
}

QV4::ReturnedValue Runtime::method_getQmlSingleton(QV4::NoThrowEngine *engine, int nameIndex)
{
    Scope scope(engine);
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[nameIndex]);
    return engine->qmlSingletonWrapper(name);
}

void Runtime::method_convertThisToObject(ExecutionEngine *engine)
{
    Value *t = &engine->current->callData->thisObject;
    if (t->isObject())
        return;
    if (t->isNullOrUndefined()) {
        *t = engine->globalObject->asReturnedValue();
    } else {
        *t = t->toObject(engine)->asReturnedValue();
    }
}

ReturnedValue Runtime::method_uPlus(const Value &value)
{
    TRACE1(value);

    if (value.isNumber())
        return value.asReturnedValue();
    if (value.integerCompatible())
        return Encode(value.int_32());

    double n = value.toNumberImpl();
    return Encode(n);
}

ReturnedValue Runtime::method_uMinus(const Value &value)
{
    TRACE1(value);

    // +0 != -0, so we need to convert to double when negating 0
    if (value.isInteger() && value.integerValue())
        return Encode(-value.integerValue());
    else {
        double n = RuntimeHelpers::toNumber(value);
        return Encode(-n);
    }
}

ReturnedValue Runtime::method_complement(const Value &value)
{
    TRACE1(value);

    int n = value.toInt32();
    return Encode((int)~n);
}

ReturnedValue Runtime::method_uNot(const Value &value)
{
    TRACE1(value);

    bool b = value.toBoolean();
    return Encode(!b);
}

// binary operators
ReturnedValue Runtime::method_bitOr(const Value &left, const Value &right)
{
    TRACE2(left, right);

    int lval = left.toInt32();
    int rval = right.toInt32();
    return Encode(lval | rval);
}

ReturnedValue Runtime::method_bitXor(const Value &left, const Value &right)
{
    TRACE2(left, right);

    int lval = left.toInt32();
    int rval = right.toInt32();
    return Encode(lval ^ rval);
}

ReturnedValue Runtime::method_bitAnd(const Value &left, const Value &right)
{
    TRACE2(left, right);

    int lval = left.toInt32();
    int rval = right.toInt32();
    return Encode(lval & rval);
}

#ifndef V4_BOOTSTRAP
ReturnedValue Runtime::method_add(ExecutionEngine *engine, const Value &left, const Value &right)
{
    TRACE2(left, right);

    if (Q_LIKELY(left.isInteger() && right.isInteger()))
        return add_int32(left.integerValue(), right.integerValue());
    if (left.isNumber() && right.isNumber())
        return Primitive::fromDouble(left.asDouble() + right.asDouble()).asReturnedValue();

    return RuntimeHelpers::addHelper(engine, left, right);
}
#endif // V4_BOOTSTRAP

ReturnedValue Runtime::method_sub(const Value &left, const Value &right)
{
    TRACE2(left, right);

    if (Q_LIKELY(left.isInteger() && right.isInteger()))
        return sub_int32(left.integerValue(), right.integerValue());

    double lval = left.isNumber() ? left.asDouble() : left.toNumberImpl();
    double rval = right.isNumber() ? right.asDouble() : right.toNumberImpl();

    return Primitive::fromDouble(lval - rval).asReturnedValue();
}

ReturnedValue Runtime::method_mul(const Value &left, const Value &right)
{
    TRACE2(left, right);

    if (Q_LIKELY(left.isInteger() && right.isInteger()))
        return mul_int32(left.integerValue(), right.integerValue());

    double lval = left.isNumber() ? left.asDouble() : left.toNumberImpl();
    double rval = right.isNumber() ? right.asDouble() : right.toNumberImpl();

    return Primitive::fromDouble(lval * rval).asReturnedValue();
}

ReturnedValue Runtime::method_div(const Value &left, const Value &right)
{
    TRACE2(left, right);

    if (Value::integerCompatible(left, right)) {
        int lval = left.integerValue();
        int rval = right.integerValue();
        if (rval != 0 && (lval % rval == 0))
            return Encode(int(lval / rval));
        else
            return Encode(double(lval) / rval);
    }

    double lval = left.toNumber();
    double rval = right.toNumber();
    return Primitive::fromDouble(lval / rval).asReturnedValue();
}

ReturnedValue Runtime::method_mod(const Value &left, const Value &right)
{
    TRACE2(left, right);

    if (Value::integerCompatible(left, right) && right.integerValue() != 0) {
        int intRes = left.integerValue() % right.integerValue();
        if (intRes != 0 || left.integerValue() >= 0)
            return Encode(intRes);
    }

    double lval = RuntimeHelpers::toNumber(left);
    double rval = RuntimeHelpers::toNumber(right);
#ifdef fmod
#  undef fmod
#endif
    return Primitive::fromDouble(std::fmod(lval, rval)).asReturnedValue();
}

ReturnedValue Runtime::method_shl(const Value &left, const Value &right)
{
    TRACE2(left, right);

    int lval = left.toInt32();
    int rval = right.toInt32() & 0x1f;
    return Encode((int)(lval << rval));
}

ReturnedValue Runtime::method_shr(const Value &left, const Value &right)
{
    TRACE2(left, right);

    int lval = left.toInt32();
    unsigned rval = right.toUInt32() & 0x1f;
    return Encode((int)(lval >> rval));
}

ReturnedValue Runtime::method_ushr(const Value &left, const Value &right)
{
    TRACE2(left, right);

    unsigned lval = left.toUInt32();
    unsigned rval = right.toUInt32() & 0x1f;
    uint res = lval >> rval;

    return Encode(res);
}

#endif // V4_BOOTSTRAP

ReturnedValue Runtime::method_greaterThan(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = method_compareGreaterThan(left, right);
    return Encode(r);
}

ReturnedValue Runtime::method_lessThan(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = method_compareLessThan(left, right);
    return Encode(r);
}

ReturnedValue Runtime::method_greaterEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = method_compareGreaterEqual(left, right);
    return Encode(r);
}

ReturnedValue Runtime::method_lessEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = method_compareLessEqual(left, right);
    return Encode(r);
}

Bool Runtime::method_compareEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    if (left.rawValue() == right.rawValue())
        // NaN != NaN
        return !left.isNaN();

    if (left.type() == right.type()) {
        if (!left.isManaged())
            return false;
        if (left.isString() == right.isString())
            return left.cast<Managed>()->isEqualTo(right.cast<Managed>());
    }

    return RuntimeHelpers::equalHelper(left, right);
}

ReturnedValue Runtime::method_equal(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = method_compareEqual(left, right);
    return Encode(r);
}

ReturnedValue Runtime::method_notEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = !method_compareEqual(left, right);
    return Encode(r);
}

ReturnedValue Runtime::method_strictEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = RuntimeHelpers::strictEqual(left, right);
    return Encode(r);
}

ReturnedValue Runtime::method_strictNotEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    bool r = ! RuntimeHelpers::strictEqual(left, right);
    return Encode(r);
}

Bool Runtime::method_compareNotEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    return !Runtime::method_compareEqual(left, right);
}

Bool Runtime::method_compareStrictEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    return RuntimeHelpers::strictEqual(left, right);
}

Bool Runtime::method_compareStrictNotEqual(const Value &left, const Value &right)
{
    TRACE2(left, right);

    return ! RuntimeHelpers::strictEqual(left, right);
}

Bool Runtime::method_toBoolean(const Value &value)
{
    return value.toBoolean();
}

} // namespace QV4

QT_END_NAMESPACE
