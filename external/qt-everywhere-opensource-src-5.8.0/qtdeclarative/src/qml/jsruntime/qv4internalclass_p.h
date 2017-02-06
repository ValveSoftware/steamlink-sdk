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
#ifndef QV4INTERNALCLASS_H
#define QV4INTERNALCLASS_H

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

#include "qv4global_p.h"

#include <QHash>
#include <private/qqmljsmemorypool_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4identifiertable_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct String;
struct ExecutionEngine;
struct Object;
struct Identifier;
struct VTable;

struct PropertyHashData;
struct PropertyHash
{
    struct Entry {
        const Identifier *identifier;
        uint index;
    };

    PropertyHashData *d;

    inline PropertyHash();
    inline PropertyHash(const PropertyHash &other);
    inline ~PropertyHash();

    void addEntry(const Entry &entry, int classSize);
    uint lookup(const Identifier *identifier) const;

private:
    PropertyHash &operator=(const PropertyHash &other);
};

struct PropertyHashData
{
    PropertyHashData(int numBits);
    ~PropertyHashData() {
        free(entries);
    }

    int refCount;
    int alloc;
    int size;
    int numBits;
    PropertyHash::Entry *entries;
};

inline PropertyHash::PropertyHash()
{
    d = new PropertyHashData(3);
}

inline PropertyHash::PropertyHash(const PropertyHash &other)
{
    d = other.d;
    ++d->refCount;
}

inline PropertyHash::~PropertyHash()
{
    if (!--d->refCount)
        delete d;
}

inline uint PropertyHash::lookup(const Identifier *identifier) const
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

template <typename T>
struct SharedInternalClassData {
    struct Private {
        Private(int alloc)
            : refcount(1),
              alloc(alloc),
              size(0)
        { data = new T  [alloc]; }
        ~Private() { delete [] data; }

        int refcount;
        uint alloc;
        uint size;
        T *data;
    };
    Private *d;

    inline SharedInternalClassData() {
        d = new Private(8);
    }

    inline SharedInternalClassData(const SharedInternalClassData &other)
        : d(other.d)
    {
        ++d->refcount;
    }
    inline ~SharedInternalClassData() {
        if (!--d->refcount)
            delete d;
    }

    void add(uint pos, T value) {
        if (pos < d->size) {
            Q_ASSERT(d->refcount > 1);
            // need to detach
            Private *dd = new Private(pos + 8);
            memcpy(dd->data, d->data, pos*sizeof(T));
            dd->size = pos + 1;
            dd->data[pos] = value;
            if (!--d->refcount)
                delete d;
            d = dd;
            return;
        }
        Q_ASSERT(pos == d->size);
        if (pos == d->alloc) {
            T *n = new T[d->alloc * 2];
            memcpy(n, d->data, d->alloc*sizeof(T));
            delete [] d->data;
            d->data = n;
            d->alloc *= 2;
        }
        d->data[pos] = value;
        ++d->size;
    }

    void set(uint pos, T value) {
        Q_ASSERT(pos < d->size);
        if (d->refcount > 1) {
            // need to detach
            Private *dd = new Private(d->alloc);
            memcpy(dd->data, d->data, d->size*sizeof(T));
            dd->size = d->size;
            if (!--d->refcount)
                delete d;
            d = dd;
        }
        d->data[pos] = value;
    }

    T *constData() const {
        return d->data;
    }
    T at(uint i) const {
        Q_ASSERT(i < d->size);
        return d->data[i];
    }
    T operator[] (uint i) {
        Q_ASSERT(i < d->size);
        return d->data[i];
    }

private:
    SharedInternalClassData &operator=(const SharedInternalClassData &other);
};

struct InternalClassTransition
{
    Identifier *id;
    InternalClass *lookup;
    int flags;
    enum {
        // range 0-0xff is reserved for attribute changes
        NotExtensible = 0x100
    };

    bool operator==(const InternalClassTransition &other) const
    { return id == other.id && flags == other.flags; }

    bool operator<(const InternalClassTransition &other) const
    { return id < other.id; }
};

struct InternalClass : public QQmlJS::Managed {
    ExecutionEngine *engine;

    PropertyHash propertyTable; // id to valueIndex
    SharedInternalClassData<Identifier *> nameMap;
    SharedInternalClassData<PropertyAttributes> propertyData;

    typedef InternalClassTransition Transition;
    std::vector<Transition> transitions;
    InternalClassTransition &lookupOrInsertTransition(const InternalClassTransition &t);

    InternalClass *m_sealed;
    InternalClass *m_frozen;

    uint size;
    bool extensible;

    InternalClass *nonExtensible();
    static void addMember(Object *object, String *string, PropertyAttributes data, uint *index);
    InternalClass *addMember(String *string, PropertyAttributes data, uint *index = 0);
    InternalClass *addMember(Identifier *identifier, PropertyAttributes data, uint *index = 0);
    InternalClass *changeMember(Identifier *identifier, PropertyAttributes data, uint *index = 0);
    static void changeMember(Object *object, String *string, PropertyAttributes data, uint *index = 0);
    static void removeMember(Object *object, Identifier *id);
    uint find(const String *string);
    uint find(const Identifier *id);

    InternalClass *sealed();
    InternalClass *frozen();
    InternalClass *propertiesFrozen() const;

    void destroy();

private:
    InternalClass *addMemberImpl(Identifier *identifier, PropertyAttributes data, uint *index);
    friend struct ExecutionEngine;
    InternalClass(ExecutionEngine *engine);
    InternalClass(const InternalClass &other);
};

inline uint InternalClass::find(const String *string)
{
    engine->identifierTable->identifier(string);
    const Identifier *id = string->d()->identifier;

    uint index = propertyTable.lookup(id);
    if (index < size)
        return index;

    return UINT_MAX;
}

struct InternalClassPool : public QQmlJS::MemoryPool
{
    void markObjects(ExecutionEngine *engine);
};

}

QT_END_NAMESPACE

#endif
