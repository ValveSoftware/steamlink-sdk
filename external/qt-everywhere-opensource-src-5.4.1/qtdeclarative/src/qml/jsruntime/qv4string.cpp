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

#include "qv4string_p.h"
#include "qv4value_inl_p.h"
#ifndef V4_BOOTSTRAP
#include "qv4identifiertable_p.h"
#include "qv4runtime_p.h"
#include "qv4objectproto_p.h"
#include "qv4stringobject_p.h"
#endif
#include <QtCore/QHash>

using namespace QV4;

static uint toArrayIndex(const QChar *ch, const QChar *end, bool *ok)
{
    *ok = false;
    uint i = ch->unicode() - '0';
    if (i > 9)
        return UINT_MAX;
    ++ch;
    // reject "01", "001", ...
    if (i == 0 && ch != end)
        return UINT_MAX;

    while (ch < end) {
        uint x = ch->unicode() - '0';
        if (x > 9)
            return UINT_MAX;
        uint n = i*10 + x;
        if (n < i)
            // overflow
            return UINT_MAX;
        i = n;
        ++ch;
    }
    *ok = true;
    return i;
}

#ifndef V4_BOOTSTRAP

static uint toArrayIndex(const char *ch, const char *end, bool *ok)
{
    *ok = false;
    uint i = *ch - '0';
    if (i > 9)
        return UINT_MAX;
    ++ch;
    // reject "01", "001", ...
    if (i == 0 && ch != end)
        return UINT_MAX;

    while (ch < end) {
        uint x = *ch - '0';
        if (x > 9)
            return UINT_MAX;
        uint n = i*10 + x;
        if (n < i)
            // overflow
            return UINT_MAX;
        i = n;
        ++ch;
    }
    *ok = true;
    return i;
}


const ObjectVTable String::static_vtbl =
{
    DEFINE_MANAGED_VTABLE_INT(String, 0),
    0,
    0,
    get,
    getIndexed,
    put,
    putIndexed,
    query,
    queryIndexed,
    deleteProperty,
    deleteIndexedProperty,
    0 /*getLookup*/,
    0 /*setLookup*/,
    0,
    0 /*advanceIterator*/,
};

void String::destroy(Managed *that)
{
    static_cast<String*>(that)->d()->~Data();
}

void String::markObjects(Managed *that, ExecutionEngine *e)
{
    String *s = static_cast<String *>(that);
    if (s->d()->largestSubLength) {
        s->d()->left->mark(e);
        s->d()->right->mark(e);
    }
}

ReturnedValue String::get(Managed *m, String *name, bool *hasProperty)
{
    ExecutionEngine *v4 = m->engine();
    Scope scope(v4);
    ScopedString that(scope, static_cast<String *>(m));

    if (name->equals(v4->id_length)) {
        if (hasProperty)
            *hasProperty = true;
        return Primitive::fromInt32(that->d()->text->size).asReturnedValue();
    }
    PropertyAttributes attrs;
    Property *pd = v4->stringObjectClass->prototype->__getPropertyDescriptor__(name, &attrs);
    if (!pd || attrs.isGeneric()) {
        if (hasProperty)
            *hasProperty = false;
        return Primitive::undefinedValue().asReturnedValue();
    }
    if (hasProperty)
        *hasProperty = true;
    return v4->stringObjectClass->prototype->getValue(that, pd, attrs);
}

ReturnedValue String::getIndexed(Managed *m, uint index, bool *hasProperty)
{
    ExecutionEngine *engine = m->engine();
    Scope scope(engine);
    ScopedString that(scope, static_cast<String *>(m));

    if (index < static_cast<uint>(that->d()->text->size)) {
        if (hasProperty)
            *hasProperty = true;
        return Encode(engine->newString(that->toQString().mid(index, 1)));
    }
    PropertyAttributes attrs;
    Property *pd = engine->stringObjectClass->prototype->__getPropertyDescriptor__(index, &attrs);
    if (!pd || attrs.isGeneric()) {
        if (hasProperty)
            *hasProperty = false;
        return Primitive::undefinedValue().asReturnedValue();
    }
    if (hasProperty)
        *hasProperty = true;
    return engine->stringObjectClass->prototype->getValue(that, pd, attrs);
}

void String::put(Managed *m, String *name, const ValueRef value)
{
    Scope scope(m->engine());
    if (scope.hasException())
        return;
    ScopedString that(scope, static_cast<String *>(m));
    Scoped<Object> o(scope, that->engine()->newStringObject(that));
    o->put(name, value);
}

void String::putIndexed(Managed *m, uint index, const ValueRef value)
{
    Scope scope(m->engine());
    if (scope.hasException())
        return;

    ScopedString that(scope, static_cast<String *>(m));
    Scoped<Object> o(scope, that->engine()->newStringObject(that));
    o->putIndexed(index, value);
}

PropertyAttributes String::query(const Managed *m, String *name)
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return queryIndexed(m, idx);
    return Attr_Invalid;
}

PropertyAttributes String::queryIndexed(const Managed *m, uint index)
{
    const String *that = static_cast<const String *>(m);
    return (index < static_cast<uint>(that->d()->text->size)) ? Attr_NotConfigurable|Attr_NotWritable : Attr_Invalid;
}

bool String::deleteProperty(Managed *, String *)
{
    return false;
}

bool String::deleteIndexedProperty(Managed *, uint)
{
    return false;
}

