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

#include "qv4object_p.h"
#include "qv4jsir_p.h"
#include "qv4isel_p.h"
#include "qv4objectproto_p.h"
#include "qv4stringobject_p.h"
#include "qv4argumentsobject_p.h"
#include <private/qv4mm_p.h>
#include "qv4lookup_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4memberdata_p.h"
#include "qv4objectiterator_p.h"
#include "qv4identifier_p.h"
#include "qv4string_p.h"
#include "qv4identifiertable_p.h"

#include <stdint.h>

using namespace QV4;

DEFINE_OBJECT_VTABLE(Object);

void Object::setInternalClass(InternalClass *ic)
{
    d()->internalClass = ic;
    ensureMemberData();
}

void Object::getProperty(uint index, Property *p, PropertyAttributes *attrs) const
{
    p->value = *propertyData(index);
    *attrs = internalClass()->propertyData.at(index);
    if (attrs->isAccessor())
        p->set = *propertyData(index + SetterOffset);
}

void Object::setProperty(uint index, const Property *p)
{
    *propertyData(index) = p->value;
    if (internalClass()->propertyData.at(index).isAccessor())
        *propertyData(index + SetterOffset) = p->set;
}

bool Object::setPrototype(Object *proto)
{
    Heap::Object *pp = proto ? proto->d() : 0;
    while (pp) {
        if (pp == d())
            return false;
        pp = pp->prototype;
    }
    d()->prototype = proto ? proto->d() : 0;
    return true;
}

void Object::put(ExecutionEngine *engine, const QString &name, const Value &value)
{
    Scope scope(engine);
    ScopedString n(scope, engine->newString(name));
    put(n, value);
}

ReturnedValue Object::getValue(const Value &thisObject, const Value &v, PropertyAttributes attrs)
{
    if (!attrs.isAccessor())
        return v.asReturnedValue();
    const QV4::FunctionObject *f = v.as<FunctionObject>();
    if (!f)
        return Encode::undefined();

    Scope scope(f->engine());
    ScopedCallData callData(scope);
    callData->thisObject = thisObject;
    f->call(scope, callData);
    return scope.result.asReturnedValue();
}

void Object::putValue(uint memberIndex, const Value &value)
{
    QV4::InternalClass *ic = internalClass();
    if (ic->engine->hasException)
        return;

    PropertyAttributes attrs = ic->propertyData[memberIndex];

    if (attrs.isAccessor()) {
        FunctionObject *set = propertyData(memberIndex + SetterOffset)->as<FunctionObject>();
        if (set) {
            Scope scope(ic->engine);
            ScopedFunctionObject setter(scope, set);
            ScopedCallData callData(scope, 1);
            callData->args[0] = value;
            callData->thisObject = this;
            setter->call(scope, callData);
            return;
        }
        goto reject;
    }

    if (!attrs.isWritable())
        goto reject;

    *propertyData(memberIndex) = value;
    return;

  reject:
    if (engine()->current->strictMode)
        engine()->throwTypeError();
}

void Object::defineDefaultProperty(const QString &name, const Value &value)
{
    ExecutionEngine *e = engine();
    Scope scope(e);
    ScopedString s(scope, e->newIdentifier(name));
    defineDefaultProperty(s, value);
}

void Object::defineDefaultProperty(const QString &name, ReturnedValue (*code)(CallContext *), int argumentCount)
{
    ExecutionEngine *e = engine();
    Scope scope(e);
    ScopedString s(scope, e->newIdentifier(name));
    ExecutionContext *global = e->rootContext();
    ScopedFunctionObject function(scope, BuiltinFunction::create(global, s, code));
    function->defineReadonlyProperty(e->id_length(), Primitive::fromInt32(argumentCount));
    defineDefaultProperty(s, function);
}

void Object::defineDefaultProperty(String *name, ReturnedValue (*code)(CallContext *), int argumentCount)
{
    ExecutionEngine *e = engine();
    Scope scope(e);
    ExecutionContext *global = e->rootContext();
    ScopedFunctionObject function(scope, BuiltinFunction::create(global, name, code));
    function->defineReadonlyProperty(e->id_length(), Primitive::fromInt32(argumentCount));
    defineDefaultProperty(name, function);
}

void Object::defineAccessorProperty(const QString &name, ReturnedValue (*getter)(CallContext *), ReturnedValue (*setter)(CallContext *))
{
    ExecutionEngine *e = engine();
    Scope scope(e);
    ScopedString s(scope, e->newIdentifier(name));
    defineAccessorProperty(s, getter, setter);
}

