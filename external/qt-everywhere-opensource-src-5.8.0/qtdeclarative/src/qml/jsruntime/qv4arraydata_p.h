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
#ifndef QV4ARRAYDATA_H
#define QV4ARRAYDATA_H

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
#include "qv4managed_p.h"
#include "qv4property_p.h"
#include "qv4sparsearray_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

#define V4_ARRAYDATA(DataClass) \
    public: \
        Q_MANAGED_CHECK \
        typedef QV4::Heap::DataClass Data; \
        static const QV4::ArrayVTable static_vtbl; \
        static inline const QV4::VTable *staticVTable() { return &static_vtbl.vTable; } \
        V4_MANAGED_SIZE_TEST \
        const Data *d() const { return static_cast<const Data *>(m()); } \
        Data *d() { return static_cast<Data *>(m()); }


struct ArrayData;

struct ArrayVTable
{
    VTable vTable;
    uint type;
    Heap::ArrayData *(*reallocate)(Object *o, uint n, bool enforceAttributes);
    ReturnedValue (*get)(const Heap::ArrayData *d, uint index);
    bool (*put)(Object *o, uint index, const Value &value);
    bool (*putArray)(Object *o, uint index, const Value *values, uint n);
    bool (*del)(Object *o, uint index);
    void (*setAttribute)(Object *o, uint index, PropertyAttributes attrs);
    void (*push_front)(Object *o, const Value *values, uint n);
    ReturnedValue (*pop_front)(Object *o);
    uint (*truncate)(Object *o, uint newLen);
    uint (*length)(const Heap::ArrayData *d);
};

namespace Heap {

struct ArrayData : public Base {
    enum Type {
        Simple = 0,
        Complex = 1,
        Sparse = 2,
        Custom = 3
    };

    uint alloc;
    Type type;
    PropertyAttributes *attrs;
    union {
        uint len;
        ReturnedValue freeList;
    };
    union {
        uint offset;
        SparseArray *sparse;
    };
    Value arrayData[1];

    bool isSparse() const { return type == Sparse; }

    const ArrayVTable *vtable() const { return reinterpret_cast<const ArrayVTable *>(Base::vtable()); }

    inline ReturnedValue get(uint i) const {
        return vtable()->get(this, i);
    }
    inline void getProperty(uint index, Property *p, PropertyAttributes *attrs);
    inline void setProperty(uint index, const Property *p);
    inline Property *getProperty(uint index);
    inline Value *getValueOrSetter(uint index, PropertyAttributes *attrs);
    inline PropertyAttributes attributes(uint i) const;

    bool isEmpty(uint i) const {
        return get(i) == Primitive::emptyValue().asReturnedValue();
    }

    inline ReturnedValue length() const {
        return vtable()->length(this);
    }

};
V4_ASSERT_IS_TRIVIAL(ArrayData)

struct SimpleArrayData : public ArrayData {
    uint mappedIndex(uint index) const { return (index + offset) % alloc; }
    Value data(uint index) const { return arrayData[mappedIndex(index)]; }
    Value &data(uint index) { return arrayData[mappedIndex(index)]; }

    Property *getProperty(uint index) {
        if (index >= len)
            return 0;
        index = mappedIndex(index);
        if (arrayData[index].isEmpty())
            return 0;
        return reinterpret_cast<Property *>(arrayData + index);
    }

    PropertyAttributes attributes(uint i) const {
        return attrs ? attrs[i] : Attr_Data;
    }
};
V4_ASSERT_IS_TRIVIAL(SimpleArrayData)

struct SparseArrayData : public ArrayData {
    void destroy() {
        delete sparse;
        ArrayData::destroy();
    }

    uint mappedIndex(uint index) const {
        SparseArrayNode *n = sparse->findNode(index);
        if (!n)
            return UINT_MAX;
        return n->value;
    }

    Property *getProperty(uint index) {
        SparseArrayNode *n = sparse->findNode(index);
        if (!n)
            return 0;
        return reinterpret_cast<Property *>(arrayData + n->value);
    }

    PropertyAttributes attributes(uint i) const {
        if (!attrs)
            return Attr_Data;
        uint index = mappedIndex(i);
        return index < UINT_MAX ? attrs[index] : Attr_Data;
    }
};

}

struct Q_QML_EXPORT ArrayData : public Managed
{
    typedef Heap::ArrayData::Type Type;
    V4_MANAGED(ArrayData, Managed)

    uint alloc() const { return d()->alloc; }
    uint &alloc() { return d()->alloc; }
    void setAlloc(uint a) { d()->alloc = a; }
    Type type() const { return d()->type; }
    void setType(Type t) { d()->type = t; }
    PropertyAttributes *attrs() const { return d()->attrs; }
    void setAttrs(PropertyAttributes *a) { d()->attrs = a; }
    const Value *arrayData() const { return &d()->arrayData[0]; }
    Value *arrayData() { return &d()->arrayData[0]; }

