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


IdentifierTable::IdentifierTable(ExecutionEngine *engine)
    : engine(engine)
    , size(0)
    , numBits(8)
{
    alloc = primeForNumBits(numBits);
    entries = (String **)malloc(alloc*sizeof(String *));
    memset(entries, 0, alloc*sizeof(String *));
}

IdentifierTable::~IdentifierTable()
{
    for (int i = 0; i < alloc; ++i)
        if (entries[i])
            delete entries[i]->d()->identifier;
    free(entries);
}

void IdentifierTable::addEntry(String *str)
{
    uint hash = str->hashValue();

    if (str->subtype() == String::StringType_ArrayIndex)
        return;

    str->d()->identifier = new Identifier;
    str->d()->identifier->string = str->toQString();
    str->d()->identifier->hashValue = hash;

    bool grow = (alloc <= size*2);

    if (grow) {
        ++numBits;
        int newAlloc = primeForNumBits(numBits);
        String **newEntries = (String **)malloc(newAlloc*sizeof(String *));
        memset(newEntries, 0, newAlloc*sizeof(String *));
        for (int i = 0; i < alloc; ++i) {
            String *e = entries[i];
            if (!e)
                continue;
            uint idx = e->d()->stringHash % newAlloc;
            while (newEntries[idx]) {
                ++idx;
                idx %= newAlloc;
            }
            newEntries[idx] = e;
        }
        free(entries);
        entries = newEntries;
        alloc = newAlloc;
    }

    uint idx = hash % alloc;
    while (entries[idx]) {
        ++idx;
        idx %= alloc;
    }
    entries[idx] = str;
    ++size;
}



String *IdentifierTable::insertString(const QString &s)
{
    uint hash = String::createHashValue(s.constData(), s.length());
    uint idx = hash % alloc;
    while (String *e = entries[idx]) {
        if (e->d()->stringHash == hash && e->toQString() == s)
            return e;
        ++idx;
        idx %= alloc;
    }

    String *str = engine->newString(s)->getPointer();
    addEntry(str);
    return str;
}


Identifier *IdentifierTable::identifierImpl(const String *str)
{
    if (str->d()->identifier)
        return str->d()->identifier;
    uint hash = str->hashValue();
    if (str->subtype() == String::StringType_ArrayIndex)
        return 0;

    uint idx = hash % alloc;
    while (String *e = entries[idx]) {
        if (e->d()->stringHash == hash && e->isEqualTo(str)) {
            str->d()->identifier = e->d()->identifier;
            return e->d()->identifier;
        }
        ++idx;
        idx %= alloc;
    }

    addEntry(const_cast<QV4::String *>(str));
    return str->d()->identifier;
}

Identifier *IdentifierTable::identifier(const QString &s)
{
    return insertString(s)->d()->identifier;
}

Identifier *IdentifierTable::identifier(const char *s, int len)
{
    uint hash = String::createHashValue(s, len);
    if (hash == UINT_MAX)
        return identifier(QString::fromUtf8(s, len));

    QLatin1String latin(s, len);
    uint idx = hash % alloc;
    while (String *e = entries[idx]) {
        if (e->d()->stringHash == hash && e->toQString() == latin)
            return e->d()->identifier;
        ++idx;
        idx %= alloc;
    }

    String *str = engine->newString(QString::fromLatin1(s, len))->getPointer();
    addEntry(str);
    return str->d()->identifier;
}

}

QT_END_NAMESPACE