void Object::defineAccessorProperty(String *name, ReturnedValue (*getter)(CallContext *), ReturnedValue (*setter)(CallContext *))
{
    ExecutionEngine *v4 = engine();
    QV4::Scope scope(v4);
    ScopedProperty p(scope);
    ExecutionContext *global = v4->rootContext();
    p->setGetter(ScopedFunctionObject(scope, (getter ? BuiltinFunction::create(global, name, getter) : 0)));
    p->setSetter(ScopedFunctionObject(scope, (setter ? BuiltinFunction::create(global, name, setter) : 0)));
    insertMember(name, p, QV4::Attr_Accessor|QV4::Attr_NotConfigurable|QV4::Attr_NotEnumerable);
}

void Object::defineReadonlyProperty(const QString &name, const Value &value)
{
    QV4::ExecutionEngine *e = engine();
    Scope scope(e);
    ScopedString s(scope, e->newIdentifier(name));
    defineReadonlyProperty(s, value);
}

void Object::defineReadonlyProperty(String *name, const Value &value)
{
    insertMember(name, value, Attr_ReadOnly);
}

void Object::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    Heap::Object *o = static_cast<Heap::Object *>(that);

    if (o->inlineMemberSize) {
        Value *v = o->propertyData(0);
        for (uint i = 0; i < o->inlineMemberSize; ++i)
            v[i].mark(e);
    }

    if (o->memberData)
        o->memberData->mark(e);
    if (o->arrayData)
        o->arrayData->mark(e);
    if (o->prototype)
        o->prototype->mark(e);
}

void Object::ensureMemberData()
{
    QV4::InternalClass *ic = internalClass();
    if (ic->size > d()->inlineMemberSize)
        d()->memberData = MemberData::reallocate(ic->engine, d()->memberData, ic->size - d()->inlineMemberSize);
}

void Object::insertMember(String *s, const Property *p, PropertyAttributes attributes)
{
    uint idx;
    InternalClass::addMember(this, s, attributes, &idx);

    if (attributes.isAccessor()) {
        *propertyData(idx + GetterOffset) = p->value;
        *propertyData(idx + SetterOffset) = p->set;
    } else {
        *propertyData(idx) = p->value;
    }
}

// Section 8.12.1
void Object::getOwnProperty(String *name, PropertyAttributes *attrs, Property *p)
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return getOwnProperty(idx, attrs, p);

    uint member = internalClass()->find(name);
    if (member < UINT_MAX) {
        *attrs = internalClass()->propertyData[member];
        if (p) {
            p->value = *propertyData(member);
            if (attrs->isAccessor())
                p->set = *propertyData(member + SetterOffset);
        }
        return;
    }

    if (attrs)
        *attrs = Attr_Invalid;
    return;
}

void Object::getOwnProperty(uint index, PropertyAttributes *attrs, Property *p)
{
    Property *pd = arrayData() ? arrayData()->getProperty(index) : 0;
    if (pd) {
        *attrs = arrayData()->attributes(index);
        if (p)
            p->copy(pd, *attrs);
        return;
    }
    if (isStringObject()) {
        *attrs = Attr_NotConfigurable|Attr_NotWritable;
        if (p)
            p->value = static_cast<StringObject *>(this)->getIndex(index);
        return;
    }

    if (attrs)
        *attrs = Attr_Invalid;
    return;
}

// Section 8.12.2
Value *Object::getValueOrSetter(String *name, PropertyAttributes *attrs)
{
    Q_ASSERT(name->asArrayIndex() == UINT_MAX);

    Heap::Object *o = d();
    while (o) {
        uint idx = o->internalClass->find(name);
        if (idx < UINT_MAX) {
            *attrs = o->internalClass->propertyData[idx];
            return o->propertyData(attrs->isAccessor() ? idx + SetterOffset : idx);
        }

        o = o->prototype;
    }
    *attrs = Attr_Invalid;
    return 0;
}

