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
#ifndef QV4STRING_H
#define QV4STRING_H

#include <QtCore/qstring.h>
#include "qv4managed_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ExecutionEngine;
struct Identifier;

struct Q_QML_PRIVATE_EXPORT String : public Managed {
#ifndef V4_BOOTSTRAP
    struct Q_QML_PRIVATE_EXPORT Data : Managed::Data {
        Data(ExecutionEngine *engine, const QString &text);
        Data(ExecutionEngine *engine, String *l, String *n);
        ~Data() {
            if (!largestSubLength && !text->ref.deref())
                QStringData::deallocate(text);
        }
        void simplifyString() const;
        int length() const {
            Q_ASSERT((largestSubLength &&
                      (len == left->d()->len + right->d()->len)) ||
                     len == (uint)text->size);
            return len;
        }
        union {
            mutable QStringData *text;
            mutable String *left;
        };
        union {
            mutable Identifier *identifier;
            mutable String *right;
        };
        mutable uint stringHash;
        mutable uint largestSubLength;
        uint len;
    private:
        static void append(const Data *data, QChar *ch);
    };
    // ### FIXME: Should this be a V4_OBJECT
    V4_OBJECT(QV4::Managed)
    Q_MANAGED_TYPE(String)
    enum {
        IsString = true
    };

    enum StringType {
        StringType_Unknown,
        StringType_Regular,
        StringType_UInt,
        StringType_ArrayIndex
    };

    bool equals(String *other) const;
    inline bool isEqualTo(const String *other) const {
        if (this == other)
            return true;
        if (hashValue() != other->hashValue())
            return false;
        Q_ASSERT(!d()->largestSubLength);
        if (d()->identifier && d()->identifier == other->d()->identifier)
            return true;
        if (subtype() >= StringType_UInt && subtype() == other->subtype())
            return true;

        return toQString() == other->toQString();
    }

    inline bool compare(const String *other) {
        return toQString() < other->toQString();
    }

    inline QString toQString() const {
        if (d()->largestSubLength)
            d()->simplifyString();
        QStringDataPtr ptr = { d()->text };
        d()->text->ref.ref();
        return QString(ptr);
    }

    inline unsigned hashValue() const {
        if (subtype() == StringType_Unknown)
            createHashValue();
        Q_ASSERT(!d()->largestSubLength);

        return d()->stringHash;
    }
    uint asArrayIndex() const {
        if (subtype() == StringType_Unknown)
            createHashValue();
        Q_ASSERT(!d()->largestSubLength);
        if (subtype() == StringType_ArrayIndex)
            return d()->stringHash;
        return UINT_MAX;
    }
    uint toUInt(bool *ok) const;

    void makeIdentifier() const {
        if (d()->identifier)
            return;
        makeIdentifierImpl();
    }

    void makeIdentifierImpl() const;

    void createHashValue() const;
    static uint createHashValue(const QChar *ch, int length);
    static uint createHashValue(const char *ch, int length);

    bool startsWithUpper() const {
        const String *l = this;
        while (l->d()->largestSubLength)
            l = l->d()->left;
        return l->d()->text->size && QChar::isUpper(l->d()->text->data()[0]);
    }

    Identifier *identifier() const { return d()->identifier; }

protected:
    static void destroy(Managed *);
    static void markObjects(Managed *that, ExecutionEngine *e);
    static ReturnedValue get(Managed *m, String *name, bool *hasProperty);
    static ReturnedValue getIndexed(Managed *m, uint index, bool *hasProperty);
    static void put(Managed *m, String *name, const ValueRef value);
    static void putIndexed(Managed *m, uint index, const ValueRef value);
    static PropertyAttributes query(const Managed *m, String *name);
    static PropertyAttributes queryIndexed(const Managed *m, uint index);
    static bool deleteProperty(Managed *, String *);
    static bool deleteIndexedProperty(Managed *m, uint index);
    static bool isEqualTo(Managed *that, Managed *o);
    static uint getLength(const Managed *m);
#endif

public:
    static uint toArrayIndex(const QString &str);
};

#ifndef V4_BOOTSTRAP
template<>
inline String *value_cast(const Value &v) {
    return v.asString();
}

template<>
inline ReturnedValue value_convert<String>(ExecutionEngine *e, const Value &v)
{
    return v.toString(e)->asReturnedValue();
}

#endif

}

QT_END_NAMESPACE

#endif
