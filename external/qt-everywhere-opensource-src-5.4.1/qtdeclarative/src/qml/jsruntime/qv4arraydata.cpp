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
#include "qv4arraydata_p.h"
#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4mm_p.h"

using namespace QV4;

const QV4::ManagedVTable QV4::ArrayData::static_vtbl = {
    0,
    QV4::ArrayData::IsExecutionContext,
    QV4::ArrayData::IsString,
    QV4::ArrayData::IsObject,
    QV4::ArrayData::IsFunctionObject,
    QV4::ArrayData::IsErrorObject,
    QV4::ArrayData::IsArrayData,
    0,
    QV4::ArrayData::MyType,
    "ArrayData",
    Q_VTABLE_FUNCTION(QV4::ArrayData, destroy),
    0,
    isEqualTo
};

const ArrayVTable SimpleArrayData::static_vtbl =
{
    DEFINE_MANAGED_VTABLE_INT(SimpleArrayData, 0),
    SimpleArrayData::Simple,
    SimpleArrayData::reallocate,
    SimpleArrayData::get,
    SimpleArrayData::put,
    SimpleArrayData::putArray,
    SimpleArrayData::del,
    SimpleArrayData::setAttribute,
    SimpleArrayData::attribute,
    SimpleArrayData::push_front,
    SimpleArrayData::pop_front,
    SimpleArrayData::truncate,
    SimpleArrayData::length
};

const ArrayVTable SparseArrayData::static_vtbl =
{
    DEFINE_MANAGED_VTABLE_INT(SparseArrayData, 0),
    ArrayData::Sparse,
    SparseArrayData::reallocate,
    SparseArrayData::get,
    SparseArrayData::put,
    SparseArrayData::putArray,
    SparseArrayData::del,
    SparseArrayData::setAttribute,
    SparseArrayData::attribute,
    SparseArrayData::push_front,
    SparseArrayData::pop_front,
    SparseArrayData::truncate,
    SparseArrayData::length
};

Q_STATIC_ASSERT(sizeof(ArrayData::Data) == sizeof(SimpleArrayData::Data));
Q_STATIC_ASSERT(sizeof(ArrayData::Data) == sizeof(SparseArrayData::Data));