Value *Object::getValueOrSetter(uint index, PropertyAttributes *attrs)
{
    Heap::Object *o = d();
    while (o) {
        Property *p = o->arrayData ? o->arrayData->getProperty(index) : 0;
        if (p) {
            *attrs = o->arrayData->attributes(index);
            return attrs->isAccessor() ? &p->set : &p->value;
        }
        if (o->vtable()->type == Type_StringObject) {
            if (index < static_cast<const Heap::StringObject *>(o)->length()) {
                // this is an evil hack, but it works, as the method is only ever called from putIndexed,
                // where we don't use the returned pointer there for non writable attributes
                *attrs = (Attr_NotWritable|Attr_NotConfigurable);
                return reinterpret_cast<Value *>(0x1);
            }
        }
        o = o->prototype;
    }
    *attrs = Attr_Invalid;
    return 0;
}

bool Object::hasProperty(String *name) const
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return hasProperty(idx);

    Scope scope(engine());
    ScopedObject o(scope, d());
    while (o) {
        if (o->hasOwnProperty(name))
            return true;

        o = o->prototype();
    }

    return false;
}

bool Object::hasProperty(uint index) const
{
    Scope scope(engine());
    ScopedObject o(scope, d());
    while (o) {
        if (o->hasOwnProperty(index))
                return true;

        o = o->prototype();
    }

    return false;
}

bool Object::hasOwnProperty(String *name) const
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return hasOwnProperty(idx);

    if (internalClass()->find(name) < UINT_MAX)
        return true;
    if (!query(name).isEmpty())
        return true;
    return false;
}

bool Object::hasOwnProperty(uint index) const
{
    if (arrayData() && !arrayData()->isEmpty(index))
        return true;

    if (isStringObject()) {
        if (index < static_cast<const StringObject *>(this)->length())
            return true;
    }
    if (!queryIndexed(index).isEmpty())
        return true;
    return false;
}

void Object::construct(const Managed *m, Scope &scope, CallData *)
{
    scope.result = static_cast<const Object *>(m)->engine()->throwTypeError();
}

void Object::call(const Managed *m, Scope &scope, CallData *)
{
    scope.result = static_cast<const Object *>(m)->engine()->throwTypeError();
}

ReturnedValue Object::get(const Managed *m, String *name, bool *hasProperty)
{
    return static_cast<const Object *>(m)->internalGet(name, hasProperty);
}

ReturnedValue Object::getIndexed(const Managed *m, uint index, bool *hasProperty)
{
    return static_cast<const Object *>(m)->internalGetIndexed(index, hasProperty);
}

void Object::put(Managed *m, String *name, const Value &value)
{
    static_cast<Object *>(m)->internalPut(name, value);
}

void Object::putIndexed(Managed *m, uint index, const Value &value)
{
    static_cast<Object *>(m)->internalPutIndexed(index, value);
}

PropertyAttributes Object::query(const Managed *m, String *name)
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return queryIndexed(m, idx);

    const Object *o = static_cast<const Object *>(m);
    idx = o->internalClass()->find(name);
    if (idx < UINT_MAX)
        return o->internalClass()->propertyData[idx];

    return Attr_Invalid;
}

PropertyAttributes Object::queryIndexed(const Managed *m, uint index)
{
    const Object *o = static_cast<const Object *>(m);
    if (o->arrayData() && !o->arrayData()->isEmpty(index))
        return o->arrayData()->attributes(index);

    if (o->isStringObject()) {
        if (index < static_cast<const StringObject *>(o)->length())
            return (Attr_NotWritable|Attr_NotConfigurable);
    }
    return Attr_Invalid;
}

bool Object::deleteProperty(Managed *m, String *name)
{
    return static_cast<Object *>(m)->internalDeleteProperty(name);
}

bool Object::deleteIndexedProperty(Managed *m, uint index)
{
    return static_cast<Object *>(m)->internalDeleteIndexedProperty(index);
}

ReturnedValue Object::getLookup(const Managed *m, Lookup *l)
{
    const Object *o = static_cast<const Object *>(m);
    PropertyAttributes attrs;
    ReturnedValue v = l->lookup(o, &attrs);
    if (v != Primitive::emptyValue().asReturnedValue()) {
        if (attrs.isData()) {
            if (l->level == 0)
                l->getter = Lookup::getter0;
            else if (l->level == 1)
                l->getter = Lookup::getter1;
            else if (l->level == 2)
                l->getter = Lookup::getter2;
            else
                l->getter = Lookup::getterFallback;
            return v;
        } else {
            if (l->level == 0)
                l->getter = Lookup::getterAccessor0;
            else if (l->level == 1)
                l->getter = Lookup::getterAccessor1;
            else if (l->level == 2)
                l->getter = Lookup::getterAccessor2;
            else
                l->getter = Lookup::getterFallback;
            return v;
        }
    }
    return Encode::undefined();
}

