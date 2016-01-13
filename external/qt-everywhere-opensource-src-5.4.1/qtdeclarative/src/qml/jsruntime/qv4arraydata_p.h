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
#ifndef QV4ARRAYDATA_H
#define QV4ARRAYDATA_H

#include "qv4global_p.h"
#include "qv4managed_p.h"
#include "qv4property_p.h"
#include "qv4sparsearray_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

#define V4_ARRAYDATA \
    public: \
        Q_MANAGED_CHECK \
        static const QV4::ArrayVTable static_vtbl; \
        static inline const QV4::ManagedVTable *staticVTable() { return &static_vtbl.managedVTable; } \
        template <typename T> \
        QV4::Returned<T> *asReturned() { return QV4::Returned<T>::create(this); } \
        V4_MANAGED_SIZE_TEST \
        const Data *d() const { return &static_cast<const Data &>(Managed::data); } \
        Data *d() { return &static_cast<Data &>(Managed::data); }


struct ArrayData;

struct ArrayVTable
{
    ManagedVTable managedVTable;
    uint type;
    ArrayData *(*reallocate)(Object *o, uint n, bool enforceAttributes);
    ReturnedValue (*get)(const ArrayData *d, uint index);
    bool (*put)(Object *o, uint index, ValueRef value);
    bool (*putArray)(Object *o, uint index, Value *values, uint n);
    bool (*del)(Object *o, uint index);
    void (*setAttribute)(Object *o, uint index, PropertyAttributes attrs);
    PropertyAttributes (*attribute)(const ArrayData *d, uint index);
    void (*push_front)(Object *o, Value *values, uint n);
    ReturnedValue (*pop_front)(Object *o);
    uint (*truncate)(Object *o, uint newLen);
    uint (*length)(const ArrayData *d);
};


struct Q_QML_EXPORT ArrayData : public Managed
{
    enum Type {
        Simple = 0,
        Complex = 1,
        Sparse = 2,
        Custom = 3
    };

    struct Data : public Managed::Data {
        Data(InternalClass *ic)
            : Managed::Data(ic)
        {}
        uint alloc;
        Type type;
        PropertyAttributes *attrs;
        union {
            uint len;
            uint freeList;
        };
        union {
            uint offset;
            SparseArray *sparse;
        };
        Value arrayData[1];
    };
    V4_MANAGED(Managed)

    uint alloc() const { return d()->alloc; }
    uint &alloc() { return d()->alloc; }
    void setAlloc(uint a) { d()->alloc = a; }
    Type type() const { return d()->type; }
    void setType(Type t) { d()->type = t; }
    PropertyAttributes *attrs() const { return d()->attrs; }
    void setAttrs(PropertyAttributes *a) { d()->attrs = a; }
    const Value *arrayData() const { return &d()->arrayData[0]; }
    Value *arrayData() { return &d()->arrayData[0]; }

    const ArrayVTable *vtable() const { return reinterpret_cast<const ArrayVTable *>(internalClass()->vtable); }
    bool isSparse() const { return type() == Sparse; }

    uint length() const {
        return vtable()->length(this);
    }

    bool hasAttributes() const {
        return attrs();
    }
    PropertyAttributes attributes(int i) const {
        Q_ASSERT(this);
        return attrs() ? vtable()->attribute(this, i) : Attr_Data;
    }

    bool isEmpty(uint i) const {
        return (vtable()->get(this, i) == Primitive::emptyValue().asReturnedValue());
    }

    ReturnedValue get(uint i) const {
        return vtable()->get(this, i);
    }
    inline Property *getProperty(uint index);

    static void ensureAttributes(Object *o);
    static void realloc(Object *o, Type newType, uint alloc, bool enforceAttributes);

    static void sort(ExecutionContext *context, Object *thisObject, const ValueRef comparefn, uint dataLen);
    static uint append(Object *obj, ArrayObject *otherObj, uint n);
    static Property *insert(Object *o, uint index, bool isAccessor = false);
};

struct Q_QML_EXPORT SimpleArrayData : public ArrayData
{

    struct Data : public ArrayData::Data {
        Data(ExecutionEngine *engine)
            : ArrayData::Data(engine->simpleArrayDataClass)
        {}
    };
    V4_ARRAYDATA

    uint mappedIndex(uint index) const { return (index + d()->offset) % d()->alloc; }
    Value data(uint index) const { return d()->arrayData[mappedIndex(index)]; }
    Value &data(uint index) { return d()->arrayData[mappedIndex(index)]; }

    Property *getProperty(uint index) {
        if (index >= len())
            return 0;
        index = mappedIndex(index);
        if (d()->arrayData[index].isEmpty())
            return 0;
        return reinterpret_cast<Property *>(d()->arrayData + index);
    }

    uint &len() { return d()->len; }
    uint len() const { return d()->len; }

    static ArrayData *reallocate(Object *o, uint n, bool enforceAttributes);

    static void markObjects(Managed *d, ExecutionEngine *e);

    static ReturnedValue get(const ArrayData *d, uint index);
    static bool put(Object *o, uint index, ValueRef value);
    static bool putArray(Object *o, uint index, Value *values, uint n);
    static bool del(Object *o, uint index);
    static void setAttribute(Object *o, uint index, PropertyAttributes attrs);
    static PropertyAttributes attribute(const ArrayData *d, uint index);
    static void push_front(Object *o, Value *values, uint n);
    static ReturnedValue pop_front(Object *o);
    static uint truncate(Object *o, uint newLen);
    static uint length(const ArrayData *d);
};

struct Q_QML_EXPORT SparseArrayData : public ArrayData
{
    struct Data : public ArrayData::Data {
        Data(ExecutionEngine *engine)
            : ArrayData::Data(engine->emptyClass)
        { setVTable(staticVTable()); }
    };
    V4_ARRAYDATA

    uint &freeList() { return d()->freeList; }
    uint freeList() const { return d()->freeList; }
    SparseArray *sparse() const { return d()->sparse; }
    void setSparse(SparseArray *s) { d()->sparse = s; }

    static uint allocate(Object *o, bool doubleSlot = false);
    static void free(ArrayData *d, uint idx);

    Property *getProperty(uint index) {
        SparseArrayNode *n = sparse()->findNode(index);
        if (!n)
            return 0;
        return reinterpret_cast<Property *>(arrayData() + n->value);
    }

    uint mappedIndex(uint index) const {
        SparseArrayNode *n = sparse()->findNode(index);
        if (!n)
            return UINT_MAX;
        return n->value;
    }

    static void destroy(Managed *d);
    static void markObjects(Managed *d, ExecutionEngine *e);

    static ArrayData *reallocate(Object *o, uint n, bool enforceAttributes);
    static ReturnedValue get(const ArrayData *d, uint index);
    static bool put(Object *o, uint index, ValueRef value);
    static bool putArray(Object *o, uint index, Value *values, uint n);
    static bool del(Object *o, uint index);
    static void setAttribute(Object *o, uint index, PropertyAttributes attrs);
    static PropertyAttributes attribute(const ArrayData *d, uint index);
    static void push_front(Object *o, Value *values, uint n);
    static ReturnedValue pop_front(Object *o);
    static uint truncate(Object *o, uint newLen);
    static uint length(const ArrayData *d);
};


inline Property *ArrayData::getProperty(uint index)
{
    if (type() != Sparse) {
        SimpleArrayData *that = static_cast<SimpleArrayData *>(this);
        return that->getProperty(index);
    } else {
        SparseArrayData *that = static_cast<SparseArrayData *>(this);
        return that->getProperty(index);
    }
}

}

QT_END_NAMESPACE

#endif
