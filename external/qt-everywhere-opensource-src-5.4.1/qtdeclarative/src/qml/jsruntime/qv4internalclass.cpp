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

#include <qv4internalclass_p.h>
#include <qv4string_p.h>
#include <qv4engine_p.h>
#include <qv4identifier_p.h>
#include "qv4object_p.h"
#include "qv4identifiertable_p.h"

QT_BEGIN_NAMESPACE

uint QV4::qHash(const QV4::InternalClassTransition &t, uint)
{
    if (t.flags == QV4::InternalClassTransition::ProtoChange)
        // INT_MAX is prime, so this should give a decent distribution of keys
        return (uint)((quintptr)t.prototype * INT_MAX);
    return t.id->hashValue ^ t.flags;
}

using namespace QV4;

static const uchar prime_deltas[] = {
    0,  0,  1,  3,  1,  5,  3,  3,  1,  9,  7,  5,  3,  9, 25,  3,
    1, 21,  3, 21,  7, 15,  9,  5,  3, 29, 15,  0,  0,  0,  0,  0
};

static inline int primeForNumBits(int numBits)
{
    return (1 << numBits) + prime_deltas[numBits];
}

PropertyHashData::PropertyHashData(int numBits)
    : refCount(1)
    , size(0)
    , numBits(numBits)
{
    alloc = primeForNumBits(numBits);
    entries = (PropertyHash::Entry *)malloc(alloc*sizeof(PropertyHash::Entry));
    memset(entries, 0, alloc*sizeof(PropertyHash::Entry));
}

void PropertyHash::addEntry(const PropertyHash::Entry &entry, int classSize)
{
    // fill up to max 50%
    bool grow = (d->alloc <= d->size*2);

    if (classSize < d->size || grow) {
        PropertyHashData *dd = new PropertyHashData(grow ? d->numBits + 1 : d->numBits);
        for (int i = 0; i < d->alloc; ++i) {
            const Entry &e = d->entries[i];
            if (!e.identifier || e.index >= static_cast<unsigned>(classSize))
                continue;
            uint idx = e.identifier->hashValue % dd->alloc;
            while (dd->entries[idx].identifier) {
                ++idx;
                idx %= dd->alloc;
            }
            dd->entries[idx] = e;
        }
        dd->size = classSize;
        Q_ASSERT(d->refCount > 1);
        --d->refCount;
        d = dd;
    }

    uint idx = entry.identifier->hashValue % d->alloc;
    while (d->entries[idx].identifier) {
        ++idx;
        idx %= d->alloc;
    }
    d->entries[idx] = entry;
    ++d->size;
}

uint PropertyHash::lookup(const Identifier *identifier) const
{
    Q_ASSERT(d->entries);

    uint idx = identifier->hashValue % d->alloc;
    while (1) {
        if (d->entries[idx].identifier == identifier)
            return d->entries[idx].index;
        if (!d->entries[idx].identifier)
            return UINT_MAX;
        ++idx;
        idx %= d->alloc;
    }
}

InternalClass::InternalClass(ExecutionEngine *engine)
    : engine(engine)
    , prototype(0)
    , vtable(&QV4::Managed::static_vtbl)
    , m_sealed(0)
    , m_frozen(0)
    , size(0)
{
}


InternalClass::InternalClass(const QV4::InternalClass &other)
    : QQmlJS::Managed()
    , engine(other.engine)
    , prototype(other.prototype)
    , vtable(other.vtable)
    , propertyTable(other.propertyTable)
    , nameMap(other.nameMap)
    , propertyData(other.propertyData)
    , transitions()
    , m_sealed(0)
    , m_frozen(0)
    , size(other.size)
{
}

void InternalClass::changeMember(Object *object, String *string, PropertyAttributes data, uint *index)
{
    uint idx;
    InternalClass *newClass = object->internalClass()->changeMember(string, data, &idx);
    if (index)
        *index = idx;

    if (newClass->size > object->internalClass()->size) {
        Q_ASSERT(newClass->size == object->internalClass()->size + 1);
        memmove(object->memberData().data() + idx + 2, object->memberData().data() + idx + 1, (object->internalClass()->size - idx - 1)*sizeof(Value));
    } else if (newClass->size < object->internalClass()->size) {
        Q_ASSERT(newClass->size == object->internalClass()->size - 1);
        memmove(object->memberData().data() + idx + 1, object->memberData().data() + idx + 2, (object->internalClass()->size - idx - 2)*sizeof(Value));
    }
    object->setInternalClass(newClass);
}

