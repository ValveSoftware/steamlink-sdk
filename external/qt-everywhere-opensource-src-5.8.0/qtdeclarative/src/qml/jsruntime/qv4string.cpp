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

#include "qv4string_p.h"
#include "qv4value_p.h"
#ifndef V4_BOOTSTRAP
#include "qv4identifiertable_p.h"
#include "qv4runtime_p.h"
#include "qv4objectproto_p.h"
#include "qv4stringobject_p.h"
#endif
#include <QtCore/QHash>
#include <QtCore/private/qnumeric_p.h>

using namespace QV4;

#ifndef V4_BOOTSTRAP

DEFINE_MANAGED_VTABLE(String);

void String::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    String::Data *s = static_cast<String::Data *>(that);
    if (s->largestSubLength) {
        s->left->mark(e);
        s->right->mark(e);
    }
}

bool String::isEqualTo(Managed *t, Managed *o)
{
    if (t == o)
        return true;

    if (!o->d()->vtable()->isString)
        return false;

    return static_cast<String *>(t)->isEqualTo(static_cast<String *>(o));
}


void Heap::String::init(MemoryManager *mm, const QString &t)
{
    Base::init();
    this->mm = mm;

    subtype = String::StringType_Unknown;

    text = const_cast<QString &>(t).data_ptr();
    text->ref.ref();
    identifier = 0;
    stringHash = UINT_MAX;
    largestSubLength = 0;
    len = text->size;
}

void Heap::String::init(MemoryManager *mm, String *l, String *r)
{
    Base::init();
    this->mm = mm;

    subtype = String::StringType_Unknown;

    left = l;
    right = r;
    stringHash = UINT_MAX;
    largestSubLength = qMax(l->largestSubLength, r->largestSubLength);
    len = l->len + r->len;

    if (!l->largestSubLength && l->len > largestSubLength)
        largestSubLength = l->len;
    if (!r->largestSubLength && r->len > largestSubLength)
        largestSubLength = r->len;

    // make sure we don't get excessive depth in our strings
    if (len > 256 && len >= 2*largestSubLength)
        simplifyString();
}

uint String::toUInt(bool *ok) const
{
    *ok = true;

    if (subtype() == Heap::String::StringType_Unknown)
        d()->createHashValue();
    if (subtype() == Heap::String::StringType_ArrayIndex)
        return d()->stringHash;

    // required for UINT_MAX or numbers starting with a leading 0
    double d = RuntimeHelpers::stringToNumber(toQString());
    uint l = (uint)d;
    if (d == l)
        return l;
    *ok = false;
    return UINT_MAX;
}

void String::makeIdentifierImpl(ExecutionEngine *e) const
{
    if (d()->largestSubLength)
        d()->simplifyString();
    Q_ASSERT(!d()->largestSubLength);
    e->identifierTable->identifier(this);
}

void Heap::String::simplifyString() const
{
    Q_ASSERT(largestSubLength);

    int l = length();
    QString result(l, Qt::Uninitialized);
    QChar *ch = const_cast<QChar *>(result.constData());
    append(this, ch);
    text = result.data_ptr();
    text->ref.ref();
    identifier = 0;
    largestSubLength = 0;
    mm->growUnmanagedHeapSizeUsage(size_t(text->size) * sizeof(QChar));
}

void Heap::String::append(const String *data, QChar *ch)
{
    std::vector<const String *> worklist;
    worklist.reserve(32);
    worklist.push_back(data);

    while (!worklist.empty()) {
        const String *item = worklist.back();
        worklist.pop_back();

        if (item->largestSubLength) {
            worklist.push_back(item->right);
            worklist.push_back(item->left);
        } else {
            memcpy(ch, item->text->data(), item->text->size * sizeof(QChar));
            ch += item->text->size;
        }
    }
}

void Heap::String::createHashValue() const
{
    if (largestSubLength)
        simplifyString();
    Q_ASSERT(!largestSubLength);
    const QChar *ch = reinterpret_cast<const QChar *>(text->data());
    const QChar *end = ch + text->size;
    stringHash = QV4::String::calculateHashValue(ch, end, &subtype);
}

uint String::getLength(const Managed *m)
{
    return static_cast<const String *>(m)->d()->length();
}

#endif // V4_BOOTSTRAP

uint String::toArrayIndex(const QString &str)
{
    return QV4::String::toArrayIndex(str.constData(), str.constData() + str.length());
}