void ArrayData::realloc(Object *o, Type newType, uint requested, bool enforceAttributes)
{
    ArrayData *d = o->arrayData();

    uint alloc = 8;
    uint toCopy = 0;
    uint offset = 0;

    if (d) {
        bool hasAttrs = d->attrs();
        enforceAttributes |= hasAttrs;

        if (requested <= d->alloc() && newType == d->type() && hasAttrs == enforceAttributes)
            return;
        if (alloc < d->alloc())
            alloc = d->alloc();

        if (d->type() < Sparse) {
            offset = static_cast<SimpleArrayData *>(d)->d()->offset;
            toCopy = static_cast<SimpleArrayData *>(d)->len();
        } else {
            toCopy = d->alloc();
        }
        if (d->type() > newType)
            newType = d->type();
    }
    if (enforceAttributes && newType == Simple)
        newType = Complex;

    while (alloc < requested)
        alloc *= 2;
    size_t size = sizeof(ArrayData::Data) + (alloc - 1)*sizeof(Value);
    if (enforceAttributes)
        size += alloc*sizeof(PropertyAttributes);

    ArrayData *newData;
    if (newType < Sparse) {
        SimpleArrayData *n = static_cast<SimpleArrayData *>(o->engine()->memoryManager->allocManaged(size));
        new (n->d()) SimpleArrayData::Data(o->engine());
        n->d()->offset = 0;
        n->len() = d ? static_cast<SimpleArrayData *>(d)->len() : 0;
        newData = n;
    } else {
        SparseArrayData *n = static_cast<SparseArrayData *>(o->engine()->memoryManager->allocManaged(size));
        new (n->d()) SparseArrayData::Data(o->engine());
        newData = n;
    }
    newData->setAlloc(alloc);
    newData->setType(newType);
    newData->setAttrs(enforceAttributes ? reinterpret_cast<PropertyAttributes *>(newData->d()->arrayData + alloc) : 0);
    o->setArrayData(newData);

    if (d) {
        if (enforceAttributes) {
            if (d->attrs())
                memcpy(o->arrayData()->attrs(), d->attrs(), sizeof(PropertyAttributes)*toCopy);
            else
                for (uint i = 0; i < toCopy; ++i)
                    o->arrayData()->attrs()[i] = Attr_Data;
        }

        if (toCopy > d->d()->alloc - offset) {
            uint copyFromStart = toCopy - (d->d()->alloc - offset);
            memcpy(o->arrayData()->d()->arrayData + toCopy - copyFromStart, d->d()->arrayData, sizeof(Value)*copyFromStart);
            toCopy -= copyFromStart;
        }
        memcpy(o->arrayData()->d()->arrayData, d->d()->arrayData + offset, sizeof(Value)*toCopy);
    }

    if (newType != Sparse)
        return;

    SparseArrayData *sparse = static_cast<SparseArrayData *>(o->arrayData());

    uint *lastFree;
    if (d && d->type() == Sparse) {
        SparseArrayData *old = static_cast<SparseArrayData *>(d);
        sparse->setSparse(old->sparse());
        old->setSparse(0);
        sparse->freeList() = old->freeList();
        lastFree = &sparse->freeList();
    } else {
        sparse->setSparse(new SparseArray);
        lastFree = &sparse->freeList();
        for (uint i = 0; i < toCopy; ++i) {
            if (!sparse->arrayData()[i].isEmpty()) {
                SparseArrayNode *n = sparse->sparse()->insert(i);
                n->value = i;
            } else {
                *lastFree = i;
                sparse->arrayData()[i].tag = Value::Empty_Type;
                lastFree = &sparse->arrayData()[i].uint_32;
            }
        }
    }

    if (toCopy < sparse->alloc()) {
        for (uint i = toCopy; i < sparse->alloc(); ++i) {
            *lastFree = i;
            sparse->arrayData()[i].tag = Value::Empty_Type;
            lastFree = &sparse->arrayData()[i].uint_32;
        }
        *lastFree = UINT_MAX;
    }
    // ### Could explicitly free the old data
}

ArrayData *SimpleArrayData::reallocate(Object *o, uint n, bool enforceAttributes)
{
    realloc(o, Simple, n, enforceAttributes);
    return o->arrayData();
}

void ArrayData::ensureAttributes(Object *o)
{
    if (o->arrayData() && o->arrayData()->attrs())
        return;

    ArrayData::realloc(o, Simple, 0, true);
}


void SimpleArrayData::markObjects(Managed *d, ExecutionEngine *e)
{
    SimpleArrayData *dd = static_cast<SimpleArrayData *>(d);
    uint l = dd->len();
    for (uint i = 0; i < l; ++i)
        dd->data(i).mark(e);
}

ReturnedValue SimpleArrayData::get(const ArrayData *d, uint index)
{
    const SimpleArrayData *dd = static_cast<const SimpleArrayData *>(d);
    if (index >= dd->len())
        return Primitive::emptyValue().asReturnedValue();
    return dd->data(index).asReturnedValue();
}

bool SimpleArrayData::put(Object *o, uint index, ValueRef value)
{
    SimpleArrayData *dd = static_cast<SimpleArrayData *>(o->arrayData());
    Q_ASSERT(index >= dd->len() || !dd->attrs() || !dd->attrs()[index].isAccessor());
    // ### honour attributes
    dd->data(index) = value;
    if (index >= dd->len()) {
        if (dd->attrs())
            dd->attrs()[index] = Attr_Data;
        dd->len() = index + 1;
    }
    return true;
}

bool SimpleArrayData::del(Object *o, uint index)
{
    SimpleArrayData *dd = static_cast<SimpleArrayData *>(o->arrayData());
    if (index >= dd->len())
        return true;

    if (!dd->attrs() || dd->attrs()[index].isConfigurable()) {
        dd->data(index) = Primitive::emptyValue();
        if (dd->attrs())
            dd->attrs()[index] = Attr_Data;
        return true;
    }
    if (dd->data(index).isEmpty())
        return true;
    return false;
}

