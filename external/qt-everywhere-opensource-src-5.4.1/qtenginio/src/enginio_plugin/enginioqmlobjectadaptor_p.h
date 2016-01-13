/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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

#ifndef ENGINIOQMLOBJECTADAPTOR_P_H
#define ENGINIOQMLOBJECTADAPTOR_P_H

#include <Enginio/private/enginioobjectadaptor_p.h>
#include "enginioqmlclient_p.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtQml/qjsengine.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qjsvalueiterator.h>

QT_BEGIN_NAMESPACE

template <> struct ValueAdaptor<QJSValue>;
template <> struct ObjectAdaptor<QJSValue>;
template <> struct ArrayAdaptor<QJSValue>;

template <> struct ValueAdaptor<QJSValue>
{
    QJSValue _value;
    EnginioQmlClientPrivate *_client;

    ValueAdaptor(const QJSValue &value, EnginioQmlClientPrivate *client)
        : _value(value)
        , _client(client)
    {}

    bool isComposedType() const { return _value.isObject(); }

    int toInt() const { return _value.toInt(); }

    QString toString() const {
        if (_value.isUndefined() || _value.isNull())
            return QString();
        return _value.toString();
    }

    ValueAdaptor<QJSValue> operator[](const QString &index) const
    {
        return ValueAdaptor<QJSValue>(_value.property(index), _client);
    }

    bool contains(const QString &key) const { return _value.hasProperty(key); }

    QByteArray toJson() const
    {
        return _client->toJson(_value);
    }
    void remove(const QString &index) { _value.deleteProperty(index); }

    ObjectAdaptor<QJSValue> toObject() const;
    ArrayAdaptor<QJSValue> toArray() const;
};

template <> struct ObjectAdaptor<QJSValue> : public ValueAdaptor<QJSValue>
{
    ObjectAdaptor(const QJSValue &value, EnginioQmlClientPrivate *client)
        : ValueAdaptor<QJSValue>(value, client)
    {}
    bool isEmpty() const { return !_value.isObject(); }
};

template <> struct ArrayAdaptor<QJSValue> : public ValueAdaptor<QJSValue>
{
    ArrayAdaptor(const QJSValue &value, EnginioQmlClientPrivate *client)
        : ValueAdaptor<QJSValue>(value, client)
    {}

    ArrayAdaptor(const ArrayAdaptor<QJSValue> &value)
        : ValueAdaptor<QJSValue>(value)
    {}

    bool isEmpty() const { return _value.property(EnginioString::length).toInt() == 0; }

    struct const_iterator
    {
        QJSValueIterator _iter;
        QJSValue _value;

        const_iterator(const QJSValue &value = QJSValue())
            : _iter(value)
            , _value(value)
        {}

        const_iterator(const const_iterator &other)
            : _iter(other._value)
            , _value(other._value)
        {}

        bool operator !=(const const_iterator &other) const
        {
            return !other._iter.hasNext() && !_iter.hasNext();
        }
        QJSValue operator *()
        {
            return _iter.value();
        }
        void operator ++() { _iter.next(); }
    };

    const_iterator constBegin() const { return _value; }
    const_iterator constEnd() const { return const_iterator(); }
};

inline ObjectAdaptor<QJSValue> ValueAdaptor<QJSValue>::toObject() const { return ObjectAdaptor<QJSValue>(_value, _client); }
inline ArrayAdaptor<QJSValue> ValueAdaptor<QJSValue>::toArray() const { return ArrayAdaptor<QJSValue>(_value, _client); }

QT_END_NAMESPACE

#endif // ENGINIOQMLOBJECTADAPTOR_P_H