void Object::setLookup(Managed *m, Lookup *l, const Value &value)
{
    Scope scope(static_cast<Object *>(m)->engine());
    ScopedObject o(scope, static_cast<Object *>(m));
    ScopedString name(scope, scope.engine->current->compilationUnit->runtimeStrings[l->nameIndex]);

    InternalClass *c = o->internalClass();
    uint idx = c->find(name);
    if (!o->isArrayObject() || idx != Heap::ArrayObject::LengthPropertyIndex) {
        if (idx != UINT_MAX && o->internalClass()->propertyData[idx].isData() && o->internalClass()->propertyData[idx].isWritable()) {
            l->classList[0] = o->internalClass();
            l->index = idx;
            l->setter = Lookup::setter0;
            *o->propertyData(idx) = value;
            return;
        }

        if (idx != UINT_MAX) {
            o->putValue(idx, value);
            return;
        }
    }

    o->put(name, value);

    if (o->internalClass() == c)
        return;
    idx = o->internalClass()->find(name);
    if (idx == UINT_MAX)
        return;
    l->classList[0] = c;
    l->classList[3] = o->internalClass();
    l->index = idx;
    if (!o->prototype()) {
        l->setter = Lookup::setterInsert0;
        return;
    }
    o = o->prototype();
    l->classList[1] = o->internalClass();
    if (!o->prototype()) {
        l->setter = Lookup::setterInsert1;
        return;
    }
    o = o->prototype();
    l->classList[2] = o->internalClass();
    if (!o->prototype()) {
        l->setter = Lookup::setterInsert2;
        return;
    }
    l->setter = Lookup::setterGeneric;
}

void Object::advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *pd, PropertyAttributes *attrs)
{
    Object *o = static_cast<Object *>(m);
    name->setM(0);
    *index = UINT_MAX;

    if (o->arrayData()) {
        if (!it->arrayIndex)
            it->arrayNode = o->sparseBegin();

        // sparse arrays
        if (it->arrayNode) {
            while (it->arrayNode != o->sparseEnd()) {
                int k = it->arrayNode->key();
                uint pidx = it->arrayNode->value;
                Heap::SparseArrayData *sa = o->d()->arrayData.cast<Heap::SparseArrayData>();
                Property *p = reinterpret_cast<Property *>(sa->arrayData + pidx);
                it->arrayNode = it->arrayNode->nextNode();
                PropertyAttributes a = sa->attrs ? sa->attrs[pidx] : Attr_Data;
                if (!(it->flags & ObjectIterator::EnumerableOnly) || a.isEnumerable()) {
                    it->arrayIndex = k + 1;
                    *index = k;
                    *attrs = a;
                    pd->copy(p, a);
                    return;
                }
            }
            it->arrayNode = 0;
            it->arrayIndex = UINT_MAX;
        }
        // dense arrays
        while (it->arrayIndex < o->d()->arrayData->len) {
            Heap::SimpleArrayData *sa = o->d()->arrayData.cast<Heap::SimpleArrayData>();
            Value &val = sa->data(it->arrayIndex);
            PropertyAttributes a = o->arrayData()->attributes(it->arrayIndex);
            ++it->arrayIndex;
            if (!val.isEmpty()
                && (!(it->flags & ObjectIterator::EnumerableOnly) || a.isEnumerable())) {
                *index = it->arrayIndex - 1;
                *attrs = a;
                pd->value = val;
                return;
            }
        }
    }

    while (it->memberIndex < o->internalClass()->size) {
        Identifier *n = o->internalClass()->nameMap.at(it->memberIndex);
        if (!n) {
            // accessor properties have a dummy entry with n == 0
            ++it->memberIndex;
            continue;
        }

        int idx = it->memberIndex;
        PropertyAttributes a = o->internalClass()->propertyData[it->memberIndex];
        ++it->memberIndex;
        if (!(it->flags & ObjectIterator::EnumerableOnly) || a.isEnumerable()) {
            name->setM(o->engine()->identifierTable->stringFromIdentifier(n));
            *attrs = a;
            pd->value = *o->propertyData(idx);
            if (a.isAccessor())
                pd->set = *o->propertyData(idx + SetterOffset);
            return;
        }
    }

    *attrs = PropertyAttributes();
}