void SimpleArrayData::setAttribute(Object *o, uint index, PropertyAttributes attrs)
{
    o->arrayData()->attrs()[index] = attrs;
}

PropertyAttributes SimpleArrayData::attribute(const ArrayData *d, uint index)
{
    return d->attrs()[index];
}

void SimpleArrayData::push_front(Object *o, Value *values, uint n)
{
    SimpleArrayData *dd = static_cast<SimpleArrayData *>(o->arrayData());
    Q_ASSERT(!dd->attrs());
    if (dd->len() + n > dd->alloc()) {
        realloc(o, Simple, dd->len() + n, false);
        dd = static_cast<SimpleArrayData *>(o->arrayData());
    }
    dd->d()->offset = (dd->d()->offset - n) % dd->d()->alloc;
    dd->len() += n;
    for (uint i = 0; i < n; ++i)
        dd->data(i) = values[i].asReturnedValue();
}

ReturnedValue SimpleArrayData::pop_front(Object *o)
{
    SimpleArrayData *dd = static_cast<SimpleArrayData *>(o->arrayData());
    Q_ASSERT(!dd->attrs());
    if (!dd->len())
        return Encode::undefined();

    ReturnedValue v = dd->data(0).isEmpty() ? Encode::undefined() : dd->data(0).asReturnedValue();
    dd->d()->offset = (dd->d()->offset + 1) % dd->d()->alloc;
    --dd->len();
    return v;
}

uint SimpleArrayData::truncate(Object *o, uint newLen)
{
    SimpleArrayData *dd = static_cast<SimpleArrayData *>(o->arrayData());
    if (dd->len() < newLen)
        return newLen;

    if (!dd->attrs()) {
        dd->len() = newLen;
        return newLen;
    }

    while (dd->len() > newLen) {
        if (!dd->data(dd->len() - 1).isEmpty() && !dd->attrs()[dd->len() - 1].isConfigurable())
            return dd->len();
        --dd->len();
    }
    return dd->len();
}

uint SimpleArrayData::length(const ArrayData *d)
{
    return static_cast<const SimpleArrayData *>(d)->len();
}

bool SimpleArrayData::putArray(Object *o, uint index, Value *values, uint n)
{
    SimpleArrayData *dd = static_cast<SimpleArrayData *>(o->arrayData());
    if (index + n > dd->alloc()) {
        reallocate(o, index + n + 1, false);
        dd = static_cast<SimpleArrayData *>(o->arrayData());
    }
    for (uint i = dd->len(); i < index; ++i)
        dd->data(i) = Primitive::emptyValue();
    for (uint i = 0; i < n; ++i)
        dd->data(index + i) = values[i];
    dd->len() = qMax(dd->len(), index + n);
    return true;
}

void SparseArrayData::free(ArrayData *d, uint idx)
{
    Q_ASSERT(d && d->type() == ArrayData::Sparse);
    SparseArrayData *dd = static_cast<SparseArrayData *>(d);
    Value *v = dd->arrayData() + idx;
    if (dd->attrs() && dd->attrs()[idx].isAccessor()) {
        // double slot, free both. Order is important, so we have a double slot for allocation again afterwards.
        v[1].tag = Value::Empty_Type;
        v[1].uint_32 = dd->freeList();
        v[0].tag = Value::Empty_Type;
        v[0].uint_32 = idx + 1;
    } else {
        v->tag = Value::Empty_Type;
        v->uint_32 = dd->freeList();
    }
    dd->freeList() = idx;
    if (dd->attrs())
        dd->attrs()[idx].clear();
}


void SparseArrayData::destroy(Managed *d)
{
    SparseArrayData *dd = static_cast<SparseArrayData *>(d);
    delete dd->sparse();
}