InternalClass *InternalClass::changeMember(String *string, PropertyAttributes data, uint *index)
{
    data.resolve();
    uint idx = find(string);
    Q_ASSERT(idx != UINT_MAX);

    if (index)
        *index = idx;

    if (data == propertyData.at(idx))
        return this;

    Transition t = { { string->d()->identifier }, (int)data.flags() };
    QHash<Transition, InternalClass *>::const_iterator tit = transitions.constFind(t);
    if (tit != transitions.constEnd())
        return tit.value();

    // create a new class and add it to the tree
    InternalClass *newClass = engine->emptyClass->changeVTable(vtable);
    newClass = newClass->changePrototype(prototype);
    for (uint i = 0; i < size; ++i) {
        if (i == idx) {
            newClass = newClass->addMember(nameMap.at(i), data);
        } else if (!propertyData.at(i).isEmpty()) {
            newClass = newClass->addMember(nameMap.at(i), propertyData.at(i));
        }
    }

    transitions.insert(t, newClass);
    return newClass;
}

InternalClass *InternalClass::create(ExecutionEngine *engine, const ManagedVTable *vtable, Object *proto)
{
    InternalClass *c = engine->emptyClass->changeVTable(vtable);
    if (!proto)
        return c;
    return c->changePrototype(proto);
}

InternalClass *InternalClass::changePrototype(Object *proto)
{
    if (prototype == proto)
        return this;

    Transition t;
    t.prototype = proto;
    t.flags = Transition::ProtoChange;

    QHash<Transition, InternalClass *>::const_iterator tit = transitions.constFind(t);
    if (tit != transitions.constEnd())
        return tit.value();

    // create a new class and add it to the tree
    InternalClass *newClass;
    if (!size) {
        newClass = engine->newClass(*this);
        newClass->prototype = proto;
    } else {
        newClass = engine->emptyClass->changeVTable(vtable);
        newClass = newClass->changePrototype(proto);
        for (uint i = 0; i < size; ++i) {
            if (!propertyData.at(i).isEmpty())
                newClass = newClass->addMember(nameMap.at(i), propertyData.at(i));
        }
    }

    transitions.insert(t, newClass);
    return newClass;
}

InternalClass *InternalClass::changeVTable(const ManagedVTable *vt)
{
    if (vtable == vt)
        return this;

    Transition t;
    t.vtable = vt;
    t.flags = Transition::VTableChange;

    QHash<Transition, InternalClass *>::const_iterator tit = transitions.constFind(t);
    if (tit != transitions.constEnd())
        return tit.value();

    // create a new class and add it to the tree
    InternalClass *newClass;
    if (this == engine->emptyClass) {
        newClass = engine->newClass(*this);
        newClass->vtable = vt;
    } else {
        newClass = engine->emptyClass->changeVTable(vt);
        newClass = newClass->changePrototype(prototype);
        for (uint i = 0; i < size; ++i) {
            if (!propertyData.at(i).isEmpty())
                newClass = newClass->addMember(nameMap.at(i), propertyData.at(i));
        }
    }

    transitions.insert(t, newClass);
    return newClass;
}

void InternalClass::addMember(Object *object, String *string, PropertyAttributes data, uint *index)
{
    data.resolve();
    object->internalClass()->engine->identifierTable->identifier(string);
    if (object->internalClass()->propertyTable.lookup(string->d()->identifier) < object->internalClass()->size) {
        changeMember(object, string, data, index);
        return;
    }

    uint idx;
    InternalClass *newClass = object->internalClass()->addMemberImpl(string, data, &idx);
    if (index)
        *index = idx;

    object->setInternalClass(newClass);
}


InternalClass *InternalClass::addMember(String *string, PropertyAttributes data, uint *index)
{
    data.resolve();
    engine->identifierTable->identifier(string);

    if (propertyTable.lookup(string->d()->identifier) < size)
        return changeMember(string, data, index);

    return addMemberImpl(string, data, index);
}

InternalClass *InternalClass::addMemberImpl(String *string, PropertyAttributes data, uint *index)
{
    Transition t = { { string->d()->identifier }, (int)data.flags() };
    QHash<Transition, InternalClass *>::const_iterator tit = transitions.constFind(t);

    if (index)
        *index = size;
    if (tit != transitions.constEnd())
        return tit.value();

    // create a new class and add it to the tree
    InternalClass *newClass = engine->newClass(*this);
    PropertyHash::Entry e = { string->d()->identifier, newClass->size };
    newClass->propertyTable.addEntry(e, newClass->size);

    // The incoming string can come from anywhere, so make sure to
    // store a string in the nameMap that's guaranteed to get
    // marked properly during GC.
    String *name = engine->newIdentifier(string->toQString());
    newClass->nameMap.add(newClass->size, name);
    newClass->propertyData.add(newClass->size, data);
    ++newClass->size;
    if (data.isAccessor()) {
        // add a dummy entry, since we need two entries for accessors
        newClass->propertyTable.addEntry(e, newClass->size);
        newClass->nameMap.add(newClass->size, 0);
        newClass->propertyData.add(newClass->size, PropertyAttributes());
        ++newClass->size;
    }

    transitions.insert(t, newClass);
    return newClass;
}