// Section 8.12.3
ReturnedValue Object::internalGet(String *name, bool *hasProperty) const
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return getIndexed(idx, hasProperty);

    Scope scope(engine());
    name->makeIdentifier(scope.engine);

    ScopedObject o(scope, this);
    while (o) {
        uint idx = o->internalClass()->find(name);
        if (idx < UINT_MAX) {
            if (hasProperty)
                *hasProperty = true;
            return getValue(*o->propertyData(idx), o->internalClass()->propertyData.at(idx));
        }

        o = o->prototype();
    }

    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

ReturnedValue Object::internalGetIndexed(uint index, bool *hasProperty) const
{
    Property *pd = 0;
    PropertyAttributes attrs;
    Scope scope(engine());
    ScopedObject o(scope, this);
    while (o) {
        Property *p = o->arrayData() ? o->arrayData()->getProperty(index) : 0;
        if (p) {
            pd = p;
            attrs = o->arrayData()->attributes(index);
            break;
        }
        if (o->isStringObject()) {
            ScopedString str(scope, static_cast<StringObject *>(o.getPointer())->getIndex(index));
            if (str) {
                attrs = (Attr_NotWritable|Attr_NotConfigurable);
                if (hasProperty)
                    *hasProperty = true;
                return str.asReturnedValue();
            }
        }
        o = o->prototype();
    }

    if (pd) {
        if (hasProperty)
            *hasProperty = true;
        return getValue(pd->value, attrs);
    }

    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}


// Section 8.12.5
void Object::internalPut(String *name, const Value &value)
{
    if (internalClass()->engine->hasException)
        return;

    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return putIndexed(idx, value);

    name->makeIdentifier(engine());

    uint member = internalClass()->find(name);
    Value *v = 0;
    PropertyAttributes attrs;
    if (member < UINT_MAX) {
        attrs = internalClass()->propertyData[member];
        v = propertyData(attrs.isAccessor() ? member + SetterOffset : member);
    }

    // clause 1
    if (v) {
        if (attrs.isAccessor()) {
            if (v->as<FunctionObject>())
                goto cont;
            goto reject;
        } else if (!attrs.isWritable())
            goto reject;
        else if (isArrayObject() && name->equals(engine()->id_length())) {
            bool ok;
            uint l = value.asArrayLength(&ok);
            if (!ok) {
                engine()->throwRangeError(value);
                return;
            }
            ok = setArrayLength(l);
            if (!ok)
                goto reject;
        } else {
            *v = value;
        }
        return;
    } else if (!prototype()) {
        if (!isExtensible())
            goto reject;
    } else {
        // clause 4
        Scope scope(engine());
        if ((v = ScopedObject(scope, prototype())->getValueOrSetter(name, &attrs))) {
            if (attrs.isAccessor()) {
                if (!v->as<FunctionObject>())
                    goto reject;
            } else if (!isExtensible() || !attrs.isWritable()) {
                goto reject;
            }
        } else if (!isExtensible()) {
            goto reject;
        }
    }

    cont:

    // Clause 5
    if (v && attrs.isAccessor()) {
        Q_ASSERT(v->as<FunctionObject>());

        Scope scope(engine());
        ScopedFunctionObject setter(scope, *v);
        ScopedCallData callData(scope, 1);
        callData->args[0] = value;
        callData->thisObject = this;
        setter->call(scope, callData);
        return;
    }

    insertMember(name, value);
    return;

  reject:
    if (engine()->current->strictMode) {
        QString message = QLatin1String("Cannot assign to read-only property \"") +
                name->toQString() + QLatin1Char('\"');
        engine()->throwTypeError(message);
    }
}

void Object::internalPutIndexed(uint index, const Value &value)
{
    if (internalClass()->engine->hasException)
        return;

    PropertyAttributes attrs;

    Value *v = arrayData() ? arrayData()->getValueOrSetter(index, &attrs) : 0;

    if (!v && isStringObject()) {
        if (index < static_cast<StringObject *>(this)->length())
            // not writable
            goto reject;
    }

    // clause 1
    if (v) {
        if (attrs.isAccessor()) {
            if (v->as<FunctionObject>())
                goto cont;
            goto reject;
        } else if (!attrs.isWritable())
            goto reject;
        else
            *v = value;
        return;
    } else if (!prototype()) {
        if (!isExtensible())
            goto reject;
    } else {
        // clause 4
        Scope scope(engine());
        if ((v = ScopedObject(scope, prototype())->getValueOrSetter(index, &attrs))) {
            if (attrs.isAccessor()) {
                if (!v->as<FunctionObject>())
                    goto reject;
            } else if (!isExtensible() || !attrs.isWritable()) {
                goto reject;
            }
        } else if (!isExtensible()) {
            goto reject;
        }
    }

    cont:

    // Clause 5
    if (v && attrs.isAccessor()) {
        Q_ASSERT(v->as<FunctionObject>());

        Scope scope(engine());
        ScopedFunctionObject setter(scope, *v);
        ScopedCallData callData(scope, 1);
        callData->args[0] = value;
        callData->thisObject = this;
        setter->call(scope, callData);
        return;
    }

    arraySet(index, value);
    return;

  reject:
    if (engine()->current->strictMode)
        engine()->throwTypeError();
}

