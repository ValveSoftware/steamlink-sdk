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

#include "qv4persistent_p.h"
#include "qv4mm_p.h"

using namespace QV4;

PersistentValue::PersistentValue(const ValueRef val)
    : d(new PersistentValuePrivate(val.asReturnedValue()))
{
}

PersistentValue::PersistentValue(ReturnedValue val)
    : d(new PersistentValuePrivate(val))
{
}

PersistentValue::PersistentValue(const PersistentValue &other)
    : d(other.d)
{
    if (d)
        d->ref();
}

PersistentValue &PersistentValue::operator=(const PersistentValue &other)
{
    if (d == other.d)
        return *this;

    // the memory manager cleans up those with a refcount of 0

    if (d)
        d->deref();
    d = other.d;
    if (d)
        d->ref();

    return *this;
}

PersistentValue &PersistentValue::operator =(const ValueRef other)
{
    if (!d) {
        d = new PersistentValuePrivate(other.asReturnedValue());
        return *this;
    }
    d = d->detach(other.asReturnedValue());
    return *this;
}

PersistentValue &PersistentValue::operator =(ReturnedValue other)
{
    if (!d) {
        d = new PersistentValuePrivate(other);
        return *this;
    }
    d = d->detach(other);
    return *this;
}

PersistentValue::~PersistentValue()
{
    if (d)
        d->deref();
}

WeakValue::WeakValue(const ValueRef val)
    : d(new PersistentValuePrivate(val.asReturnedValue(), /*engine*/0, /*weak*/true))
{
}

WeakValue::WeakValue(const WeakValue &other)
    : d(other.d)
{
    if (d)
        d->ref();
}

WeakValue::WeakValue(ReturnedValue val)
    : d(new PersistentValuePrivate(val, /*engine*/0, /*weak*/true))
{
}

WeakValue &WeakValue::operator=(const WeakValue &other)
{
    if (d == other.d)
        return *this;

    // the memory manager cleans up those with a refcount of 0

    if (d)
        d->deref();
    d = other.d;
    if (d)
        d->ref();

    return *this;
}

WeakValue &WeakValue::operator =(const ValueRef other)
{
    if (!d) {
        d = new PersistentValuePrivate(other.asReturnedValue(), /*engine*/0, /*weak*/true);
        return *this;
    }
    d = d->detach(other.asReturnedValue(), /*weak*/true);
    return *this;
}

WeakValue &WeakValue::operator =(const ReturnedValue &other)
{
    if (!d) {
        d = new PersistentValuePrivate(other, /*engine*/0, /*weak*/true);
        return *this;
    }
    d = d->detach(other, /*weak*/true);
    return *this;
}


WeakValue::~WeakValue()
{
    if (d)
        d->deref();
}

void WeakValue::markOnce(ExecutionEngine *e)
{
    if (!d)
        return;
    d->value.mark(e);
}

PersistentValuePrivate::PersistentValuePrivate(ReturnedValue v, ExecutionEngine *e, bool weak)
    : refcount(1)
    , weak(weak)
    , engine(e)
    , prev(0)
    , next(0)
{
    value.val = v;
    init();
}

void PersistentValuePrivate::init()
{
    if (!engine) {
        Managed *m = value.asManaged();
        if (!m)
            return;

        engine = m->engine();
    }
    if (engine && !prev) {
        PersistentValuePrivate **listRoot = weak ? &engine->memoryManager->m_weakValues : &engine->memoryManager->m_persistentValues;

        prev = listRoot;
        next = *listRoot;
        *prev = this;
        if (next)
            next->prev = &this->next;
    }
}

PersistentValuePrivate::~PersistentValuePrivate()
{
}

void PersistentValuePrivate::removeFromList()
{
    if (prev) {
        if (next)
            next->prev = prev;
        *prev = next;
        next = 0;
        prev = 0;
    }
}

void PersistentValuePrivate::deref()
{
    // if engine is not 0, they are registered with the memory manager
    // and will get cleaned up in the next gc run
    if (!--refcount) {
        removeFromList();
        delete this;
    }
}

PersistentValuePrivate *PersistentValuePrivate::detach(const QV4::ReturnedValue val, bool weak)
{
    if (refcount == 1) {
        value.val = val;

        Managed *m = value.asManaged();
        if (!prev) {
            if (m) {
                ExecutionEngine *engine = m->engine();
                if (engine) {
                    PersistentValuePrivate **listRoot = weak ? &engine->memoryManager->m_weakValues : &engine->memoryManager->m_persistentValues;
                    prev = listRoot;
                    next = *listRoot;
                    *prev = this;
                    if (next)
                        next->prev = &this->next;
                }
            }
        } else if (!m)
            removeFromList();

        return this;
    }
    --refcount;
    return new PersistentValuePrivate(val, engine, weak);
}