    const ArrayVTable *vtable() const { return d()->vtable(); }
    bool isSparse() const { return type() == Heap::ArrayData::Sparse; }

    uint length() const {
        return d()->length();
    }

    bool hasAttributes() const {
        return attrs();
    }
    PropertyAttributes attributes(uint i) const {
        return d()->attributes(i);
    }

    bool isEmpty(uint i) const {
        return d()->isEmpty(i);
    }

    ReturnedValue get(uint i) const {
        return d()->get(i);
    }
    inline Property *getProperty(uint index) {
        return d()->getProperty(index);
    }

    static void ensureAttributes(Object *o);
    static void realloc(Object *o, Type newType, uint alloc, bool enforceAttributes);

    static void sort(ExecutionEngine *engine, Object *thisObject, const Value &comparefn, uint dataLen);
    static uint append(Object *obj, ArrayObject *otherObj, uint n);
    static void insert(Object *o, uint index, const Value *v, bool isAccessor = false);
};

struct Q_QML_EXPORT SimpleArrayData : public ArrayData
{
    V4_ARRAYDATA(SimpleArrayData)

    uint mappedIndex(uint index) const { return d()->mappedIndex(index); }
    Value data(uint index) const { return d()->data(index); }
    Value &data(uint index) { return d()->data(index); }

    uint &len() { return d()->len; }
    uint len() const { return d()->len; }

    static Heap::ArrayData *reallocate(Object *o, uint n, bool enforceAttributes);

    static void markObjects(Heap::Base *d, ExecutionEngine *e);

    static ReturnedValue get(const Heap::ArrayData *d, uint index);
    static bool put(Object *o, uint index, const Value &value);
    static bool putArray(Object *o, uint index, const Value *values, uint n);
    static bool del(Object *o, uint index);
    static void setAttribute(Object *o, uint index, PropertyAttributes attrs);
    static void push_front(Object *o, const Value *values, uint n);
    static ReturnedValue pop_front(Object *o);
    static uint truncate(Object *o, uint newLen);
    static uint length(const Heap::ArrayData *d);
};

struct Q_QML_EXPORT SparseArrayData : public ArrayData
{
    V4_ARRAYDATA(SparseArrayData)
    V4_NEEDS_DESTROY

    ReturnedValue &freeList() { return d()->freeList; }
    ReturnedValue freeList() const { return d()->freeList; }
    SparseArray *sparse() const { return d()->sparse; }
    void setSparse(SparseArray *s) { d()->sparse = s; }

    static uint allocate(Object *o, bool doubleSlot = false);
    static void free(Heap::ArrayData *d, uint idx);

    uint mappedIndex(uint index) const { return d()->mappedIndex(index); }

    static void markObjects(Heap::Base *d, ExecutionEngine *e);

    static Heap::ArrayData *reallocate(Object *o, uint n, bool enforceAttributes);
    static ReturnedValue get(const Heap::ArrayData *d, uint index);
    static bool put(Object *o, uint index, const Value &value);
    static bool putArray(Object *o, uint index, const Value *values, uint n);
    static bool del(Object *o, uint index);
    static void setAttribute(Object *o, uint index, PropertyAttributes attrs);
    static void push_front(Object *o, const Value *values, uint n);
    static ReturnedValue pop_front(Object *o);
    static uint truncate(Object *o, uint newLen);
    static uint length(const Heap::ArrayData *d);
};

namespace Heap {

void ArrayData::getProperty(uint index, Property *p, PropertyAttributes *attrs)
{
    Property *pd = getProperty(index);
    Q_ASSERT(pd);
    *attrs = attributes(index);
    p->value = pd->value;
    if (attrs->isAccessor())
        p->set = pd->set;
}

void ArrayData::setProperty(uint index, const Property *p)
{
    Property *pd = getProperty(index);
    Q_ASSERT(pd);
    pd->value = p->value;
    if (attributes(index).isAccessor())
        pd->set = p->set;
}

inline Property *ArrayData::getProperty(uint index)
{
    if (isSparse())
        return static_cast<SparseArrayData *>(this)->getProperty(index);
    return static_cast<SimpleArrayData *>(this)->getProperty(index);
}

inline PropertyAttributes ArrayData::attributes(uint i) const
{
    if (isSparse())
        return static_cast<const SparseArrayData *>(this)->attributes(i);
    return static_cast<const SimpleArrayData *>(this)->attributes(i);
}

Value *ArrayData::getValueOrSetter(uint index, PropertyAttributes *attrs)
{
    Property *p = getProperty(index);
    if (!p) {
        *attrs = Attr_Invalid;
        return 0;
    }

    *attrs = attributes(index);
    return attrs->isAccessor() ? &p->set : &p->value;
}



}

}

QT_END_NAMESPACE

#endif