void SparseArrayData::markObjects(Managed *d, ExecutionEngine *e)
{
    SparseArrayData *dd = static_cast<SparseArrayData *>(d);
    uint l = dd->alloc();
    for (uint i = 0; i < l; ++i)
        dd->arrayData()[i].mark(e);
}

ArrayData *SparseArrayData::reallocate(Object *o, uint n, bool enforceAttributes)
{
    realloc(o, Sparse, n, enforceAttributes);
    return o->arrayData();
}

// double slots are required for accessor properties
uint SparseArrayData::allocate(Object *o, bool doubleSlot)
{
    Q_ASSERT(o->arrayData()->type() == ArrayData::Sparse);
    SparseArrayData *dd = static_cast<SparseArrayData *>(o->arrayData());
    if (doubleSlot) {
        uint *last = &dd->freeList();
        while (1) {
            if (*last == UINT_MAX) {
                reallocate(o, o->arrayData()->alloc() + 2, true);
                dd = static_cast<SparseArrayData *>(o->arrayData());
                last = &dd->freeList();
                Q_ASSERT(*last != UINT_MAX);
            }

            Q_ASSERT(dd->arrayData()[*last].uint_32 != *last);
            if (dd->arrayData()[*last].uint_32 == (*last + 1)) {
                // found two slots in a row
                uint idx = *last;
                *last = dd->arrayData()[*last + 1].uint_32;
                o->arrayData()->attrs()[idx] = Attr_Accessor;
                return idx;
            }
            last = &dd->arrayData()[*last].uint_32;
        }
    } else {
        if (dd->freeList() == UINT_MAX) {
            reallocate(o, o->arrayData()->alloc() + 1, false);
            dd = static_cast<SparseArrayData *>(o->arrayData());
        }
        uint idx = dd->freeList();
        Q_ASSERT(idx != UINT_MAX);
        dd->freeList() = dd->arrayData()[idx].uint_32;
        if (dd->attrs())
            dd->attrs()[idx] = Attr_Data;
        return idx;
    }
}

ReturnedValue SparseArrayData::get(const ArrayData *d, uint index)
{
    const SparseArrayData *s = static_cast<const SparseArrayData *>(d);
    index = s->mappedIndex(index);
    if (index == UINT_MAX)
        return Primitive::emptyValue().asReturnedValue();
    return s->arrayData()[index].asReturnedValue();
}

bool SparseArrayData::put(Object *o, uint index, ValueRef value)
{
    if (value->isEmpty())
        return true;

    SparseArrayData *s = static_cast<SparseArrayData *>(o->arrayData());
    SparseArrayNode *n = s->sparse()->insert(index);
    Q_ASSERT(n->value == UINT_MAX || !o->arrayData()->attrs() || !o->arrayData()->attrs()[n->value].isAccessor());
    if (n->value == UINT_MAX)
        n->value = allocate(o);
    s->arrayData()[n->value] = value;
    if (s->attrs())
        s->attrs()[n->value] = Attr_Data;
    return true;
}

bool SparseArrayData::del(Object *o, uint index)
{
    SparseArrayData *dd = static_cast<SparseArrayData *>(o->arrayData());

    SparseArrayNode *n = dd->sparse()->findNode(index);
    if (!n)
        return true;

    uint pidx = n->value;
    Q_ASSERT(!dd->arrayData()[pidx].isEmpty());

    bool isAccessor = false;
    if (dd->attrs()) {
        if (!dd->attrs()[pidx].isConfigurable())
            return false;

        isAccessor = dd->attrs()[pidx].isAccessor();
        dd->attrs()[pidx] = Attr_Data;
    }

    if (isAccessor) {
        // free up both indices
        dd->arrayData()[pidx + 1].tag = Value::Empty_Type;
        dd->arrayData()[pidx + 1].uint_32 = static_cast<SparseArrayData *>(dd)->freeList();
        dd->arrayData()[pidx].tag = Value::Undefined_Type;
        dd->arrayData()[pidx].uint_32 = pidx + 1;
    } else {
        dd->arrayData()[pidx].tag = Value::Empty_Type;
        dd->arrayData()[pidx].uint_32 = static_cast<SparseArrayData *>(dd)->freeList();
    }

    dd->freeList() = pidx;
    dd->sparse()->erase(n);
    return true;
}