bool String::isEqualTo(Managed *t, Managed *o)
{
    if (t == o)
        return true;

    if (!o->internalClass()->vtable->isString)
        return false;

    String *that = static_cast<String *>(t);
    String *other = static_cast<String *>(o);
    if (that->hashValue() != other->hashValue())
        return false;
    if (that->identifier() && that->identifier() == other->identifier())
        return true;
    if (that->subtype() >= StringType_UInt && that->subtype() == other->subtype())
        return true;

    return that->toQString() == other->toQString();
}


String::Data::Data(ExecutionEngine *engine, const QString &t)
    : Managed::Data(engine->stringClass)
{
    subtype = StringType_Unknown;

    text = const_cast<QString &>(t).data_ptr();
    text->ref.ref();
    identifier = 0;
    stringHash = UINT_MAX;
    largestSubLength = 0;
    len = text->size;
}

String::Data::Data(ExecutionEngine *engine, String *l, String *r)
    : Managed::Data(engine->stringClass)
{
    subtype = StringType_Unknown;

    left = l;
    right = r;
    stringHash = UINT_MAX;
    largestSubLength = qMax(l->d()->largestSubLength, r->d()->largestSubLength);
    len = l->d()->len + r->d()->len;

    if (!l->d()->largestSubLength && l->d()->len > largestSubLength)
        largestSubLength = l->d()->len;
    if (!r->d()->largestSubLength && r->d()->len > largestSubLength)
        largestSubLength = r->d()->len;

    // make sure we don't get excessive depth in our strings
    if (len > 256 && len >= 2*largestSubLength)
        simplifyString();
}

uint String::toUInt(bool *ok) const
{
    *ok = true;

    if (subtype() == StringType_Unknown)
        createHashValue();
    if (subtype() >= StringType_UInt)
        return d()->stringHash;

    // ### this conversion shouldn't be required
    double d = RuntimeHelpers::stringToNumber(toQString());
    uint l = (uint)d;
    if (d == l)
        return l;
    *ok = false;
    return UINT_MAX;
}

bool String::equals(String *other) const
{
    if (this == other)
        return true;
    if (hashValue() != other->hashValue())
        return false;
    if (identifier() && identifier() == other->identifier())
        return true;
    if (subtype() >= StringType_UInt && subtype() == other->subtype())
        return true;

    return toQString() == other->toQString();
}

void String::makeIdentifierImpl() const
{
    if (d()->largestSubLength)
        d()->simplifyString();
    Q_ASSERT(!d()->largestSubLength);
    engine()->identifierTable->identifier(this);
}

void String::Data::simplifyString() const
{
    Q_ASSERT(largestSubLength);

    int l = length();
    QString result(l, Qt::Uninitialized);
    QChar *ch = const_cast<QChar *>(result.constData());
    append(this, ch);
    text = result.data_ptr();
    text->ref.ref();
    identifier = 0;
    largestSubLength = 0;
}

void String::Data::append(const String::Data *data, QChar *ch)
{
    std::vector<const String::Data *> worklist;
    worklist.reserve(32);
    worklist.push_back(data);

    while (!worklist.empty()) {
        const String::Data *item = worklist.back();
        worklist.pop_back();

        if (item->largestSubLength) {
            worklist.push_back(item->right->d());
            worklist.push_back(item->left->d());
        } else {
            memcpy(ch, item->text->data(), item->text->size * sizeof(QChar));
            ch += item->text->size;
        }
    }
}


void String::createHashValue() const
{
    if (d()->largestSubLength)
        d()->simplifyString();
    Q_ASSERT(!d()->largestSubLength);
    const QChar *ch = reinterpret_cast<const QChar *>(d()->text->data());
    const QChar *end = ch + d()->text->size;

    // array indices get their number as hash value
    bool ok;
    d()->stringHash = ::toArrayIndex(ch, end, &ok);
    if (ok) {
        setSubtype((d()->stringHash == UINT_MAX) ? StringType_UInt : StringType_ArrayIndex);
        return;
    }

    uint h = 0xffffffff;
    while (ch < end) {
        h = 31 * h + ch->unicode();
        ++ch;
    }

    d()->stringHash = h;
    setSubtype(StringType_Regular);
}

uint String::createHashValue(const QChar *ch, int length)
{
    const QChar *end = ch + length;

    // array indices get their number as hash value
    bool ok;
    uint stringHash = ::toArrayIndex(ch, end, &ok);
    if (ok)
        return stringHash;

    uint h = 0xffffffff;
    while (ch < end) {
        h = 31 * h + ch->unicode();
        ++ch;
    }

    return h;
}

uint String::createHashValue(const char *ch, int length)
{
    const char *end = ch + length;

    // array indices get their number as hash value
    bool ok;
    uint stringHash = ::toArrayIndex(ch, end, &ok);
    if (ok)
        return stringHash;

    uint h = 0xffffffff;
    while (ch < end) {
        if ((uchar)(*ch) >= 0x80)
            return UINT_MAX;
        h = 31 * h + *ch;
        ++ch;
    }

    return h;
}

uint String::getLength(const Managed *m)
{
    return static_cast<const String *>(m)->d()->length();
}

#endif // V4_BOOTSTRAP

uint String::toArrayIndex(const QString &str)
{
    bool ok;
    return ::toArrayIndex(str.constData(), str.constData() + str.length(), &ok);
}