// Section 8.12.7
bool Object::internalDeleteProperty(String *name)
{
    if (internalClass()->engine->hasException)
        return false;

    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return deleteIndexedProperty(idx);

    name->makeIdentifier(engine());

    uint memberIdx = internalClass()->find(name);
    if (memberIdx != UINT_MAX) {
        if (internalClass()->propertyData[memberIdx].isConfigurable()) {
            InternalClass::removeMember(this, name->identifier());
            return true;
        }
        if (engine()->current->strictMode)
            engine()->throwTypeError();
        return false;
    }

    return true;
}

bool Object::internalDeleteIndexedProperty(uint index)
{
    Scope scope(engine());
    if (scope.engine->hasException)
        return false;

    Scoped<ArrayData> ad(scope, arrayData());
    if (!ad || ad->vtable()->del(this, index))
        return true;

    if (engine()->current->strictMode)
        engine()->throwTypeError();
    return false;
}

// Section 8.12.9
bool Object::__defineOwnProperty__(ExecutionEngine *engine, String *name, const Property *p, PropertyAttributes attrs)
{
    uint idx = name->asArrayIndex();
    if (idx != UINT_MAX)
        return __defineOwnProperty__(engine, idx, p, attrs);

    Scope scope(engine);
    name->makeIdentifier(scope.engine);

    uint memberIndex;

    if (isArrayObject() && name->equals(engine->id_length())) {
        Q_ASSERT(Heap::ArrayObject::LengthPropertyIndex == internalClass()->find(engine->id_length()));
        ScopedProperty lp(scope);
        PropertyAttributes cattrs;
        getProperty(Heap::ArrayObject::LengthPropertyIndex, lp, &cattrs);
        if (attrs.isEmpty() || p->isSubset(attrs, lp, cattrs))
            return true;
        if (!cattrs.isWritable() || attrs.type() == PropertyAttributes::Accessor || attrs.isConfigurable() || attrs.isEnumerable())
            goto reject;
        bool succeeded = true;
        if (attrs.type() == PropertyAttributes::Data) {
            bool ok;
            uint l = p->value.asArrayLength(&ok);
            if (!ok) {
                ScopedValue v(scope, p->value);
                engine->throwRangeError(v);
                return false;
            }
            succeeded = setArrayLength(l);
        }
        if (attrs.hasWritable() && !attrs.isWritable()) {
            cattrs.setWritable(false);
            InternalClass::changeMember(this, engine->id_length(), cattrs);
        }
        if (!succeeded)
            goto reject;
        return true;
    }

    // Clause 1
    memberIndex = internalClass()->find(name);

    if (memberIndex == UINT_MAX) {
        // clause 3
        if (!isExtensible())
            goto reject;
        // clause 4
        ScopedProperty pd(scope);
        pd->copy(p, attrs);
        pd->fullyPopulated(&attrs);
        insertMember(name, pd, attrs);
        return true;
    }

    return __defineOwnProperty__(engine, memberIndex, name, p, attrs);
reject:
  if (engine->current->strictMode)
      engine->throwTypeError();
  return false;
}

bool Object::__defineOwnProperty__(ExecutionEngine *engine, uint index, const Property *p, PropertyAttributes attrs)
{
    // 15.4.5.1, 4b
    if (isArrayObject() && index >= getLength() && !internalClass()->propertyData[Heap::ArrayObject::LengthPropertyIndex].isWritable())
        goto reject;

    if (ArgumentsObject::isNonStrictArgumentsObject(this))
        return static_cast<ArgumentsObject *>(this)->defineOwnProperty(engine, index, p, attrs);

    return defineOwnProperty2(engine, index, p, attrs);
reject:
  if (engine->current->strictMode)
      engine->throwTypeError();
  return false;
}