void SparseArrayData::setAttribute(Object *o, uint index, PropertyAttributes attrs)
{
    SparseArrayData *d = static_cast<SparseArrayData *>(o->arrayData());
    SparseArrayNode *n = d->sparse()->insert(index);
    if (n->value == UINT_MAX) {
        n->value = allocate(o, attrs.isAccessor());
        d = static_cast<SparseArrayData *>(o->arrayData());
    }
    else if (attrs.isAccessor() != d->attrs()[n->value].isAccessor()) {
        // need to convert the slot
        free(d, n->value);
        n->value = allocate(o, attrs.isAccessor());
    }
    o->arrayData()->attrs()[n->value] = attrs;
}

PropertyAttributes SparseArrayData::attribute(const ArrayData *d, uint index)
{
    SparseArrayNode *n = static_cast<const SparseArrayData *>(d)->sparse()->insert(index);
    if (!n)
        return PropertyAttributes();
    return d->attrs()[n->value];
}

void SparseArrayData::push_front(Object *o, Value *values, uint n)
{
    SparseArrayData *d = static_cast<SparseArrayData *>(o->arrayData());
    Q_ASSERT(!o->arrayData()->attrs());
    for (int i = n - 1; i >= 0; --i) {
        uint idx = allocate(o);
        d->arrayData()[idx] = values[i];
        d->sparse()->push_front(idx);
    }
}

ReturnedValue SparseArrayData::pop_front(Object *o)
{
    SparseArrayData *d = static_cast<SparseArrayData *>(o->arrayData());
    Q_ASSERT(!o->arrayData()->attrs());
    uint idx = d->sparse()->pop_front();
    ReturnedValue v;
    if (idx != UINT_MAX) {
        v = d->arrayData()[idx].asReturnedValue();
        free(o->arrayData(), idx);
    } else {
        v = Encode::undefined();
    }
    return v;
}

uint SparseArrayData::truncate(Object *o, uint newLen)
{
    SparseArrayData *d = static_cast<SparseArrayData *>(o->arrayData());
    SparseArrayNode *begin = d->sparse()->lowerBound(newLen);
    if (begin != d->sparse()->end()) {
        SparseArrayNode *it = d->sparse()->end()->previousNode();
        while (1) {
            if (d->attrs()) {
                if (!d->attrs()[it->value].isConfigurable()) {
                    newLen = it->key() + 1;
                    break;
                }
            }
            free(d, it->value);
            bool brk = (it == begin);
            SparseArrayNode *prev = it->previousNode();
            static_cast<SparseArrayData *>(d)->sparse()->erase(it);
            if (brk)
                break;
            it = prev;
        }
    }
    return newLen;
}

uint SparseArrayData::length(const ArrayData *d)
{
    const SparseArrayData *dd = static_cast<const SparseArrayData *>(d);
    if (!dd->sparse())
        return 0;
    SparseArrayNode *n = dd->sparse()->end();
    n = n->previousNode();
    return n ? n->key() + 1 : 0;
}

bool SparseArrayData::putArray(Object *o, uint index, Value *values, uint n)
{
    for (uint i = 0; i < n; ++i)
        put(o, index + i, values[i]);
    return true;
}


