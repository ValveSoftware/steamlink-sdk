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
#ifndef QV4STRING_H
#define QV4STRING_H

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

#include <QtCore/qstring.h>
#include "qv4managed_p.h"
#include <QtCore/private/qnumeric_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ExecutionEngine;
struct Identifier;

namespace Heap {

struct Q_QML_PRIVATE_EXPORT String : Base {
    enum StringType {
        StringType_Unknown,
        StringType_Regular,
        StringType_ArrayIndex
    };

#ifndef V4_BOOTSTRAP
    void init(MemoryManager *mm, const QString &text);
    void init(MemoryManager *mm, String *l, String *n);
    void destroy() {
        if (!largestSubLength && !text->ref.deref())
            QStringData::deallocate(text);
        Base::destroy();
    }
    void simplifyString() const;
    int length() const {
        Q_ASSERT((largestSubLength &&
                  (len == left->len + right->len)) ||
                 len == (uint)text->size);
        return len;
    }
    std::size_t retainedTextSize() const {
        return largestSubLength ? 0 : (std::size_t(text->size) * sizeof(QChar));
    }
    void createHashValue() const;
    inline unsigned hashValue() const {
        if (subtype == StringType_Unknown)
            createHashValue();
        Q_ASSERT(!largestSubLength);

        return stringHash;
    }
    inline QString toQString() const {
        if (largestSubLength)
            simplifyString();
        QStringDataPtr ptr = { text };
        text->ref.ref();
        return QString(ptr);
    }
    inline bool isEqualTo(const String *other) const {
        if (this == other)
            return true;
        if (hashValue() != other->hashValue())
            return false;
        Q_ASSERT(!largestSubLength);
        if (identifier && identifier == other->identifier)
            return true;
        if (subtype == Heap::String::StringType_ArrayIndex && other->subtype == Heap::String::StringType_ArrayIndex)
            return true;

        return toQString() == other->toQString();
    }

    union {
        mutable QStringData *text;
        mutable String *left;
    };
    union {
        mutable Identifier *identifier;
        mutable String *right;
    };
    mutable uint subtype;
    mutable uint stringHash;
    mutable uint largestSubLength;
    uint len;
    MemoryManager *mm;
private:
    static void append(const String *data, QChar *ch);
#endif
};
V4_ASSERT_IS_TRIVIAL(String)

}

struct Q_QML_PRIVATE_EXPORT String : public Managed {
#ifndef V4_BOOTSTRAP
    V4_MANAGED(String, Managed)
    Q_MANAGED_TYPE(String)
    V4_NEEDS_DESTROY
    enum {
        IsString = true
    };

    uchar subtype() const { return d()->subtype; }
    void setSubtype(uchar subtype) const { d()->subtype = subtype; }

    bool equals(String *other) const {
        return d()->isEqualTo(other->d());
    }
    inline bool isEqualTo(const String *other) const {
        return d()->isEqualTo(other->d());
    }

    inline bool compare(const String *other) {
        return toQString() < other->toQString();
    }

    inline QString toQString() const {
        return d()->toQString();
    }

    inline unsigned hashValue() const {
        return d()->hashValue();
    }
    uint asArrayIndex() const {
        if (subtype() == Heap::String::StringType_Unknown)
            d()->createHashValue();
        Q_ASSERT(!d()->largestSubLength);
        if (subtype() == Heap::String::StringType_ArrayIndex)
            return d()->stringHash;
        return UINT_MAX;
    }
    uint toUInt(bool *ok) const;

    void makeIdentifier(ExecutionEngine *e) const {
        if (d()->identifier)
            return;
        makeIdentifierImpl(e);
    }

    void makeIdentifierImpl(ExecutionEngine *e) const;

    static uint createHashValue(const QChar *ch, int length, uint *subtype)
    {
        const QChar *end = ch + length;
        return calculateHashValue(ch, end, subtype);
    }

    static uint createHashValue(const char *ch, int length, uint *subtype)
    {
        const char *end = ch + length;
        return calculateHashValue(ch, end, subtype);
    }

    bool startsWithUpper() const {
        const String::Data *l = d();
        while (l->largestSubLength)
            l = l->left;
        return l->text->size && QChar::isUpper(l->text->data()[0]);
    }

    Identifier *identifier() const { return d()->identifier; }

protected:
    static void markObjects(Heap::Base *that, ExecutionEngine *e);
    static bool isEqualTo(Managed *that, Managed *o);
    static uint getLength(const Managed *m);
#endif

public:
    static uint toArrayIndex(const QString &str);

private:
    static inline uint toUInt(const QChar *ch) { return ch->unicode(); }
    static inline uint toUInt(const char *ch) { return *ch; }

    template <typename T>
    static inline uint toArrayIndex(const T *ch, const T *end)
    {
        uint i = toUInt(ch) - '0';
        if (i > 9)
            return UINT_MAX;
        ++ch;
        // reject "01", "001", ...
        if (i == 0 && ch != end)
            return UINT_MAX;

        while (ch < end) {
            uint x = toUInt(ch) - '0';
            if (x > 9)
                return UINT_MAX;
            if (mul_overflow(i, uint(10), &i) || add_overflow(i, x, &i)) // i = i * 10 + x
                return UINT_MAX;
            ++ch;
        }
        return i;
    }

public:
    template <typename T>
    static inline uint calculateHashValue(const T *ch, const T* end, uint *subtype)
    {
        // array indices get their number as hash value
        uint h = toArrayIndex(ch, end);
        if (h != UINT_MAX) {
            if (subtype)
                *subtype = Heap::String::StringType_ArrayIndex;
            return h;
        }

        while (ch < end) {
            h = 31 * h + toUInt(ch);
            ++ch;
        }

        if (subtype)
            *subtype = Heap::String::StringType_Regular;
        return h;
    }
};

template<>
inline const String *Value::as() const {
    return isManaged() && m() && m()->vtable()->isString ? static_cast<const String *>(this) : 0;
}

#ifndef V4_BOOTSTRAP
template<>
inline ReturnedValue value_convert<String>(ExecutionEngine *e, const Value &v)
{
    return v.toString(e)->asReturnedValue();
}
#endif


}

QT_END_NAMESPACE

#endif