bool Object::defineOwnProperty2(ExecutionEngine *engine, uint index, const Property *p, PropertyAttributes attrs)
{
    bool hasProperty = 0;

    // Clause 1
    if (arrayData()) {
        hasProperty = arrayData()->getProperty(index);
        if (!hasProperty  && isStringObject())
            hasProperty = (index < static_cast<StringObject *>(this)->length());
    }

    if (!hasProperty) {
        // clause 3
        if (!isExtensible())
            goto reject;
        // clause 4
        Scope scope(engine);
        ScopedProperty pp(scope);
        pp->copy(p, attrs);
        pp->fullyPopulated(&attrs);
        if (attrs == Attr_Data) {
            ScopedValue v(scope, pp->value);
            arraySet(index, v);
        } else {
            arraySet(index, pp, attrs);
        }
        return true;
    }

    return __defineOwnProperty__(engine, index, 0, p, attrs);
reject:
  if (engine->current->strictMode)
      engine->throwTypeError();
  return false;
}

bool Object::__defineOwnProperty__(ExecutionEngine *engine, uint index, String *member, const Property *p, PropertyAttributes attrs)
{
    // clause 5
    if (attrs.isEmpty())
        return true;

    Scope scope(engine);
    ScopedProperty current(scope);
    PropertyAttributes cattrs;
    if (member) {
        getProperty(index, current, &cattrs);
        cattrs = internalClass()->propertyData[index];
    } else if (arrayData()) {
        arrayData()->getProperty(index, current, &cattrs);
        cattrs = arrayData()->attributes(index);
    }

    // clause 6
    if (p->isSubset(attrs, current, cattrs))
        return true;

    // clause 7
    if (!cattrs.isConfigurable()) {
        if (attrs.isConfigurable())
            goto reject;
        if (attrs.hasEnumerable() && attrs.isEnumerable() != cattrs.isEnumerable())
            goto reject;
    }

    // clause 8
    if (attrs.isGeneric() || current->value.isEmpty())
        goto accept;

    // clause 9
    if (cattrs.isData() != attrs.isData()) {
        // 9a
        if (!cattrs.isConfigurable())
            goto reject;
        if (cattrs.isData()) {
            // 9b
            cattrs.setType(PropertyAttributes::Accessor);
            cattrs.clearWritable();
            if (!member) {
                // need to convert the array and the slot
                initSparseArray();
                Q_ASSERT(arrayData());
                setArrayAttributes(index, cattrs);
            }
            current->setGetter(0);
            current->setSetter(0);
        } else {
            // 9c
            cattrs.setType(PropertyAttributes::Data);
            cattrs.setWritable(false);
            if (!member) {
                // need to convert the array and the slot
                setArrayAttributes(index, cattrs);
            }
            current->value = Primitive::undefinedValue();
        }
    } else if (cattrs.isData() && attrs.isData()) { // clause 10
        if (!cattrs.isConfigurable() && !cattrs.isWritable()) {
            if (attrs.isWritable() || !current->value.sameValue(p->value))
                goto reject;
        }
    } else { // clause 10
        Q_ASSERT(cattrs.isAccessor() && attrs.isAccessor());
        if (!cattrs.isConfigurable()) {
            if (!p->value.isEmpty() && current->value.rawValue() != p->value.rawValue())
                goto reject;
            if (!p->set.isEmpty() && current->set.rawValue() != p->set.rawValue())
                goto reject;
        }
    }

  accept:

    current->merge(cattrs, p, attrs);
    if (member) {
        InternalClass::changeMember(this, member, cattrs);
        setProperty(index, current);
    } else {
        setArrayAttributes(index, cattrs);
        arrayData()->setProperty(index, current);
    }
    return true;
  reject:
    if (engine->current->strictMode)
        engine->throwTypeError();
    return false;
}


bool Object::__defineOwnProperty__(ExecutionEngine *engine, const QString &name, const Property *p, PropertyAttributes attrs)
{
    Scope scope(engine);
    ScopedString s(scope, engine->newString(name));
    return __defineOwnProperty__(engine, s, p, attrs);
}