uint ArrayData::append(Object *obj, ArrayObject *otherObj, uint n)
{
    Q_ASSERT(!obj->arrayData() || !obj->arrayData()->hasAttributes());

    if (!n)
        return obj->getLength();

    ArrayData *other = otherObj->arrayData();

    if (other && other->isSparse())
        obj->initSparseArray();
    else
        obj->arrayCreate();

    uint oldSize = obj->getLength();

    if (other && other->isSparse()) {
        SparseArrayData *os = static_cast<SparseArrayData *>(other);
        if (otherObj->hasAccessorProperty() && other->hasAttributes()) {
            Scope scope(obj->engine());
            ScopedValue v(scope);
            for (const SparseArrayNode *it = os->sparse()->begin();
                 it != os->sparse()->end(); it = it->nextNode()) {
                v = otherObj->getValue(reinterpret_cast<Property *>(os->arrayData() + it->value), other->attrs()[it->value]);
                obj->arraySet(oldSize + it->key(), v);
            }
        } else {
            for (const SparseArrayNode *it = static_cast<const SparseArrayData *>(other)->sparse()->begin();
                 it != os->sparse()->end(); it = it->nextNode())
                obj->arraySet(oldSize + it->key(), ValueRef(os->arrayData()[it->value]));
        }
    } else {
        SimpleArrayData *os = static_cast<SimpleArrayData *>(other);
        uint toCopy = n;
        uint chunk = toCopy;
        if (chunk > os->alloc() - os->d()->offset)
            chunk -= os->alloc() - os->d()->offset;
        obj->arrayPut(oldSize, os->arrayData() + os->d()->offset, chunk);
        toCopy -= chunk;
        if (toCopy)
            obj->arrayPut(oldSize + chunk, os->arrayData(), toCopy);
    }

    return oldSize + n;
}

Property *ArrayData::insert(Object *o, uint index, bool isAccessor)
{
    if (!isAccessor && o->arrayData()->type() != ArrayData::Sparse) {
        SimpleArrayData *d = static_cast<SimpleArrayData *>(o->arrayData());
        if (index < 0x1000 || index < d->len() + (d->len() >> 2)) {
            if (index >= o->arrayData()->alloc()) {
                o->arrayReserve(index + 1);
                d = static_cast<SimpleArrayData *>(o->arrayData());
            }
            if (index >= d->len()) {
                // mark possible hole in the array
                for (uint i = d->len(); i < index; ++i)
                    d->data(i) = Primitive::emptyValue();
                d->len() = index + 1;
            }
            return reinterpret_cast<Property *>(d->d()->arrayData + d->mappedIndex(index));
        }
    }

    o->initSparseArray();
    SparseArrayData *s = static_cast<SparseArrayData *>(o->arrayData());
    SparseArrayNode *n = s->sparse()->insert(index);
    if (n->value == UINT_MAX) {
        n->value = SparseArrayData::allocate(o, isAccessor);
        s = static_cast<SparseArrayData *>(o->arrayData());
    }
    return reinterpret_cast<Property *>(s->arrayData() + n->value);
}


class ArrayElementLessThan
{
public:
    inline ArrayElementLessThan(ExecutionContext *context, Object *thisObject, const ValueRef comparefn)
        : m_context(context), thisObject(thisObject), m_comparefn(comparefn) {}

    bool operator()(Value v1, Value v2) const;

private:
    ExecutionContext *m_context;
    Object *thisObject;
    const ValueRef m_comparefn;
};


bool ArrayElementLessThan::operator()(Value v1, Value v2) const
{
    Scope scope(m_context);

    if (v1.isUndefined() || v1.isEmpty())
        return false;
    if (v2.isUndefined() || v2.isEmpty())
        return true;
    ScopedObject o(scope, m_comparefn);
    if (o) {
        Scope scope(o->engine());
        ScopedValue result(scope);
        ScopedCallData callData(scope, 2);
        callData->thisObject = Primitive::undefinedValue();
        callData->args[0] = v1;
        callData->args[1] = v2;
        result = Runtime::callValue(m_context, m_comparefn, callData);

        return result->toNumber() < 0;
    }
    ScopedString p1s(scope, v1.toString(m_context));
    ScopedString p2s(scope, v2.toString(m_context));
    return p1s->toQString() < p2s->toQString();
}

