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

#include "qv4persistent_p.h"
#include <private/qv4mm_p.h>
#include "qv4object_p.h"
#include "qv4qobjectwrapper_p.h"
#include "PageAllocation.h"

using namespace QV4;

namespace {

struct Page;
struct Header {
    WTF::PageAllocation alloc;
    ExecutionEngine *engine;
    Page **prev;
    Page *next;
    int refCount;
    int freeList;
};

static const int kEntriesPerPage = int((WTF::pageSize() - sizeof(Header)) / sizeof(Value));

struct Page {
    Header header;
    Value values[1]; // Really kEntriesPerPage, but keep the compiler happy
};

Page *getPage(Value *val) {
   return reinterpret_cast<Page *>(reinterpret_cast<quintptr>(val) & ~((quintptr)(WTF::pageSize() - 1)));
}


Page *allocatePage(PersistentValueStorage *storage)
{
    PageAllocation page = WTF::PageAllocation::allocate(WTF::pageSize());
    Page *p = reinterpret_cast<Page *>(page.base());

    Q_ASSERT(!((quintptr)p & (WTF::pageSize() - 1)));

    p->header.engine = storage->engine;
    p->header.alloc = page;
    p->header.next = reinterpret_cast<Page *>(storage->firstPage);
    p->header.prev = reinterpret_cast<Page **>(&storage->firstPage);
    p->header.refCount = 0;
    p->header.freeList = 0;
    if (p->header.next)
        p->header.next->header.prev = &p->header.next;
    for (int i = 0; i < kEntriesPerPage - 1; ++i) {
        p->values[i].setEmpty(i + 1);
    }
    p->values[kEntriesPerPage - 1].setEmpty(-1);

    storage->firstPage = p;

    return p;
}


}


PersistentValueStorage::Iterator::Iterator(void *p, int idx)
    : p(p), index(idx)
{
    Page *page = static_cast<Page *>(p);
    if (page)
        ++page->header.refCount;
}

PersistentValueStorage::Iterator::Iterator(const PersistentValueStorage::Iterator &o)
    : p(o.p), index(o.index)
{
    Page *page = static_cast<Page *>(p);
    if (page)
        ++page->header.refCount;
}

PersistentValueStorage::Iterator &PersistentValueStorage::Iterator::operator=(const PersistentValueStorage::Iterator &o)
{
    Page *page = static_cast<Page *>(p);
    if (page && !--page->header.refCount)
        freePage(p);
    p = o.p;
    index = o.index;
    page = static_cast<Page *>(p);
    if (page)
        ++page->header.refCount;

    return *this;
}

PersistentValueStorage::Iterator::~Iterator()
{
    Page *page = static_cast<Page *>(p);
    if (page && !--page->header.refCount)
        freePage(page);
}

PersistentValueStorage::Iterator &PersistentValueStorage::Iterator::operator++() {
    while (p) {
        while (index < kEntriesPerPage - 1) {
            ++index;
            if (!static_cast<Page *>(p)->values[index].isEmpty())
                return *this;
        }
        index = -1;
        Page *next = static_cast<Page *>(p)->header.next;
        if (!--static_cast<Page *>(p)->header.refCount)
            freePage(p);
        p = next;
        if (next)
            ++next->header.refCount;
    }
    index = 0;
    return *this;
}

Value &PersistentValueStorage::Iterator::operator *()
{
    return static_cast<Page *>(p)->values[index];
}

PersistentValueStorage::PersistentValueStorage(ExecutionEngine *engine)
    : engine(engine),
      firstPage(0)
{
}

PersistentValueStorage::~PersistentValueStorage()
{
    Page *p = static_cast<Page *>(firstPage);
    while (p) {
        for (int i = 0; i < kEntriesPerPage; ++i) {
            if (!p->values[i].isEmpty())
                p->values[i] = Encode::undefined();
        }
        Page *n = p->header.next;
        p->header.engine = 0;
        p->header.prev = 0;
        p->header.next = 0;
        Q_ASSERT(p->header.refCount);
        p = n;
    }
}

Value *PersistentValueStorage::allocate()
{
    Page *p = static_cast<Page *>(firstPage);
    while (p) {
        if (p->header.freeList != -1)
            break;
        p = p->header.next;
    }
    if (!p)
        p = allocatePage(this);

    Value *v = p->values + p->header.freeList;
    p->header.freeList = v->int_32();
    ++p->header.refCount;

    v->setRawValue(Encode::undefined());

    return v;
}

void PersistentValueStorage::free(Value *v)
{
    if (!v)
        return;

    Page *p = getPage(v);

    v->setEmpty(p->header.freeList);
    p->header.freeList = v - p->values;
    if (!--p->header.refCount)
        freePage(p);
}