void Object::copyArrayData(Object *other)
{
    Q_ASSERT(isArrayObject());
    Scope scope(engine());

    if (other->protoHasArray() || ArgumentsObject::isNonStrictArgumentsObject(other) ||
        (other->arrayType() == Heap::ArrayData::Sparse && other->arrayData()->attrs)) {
        uint len = other->getLength();
        Q_ASSERT(len);

        ScopedValue v(scope);
        for (uint i = 0; i < len; ++i) {
            arraySet(i, (v = other->getIndexed(i)));
        }
    } else if (!other->arrayData()) {
        ;
    } else {
        Q_ASSERT(!arrayData() && other->arrayData());
        ArrayData::realloc(this, other->d()->arrayData->type, other->d()->arrayData->alloc, false);
        if (other->arrayType() == Heap::ArrayData::Sparse) {
            Heap::ArrayData *od = other->d()->arrayData;
            Heap::ArrayData *dd = d()->arrayData;
            dd->sparse = new SparseArray(*od->sparse);
            dd->freeList = od->freeList;
        } else {
            Heap::ArrayData *dd = d()->arrayData;
            dd->len = other->d()->arrayData->len;
            dd->offset = other->d()->arrayData->offset;
        }
        memcpy(d()->arrayData->arrayData, other->d()->arrayData->arrayData, other->d()->arrayData->alloc*sizeof(Value));
    }
    setArrayLengthUnchecked(other->getLength());
}

uint Object::getLength(const Managed *m)
{
    Scope scope(static_cast<const Object *>(m)->engine());
    ScopedValue v(scope, static_cast<Object *>(const_cast<Managed *>(m))->get(scope.engine->id_length()));
    return v->toUInt32();
}

bool Object::setArrayLength(uint newLen)
{
    Q_ASSERT(isArrayObject());
    if (!internalClass()->propertyData[Heap::ArrayObject::LengthPropertyIndex].isWritable())
        return false;
    uint oldLen = getLength();
    bool ok = true;
    if (newLen < oldLen) {
        if (!arrayData()) {
            Q_ASSERT(!newLen);
        } else {
            uint l = arrayData()->vtable()->truncate(this, newLen);
            if (l != newLen)
                ok = false;
            newLen = l;
        }
    } else {
        if (newLen >= 0x100000)
            initSparseArray();
    }
    setArrayLengthUnchecked(newLen);
    return ok;
}

void Object::initSparseArray()
{
    if (arrayType() == Heap::ArrayData::Sparse)
        return;

    ArrayData::realloc(this, Heap::ArrayData::Sparse, 0, false);
}


DEFINE_OBJECT_VTABLE(ArrayObject);

void Heap::ArrayObject::init(const QStringList &list)
{
    Object::init();
    commonInit();
    Scope scope(internalClass->engine);
    ScopedObject a(scope, this);

    // Converts a QStringList to JS.
    // The result is a new Array object with length equal to the length
    // of the QStringList, and the elements being the QStringList's
    // elements converted to JS Strings.
    int len = list.count();
    a->arrayReserve(len);
    ScopedValue v(scope);
    for (int ii = 0; ii < len; ++ii)
        a->arrayPut(ii, (v = scope.engine->newString(list.at(ii))));
    a->setArrayLengthUnchecked(len);
}

ReturnedValue ArrayObject::getLookup(const Managed *m, Lookup *l)
{
    Scope scope(static_cast<const Object *>(m)->engine());
    ScopedString name(scope, scope.engine->current->compilationUnit->runtimeStrings[l->nameIndex]);
    if (name->equals(scope.engine->id_length())) {
        // special case, as the property is on the object itself
        l->getter = Lookup::arrayLengthGetter;
        const ArrayObject *a = static_cast<const ArrayObject *>(m);
        return a->propertyData(Heap::ArrayObject::LengthPropertyIndex)->asReturnedValue();
    }
    return Object::getLookup(m, l);
}

uint ArrayObject::getLength(const Managed *m)
{
    const ArrayObject *a = static_cast<const ArrayObject *>(m);
    if (a->propertyData(Heap::ArrayObject::LengthPropertyIndex)->isInteger())
        return a->propertyData(Heap::ArrayObject::LengthPropertyIndex)->integerValue();
    return Primitive::toUInt32(a->propertyData(Heap::ArrayObject::LengthPropertyIndex)->doubleValue());
}

QStringList ArrayObject::toQStringList() const
{
    QStringList result;

    QV4::ExecutionEngine *engine = internalClass()->engine;
    Scope scope(engine);
    ScopedValue v(scope);

    uint length = getLength();
    for (uint i = 0; i < length; ++i) {
        v = const_cast<ArrayObject *>(this)->getIndexed(i);
        result.append(v->toQStringNoThrow());
    }
    return result;
}