void InternalClass::removeMember(Object *object, Identifier *id)
{
    InternalClass *oldClass = object->internalClass();
    uint propIdx = oldClass->propertyTable.lookup(id);
    Q_ASSERT(propIdx < oldClass->size);

    Transition t = { { id } , -1 };
    QHash<Transition, InternalClass *>::const_iterator tit = object->internalClass()->transitions.constFind(t);

    if (tit != object->internalClass()->transitions.constEnd()) {
        object->setInternalClass(tit.value());
    } else {
        // create a new class and add it to the tree
        InternalClass *newClass = oldClass->engine->emptyClass->changeVTable(oldClass->vtable);
        newClass = newClass->changePrototype(oldClass->prototype);
        for (uint i = 0; i < oldClass->size; ++i) {
            if (i == propIdx)
                continue;
            if (!oldClass->propertyData.at(i).isEmpty())
                newClass = newClass->addMember(oldClass->nameMap.at(i), oldClass->propertyData.at(i));
        }
        object->setInternalClass(newClass);
    }

    // remove the entry in memberdata
    memmove(object->memberData().data() + propIdx, object->memberData().data() + propIdx + 1, (object->internalClass()->size - propIdx)*sizeof(Value));

    oldClass->transitions.insert(t, object->internalClass());
}

uint InternalClass::find(const String *string)
{
    engine->identifierTable->identifier(string);
    const Identifier *id = string->d()->identifier;

    uint index = propertyTable.lookup(id);
    if (index < size)
        return index;

    return UINT_MAX;
}

InternalClass *InternalClass::sealed()
{
    if (m_sealed)
        return m_sealed;

    m_sealed = engine->emptyClass;
    m_sealed = m_sealed->changeVTable(vtable);
    m_sealed = m_sealed->changePrototype(prototype);
    for (uint i = 0; i < size; ++i) {
        PropertyAttributes attrs = propertyData.at(i);
        if (attrs.isEmpty())
            continue;
        attrs.setConfigurable(false);
        m_sealed = m_sealed->addMember(nameMap.at(i), attrs);
    }

    m_sealed->m_sealed = m_sealed;
    return m_sealed;
}

InternalClass *InternalClass::frozen()
{
    if (m_frozen)
        return m_frozen;

    m_frozen = engine->emptyClass;
    m_frozen = m_frozen->changeVTable(vtable);
    m_frozen = m_frozen->changePrototype(prototype);
    for (uint i = 0; i < size; ++i) {
        PropertyAttributes attrs = propertyData.at(i);
        if (attrs.isEmpty())
            continue;
        attrs.setWritable(false);
        attrs.setConfigurable(false);
        m_frozen = m_frozen->addMember(nameMap.at(i), attrs);
    }

    m_frozen->m_frozen = m_frozen;
    m_frozen->m_sealed = m_frozen;
    return m_frozen;
}

void InternalClass::destroy()
{
    QList<InternalClass *> destroyStack;
    destroyStack.append(this);

    while (!destroyStack.isEmpty()) {
        InternalClass *next = destroyStack.takeLast();
        if (!next->engine)
            continue;
        next->engine = 0;
        next->propertyTable.~PropertyHash();
        next->nameMap.~SharedInternalClassData<String *>();
        next->propertyData.~SharedInternalClassData<PropertyAttributes>();
        if (next->m_sealed)
            destroyStack.append(next->m_sealed);
        if (next->m_frozen)
            destroyStack.append(next->m_frozen);
        destroyStack.append(next->transitions.values());
        next->transitions.clear();
    }
}

struct InternalClassPoolVisitor
{
    ExecutionEngine *engine;
    void operator()(InternalClass *klass)
    {
        // all prototype changes are done on the empty class
        Q_ASSERT(!klass->prototype || klass != engine->emptyClass);

        if (klass->prototype)
            klass->prototype->mark(engine);
    }
};

void InternalClassPool::markObjects(ExecutionEngine *engine)
{
    InternalClassPoolVisitor v;
    v.engine = engine;
    visitManagedPool<InternalClass, InternalClassPoolVisitor>(v);
}

QT_END_NAMESPACE