static void drainMarkStack(QV4::ExecutionEngine *engine, Value *markBase)
{
    while (engine->jsStackTop > markBase) {
        Heap::Base *h = engine->popForGC();
        Q_ASSERT (h->vtable()->markObjects);
        h->vtable()->markObjects(h, engine);
    }
}

void PersistentValueStorage::mark(ExecutionEngine *e)
{
    Value *markBase = e->jsStackTop;

    Page *p = static_cast<Page *>(firstPage);
    while (p) {
        for (int i = 0; i < kEntriesPerPage; ++i) {
            if (Managed *m = p->values[i].as<Managed>())
                m->mark(e);
        }
        drainMarkStack(e, markBase);

        p = p->header.next;
    }
}

ExecutionEngine *PersistentValueStorage::getEngine(Value *v)
{
    return getPage(v)->header.engine;
}

void PersistentValueStorage::freePage(void *page)
{
    Page *p = static_cast<Page *>(page);
    if (p->header.prev)
        *p->header.prev = p->header.next;
    if (p->header.next)
        p->header.next->header.prev = p->header.prev;
    p->header.alloc.deallocate();
}


PersistentValue::PersistentValue(const PersistentValue &other)
    : val(0)
{
    if (other.val) {
        val = other.engine()->memoryManager->m_persistentValues->allocate();
        *val = *other.val;
    }
}

PersistentValue::PersistentValue(ExecutionEngine *engine, const Value &value)
{
    val = engine->memoryManager->m_persistentValues->allocate();
    *val = value;
}

PersistentValue::PersistentValue(ExecutionEngine *engine, ReturnedValue value)
{
    val = engine->memoryManager->m_persistentValues->allocate();
    *val = value;
}

PersistentValue::PersistentValue(ExecutionEngine *engine, Object *object)
    : val(0)
{
    if (!object)
        return;

    val = engine->memoryManager->m_persistentValues->allocate();
    *val = object;
}

PersistentValue::~PersistentValue()
{
    PersistentValueStorage::free(val);
}

PersistentValue &PersistentValue::operator=(const PersistentValue &other)
{
    if (!val) {
        if (!other.val)
            return *this;
        val = other.engine()->memoryManager->m_persistentValues->allocate();
    }

    Q_ASSERT(engine() == other.engine());

    *val = *other.val;
    return *this;
}

PersistentValue &PersistentValue::operator=(const WeakValue &other)
{
    if (!val) {
        if (!other.valueRef())
            return *this;
        val = other.engine()->memoryManager->m_persistentValues->allocate();
    }

    Q_ASSERT(engine() == other.engine());

    *val = *other.valueRef();
    return *this;
}

PersistentValue &PersistentValue::operator=(Object *object)
{
    if (!object) {
        PersistentValueStorage::free(val);
        return *this;
    }
    if (!val)
        val = object->engine()->memoryManager->m_persistentValues->allocate();

    *val = object;
    return *this;
}

void PersistentValue::set(ExecutionEngine *engine, const Value &value)
{
    if (!val)
        val = engine->memoryManager->m_persistentValues->allocate();
    *val = value;
}

void PersistentValue::set(ExecutionEngine *engine, ReturnedValue value)
{
    if (!val)
        val = engine->memoryManager->m_persistentValues->allocate();
    *val = value;
}

void PersistentValue::set(ExecutionEngine *engine, Heap::Base *obj)
{
    if (!val)
        val = engine->memoryManager->m_persistentValues->allocate();
    *val = obj;
}

WeakValue::WeakValue(const WeakValue &other)
    : val(0)
{
    if (other.val) {
        allocVal(other.engine());
        *val = *other.val;
    }
}

WeakValue::WeakValue(ExecutionEngine *engine, const Value &value)
{
    allocVal(engine);
    *val = value;
}

WeakValue &WeakValue::operator=(const WeakValue &other)
{
    if (!val) {
        if (!other.val)
            return *this;
        allocVal(other.engine());
    }

    Q_ASSERT(engine() == other.engine());

    *val = *other.val;
    return *this;
}

WeakValue::~WeakValue()
{
    free();
}

void WeakValue::allocVal(ExecutionEngine *engine)
{
    val = engine->memoryManager->m_weakValues->allocate();
}

void WeakValue::markOnce(ExecutionEngine *e)
{
    if (!val)
        return;
    val->mark(e);
}

void WeakValue::free()
{
    if (!val)
        return;

    ExecutionEngine *e = engine();
    if (e && val->as<QObjectWrapper>()) {
        // Some QV4::QObjectWrapper Value will be freed in WeakValue::~WeakValue() before MemoryManager::sweep() is being called,
        // in this case we will never have a chance to call detroyObject() on those QV4::QObjectWrapper objects.
        // Here we don't free these Value immediately, instead we keep track of them to free them later in MemoryManager::sweep()
        e->memoryManager->m_pendingFreedObjectWrapperValue.push_back(val);
    } else {
        PersistentValueStorage::free(val);
    }

    val = 0;
}

