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
#include "qv4identifier_p.h"
#include "qv4identifiertable_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

static const uchar prime_deltas[] = {
    0,  0,  1,  3,  1,  5,  3,  3,  1,  9,  7,  5,  3,  9, 25,  3,
    1, 21,  3, 21,  7, 15,  9,  5,  3, 29, 15,  0,  0,  0,  0,  0
};

static inline int primeForNumBits(int numBits)
{
    return (1 << numBits) + prime_deltas[numBits];
}


IdentifierHashData::IdentifierHashData(int numBits)
    : size(0)
    , numBits(numBits)
{
    refCount.store(1);
    alloc = primeForNumBits(numBits);
    entries = (IdentifierHashEntry *)malloc(alloc*sizeof(IdentifierHashEntry));
    memset(entries, 0, alloc*sizeof(IdentifierHashEntry));
}

IdentifierHashBase::IdentifierHashBase(ExecutionEngine *engine)
{
    d = new IdentifierHashData(3);
    d->identifierTable = engine->identifierTable;
}


IdentifierHashEntry *IdentifierHashBase::addEntry(const Identifier *identifier)
{
    // fill up to max 50%
    bool grow = (d->alloc <= d->size*2);

    if (grow) {
        ++d->numBits;
        int newAlloc = primeForNumBits(d->numBits);
        IdentifierHashEntry *newEntries = (IdentifierHashEntry *)malloc(newAlloc * sizeof(IdentifierHashEntry));
        memset(newEntries, 0, newAlloc*sizeof(IdentifierHashEntry));
        for (int i = 0; i < d->alloc; ++i) {
            const IdentifierHashEntry &e = d->entries[i];
            if (!e.identifier)
                continue;
            uint idx = e.identifier->hashValue % newAlloc;
            while (newEntries[idx].identifier) {
                ++idx;
                idx %= newAlloc;
            }
            newEntries[idx] = e;
        }
        free(d->entries);
        d->entries = newEntries;
        d->alloc = newAlloc;
    }

    uint idx = identifier->hashValue % d->alloc;
    while (d->entries[idx].identifier) {
        Q_ASSERT(d->entries[idx].identifier != identifier);
        ++idx;
        idx %= d->alloc;
    }
    d->entries[idx].identifier = identifier;
    ++d->size;
    return d->entries + idx;
}

const IdentifierHashEntry *IdentifierHashBase::lookup(const Identifier *identifier) const
{
    if (!d)
        return 0;
    Q_ASSERT(d->entries);

    uint idx = identifier->hashValue % d->alloc;
    while (1) {
        if (!d->entries[idx].identifier)
            return 0;
        if (d->entries[idx].identifier == identifier)
            return d->entries + idx;
        ++idx;
        idx %= d->alloc;
    }
}

const IdentifierHashEntry *IdentifierHashBase::lookup(const QString &str) const
{
    if (!d)
        return 0;
    Q_ASSERT(d->entries);

    uint hash = String::createHashValue(str.constData(), str.length());
    uint idx = hash % d->alloc;
    while (1) {
        if (!d->entries[idx].identifier)
            return 0;
        if (d->entries[idx].identifier->string == str)
            return d->entries + idx;
        ++idx;
        idx %= d->alloc;
    }
}

const IdentifierHashEntry *IdentifierHashBase::lookup(String *str) const
{
    if (!d)
        return 0;
    if (str->d()->identifier)
        return lookup(str->d()->identifier);
    return lookup(str->toQString());
}

const Identifier *IdentifierHashBase::toIdentifier(const QString &str) const
{
    Q_ASSERT(d);
    return d->identifierTable->identifier(str);
}

const Identifier *IdentifierHashBase::toIdentifier(String *str) const
{
    Q_ASSERT(d);
    return d->identifierTable->identifier(str);
}


}

QT_END_NAMESPACE
