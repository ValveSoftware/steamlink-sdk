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
#ifndef QV4PERSISTENT_H
#define QV4PERSISTENT_H

#include "qv4value_inl_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Q_QML_PRIVATE_EXPORT PersistentValuePrivate
{
    PersistentValuePrivate(ReturnedValue v, ExecutionEngine *engine = 0, bool weak = false);
    virtual ~PersistentValuePrivate();
    Value value;
    uint refcount;
    bool weak;
    QV4::ExecutionEngine *engine;
    PersistentValuePrivate **prev;
    PersistentValuePrivate *next;

    void init();
    void removeFromList();
    void ref() { ++refcount; }
    void deref();
    PersistentValuePrivate *detach(const ReturnedValue value, bool weak = false);

    bool checkEngine(QV4::ExecutionEngine *otherEngine) {
        if (!engine) {
            Q_ASSERT(!value.isObject());
            engine = otherEngine;
        }
        return (engine == otherEngine);
    }
};

class Q_QML_EXPORT PersistentValue
{
public:
    PersistentValue() : d(0) {}
    PersistentValue(const PersistentValue &other);
    PersistentValue &operator=(const PersistentValue &other);

    PersistentValue(const ValueRef val);
    PersistentValue(ReturnedValue val);
    template<typename T>
    PersistentValue(Returned<T> *obj);
    PersistentValue &operator=(const ValueRef other);
    PersistentValue &operator=(const ScopedValue &other);
    PersistentValue &operator =(ReturnedValue other);
    template<typename T>
    PersistentValue &operator=(Returned<T> *obj);
    ~PersistentValue();

    ReturnedValue value() const {
        return (d ? d->value.asReturnedValue() : Primitive::undefinedValue().asReturnedValue());
    }

    ExecutionEngine *engine() {
        if (!d)
            return 0;
        if (d->engine)
            return d->engine;
        Managed *m = d->value.asManaged();
        return m ? m->engine() : 0;
    }

    bool isUndefined() const { return !d || d->value.isUndefined(); }
    bool isNullOrUndefined() const { return !d || d->value.isNullOrUndefined(); }
    void clear() {
        *this = PersistentValue();
    }

private:
    friend struct ValueRef;
    PersistentValuePrivate *d;
};

class Q_QML_EXPORT WeakValue
{
public:
    WeakValue() : d(0) {}
    WeakValue(const ValueRef val);
    WeakValue(const WeakValue &other);
    WeakValue(ReturnedValue val);
    template<typename T>
    WeakValue(Returned<T> *obj);
    WeakValue &operator=(const WeakValue &other);
    WeakValue &operator=(const ValueRef other);
    WeakValue &operator =(const ReturnedValue &other);
    template<typename T>
    WeakValue &operator=(Returned<T> *obj);

    ~WeakValue();

    ReturnedValue value() const {
        return (d ? d->value.asReturnedValue() : Primitive::undefinedValue().asReturnedValue());
    }

    ExecutionEngine *engine() {
        if (!d)
            return 0;
        if (d->engine)
            return d->engine;
        Managed *m = d->value.asManaged();
        return m ? m->engine() : 0;
    }

    bool isUndefined() const { return !d || d->value.isUndefined(); }
    bool isNullOrUndefined() const { return !d || d->value.isNullOrUndefined(); }
    void clear() {
        *this = WeakValue();
    }

    void markOnce(ExecutionEngine *e);

private:
    friend struct ValueRef;
    PersistentValuePrivate *d;
};

} // namespace QV4

QT_END_NAMESPACE

#endif