template <typename RandomAccessIterator, typename T, typename LessThan>
void sortHelper(RandomAccessIterator start, RandomAccessIterator end, const T &t, LessThan lessThan)
{
top:
    int span = int(end - start);
    if (span < 2)
        return;

    --end;
    RandomAccessIterator low = start, high = end - 1;
    RandomAccessIterator pivot = start + span / 2;

    if (lessThan(*end, *start))
        qSwap(*end, *start);
    if (span == 2)
        return;

    if (lessThan(*pivot, *start))
        qSwap(*pivot, *start);
    if (lessThan(*end, *pivot))
        qSwap(*end, *pivot);
    if (span == 3)
        return;

    qSwap(*pivot, *end);

    while (low < high) {
        while (low < high && lessThan(*low, *end))
            ++low;

        while (high > low && lessThan(*end, *high))
            --high;

        if (low < high) {
            qSwap(*low, *high);
            ++low;
            --high;
        } else {
            break;
        }
    }

    if (lessThan(*low, *end))
        ++low;

    qSwap(*end, *low);
    sortHelper(start, low, t, lessThan);

    start = low + 1;
    ++end;
    goto top;
}


void ArrayData::sort(ExecutionContext *context, Object *thisObject, const ValueRef comparefn, uint len)
{
    if (!len)
        return;

    if (!thisObject->arrayData() || !thisObject->arrayData()->length())
        return;

    if (!(comparefn->isUndefined() || comparefn->asObject())) {
        context->throwTypeError();
        return;
    }

    // The spec says the sorting goes through a series of get,put and delete operations.
    // this implies that the attributes don't get sorted around.

    if (thisObject->arrayData()->type() == ArrayData::Sparse) {
        // since we sort anyway, we can simply iterate over the entries in the sparse
        // array and append them one by one to a regular one.
        SparseArrayData *sparse = static_cast<SparseArrayData *>(thisObject->arrayData());

        if (!sparse->sparse()->nEntries())
            return;

        thisObject->setArrayData(0);
        ArrayData::realloc(thisObject, ArrayData::Simple, sparse->sparse()->nEntries(), sparse->attrs() ? true : false);
        SimpleArrayData *d = static_cast<SimpleArrayData *>(thisObject->arrayData());

        SparseArrayNode *n = sparse->sparse()->begin();
        uint i = 0;
        if (sparse->attrs()) {
            while (n != sparse->sparse()->end()) {
                if (n->value >= len)
                    break;

                PropertyAttributes a = sparse->attrs() ? sparse->attrs()[n->value] : Attr_Data;
                d->data(i) = thisObject->getValue(reinterpret_cast<Property *>(sparse->arrayData() + n->value), a);
                d->attrs()[i] = a.isAccessor() ? Attr_Data : a;

                n = n->nextNode();
                ++i;
            }
        } else {
            while (n != sparse->sparse()->end()) {
                if (n->value >= len)
                    break;
                d->data(i) = sparse->arrayData()[n->value];
                n = n->nextNode();
                ++i;
            }
        }
        d->len() = i;
        if (len > i)
            len = i;
        if (n != sparse->sparse()->end()) {
            // have some entries outside the sort range that we need to ignore when sorting
            thisObject->initSparseArray();
            while (n != sparse->sparse()->end()) {
                PropertyAttributes a = sparse->attrs() ? sparse->attrs()[n->value] : Attr_Data;
                thisObject->arraySet(n->value, *reinterpret_cast<Property *>(sparse->arrayData() + n->value), a);

                n = n->nextNode();
            }

        }
        // ### explicitly delete sparse
    } else {
        SimpleArrayData *d = static_cast<SimpleArrayData *>(thisObject->arrayData());
        if (len > d->len())
            len = d->len();

        // sort empty values to the end
        for (uint i = 0; i < len; i++) {
            if (d->data(i).isEmpty()) {
                while (--len > i)
                    if (!d->data(len).isEmpty())
                        break;
                Q_ASSERT(!d->attrs() || !d->attrs()[len].isAccessor());
                d->data(i) = d->data(len);
                d->data(len) = Primitive::emptyValue();
            }
        }

        if (!len)
            return;
    }


    ArrayElementLessThan lessThan(context, thisObject, comparefn);

    Value *begin = thisObject->arrayData()->d()->arrayData;
    sortHelper(begin, begin + len, *begin, lessThan);

#ifdef CHECK_SPARSE_ARRAYS
    thisObject->initSparseArray();
#endif

}
