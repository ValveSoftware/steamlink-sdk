/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSCriptTools module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscriptdebuggervalueproperty_p.h"
#include "qscriptdebuggervalue_p.h"
#include "qscriptdebuggerobjectsnapshotdelta_p.h"

#include <QtCore/qshareddata.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qstring.h>

QT_BEGIN_NAMESPACE

/*!
  \internal
  \class QScriptDebuggerValueProperty
*/

class QScriptDebuggerValuePropertyPrivate : public QSharedData
{
public:
    QScriptDebuggerValuePropertyPrivate();
    ~QScriptDebuggerValuePropertyPrivate();

    QString name;
    QScriptDebuggerValue value;
    QString valueAsString;
    QScriptValue::PropertyFlags flags;
};

QScriptDebuggerValuePropertyPrivate::QScriptDebuggerValuePropertyPrivate()
{
}

QScriptDebuggerValuePropertyPrivate::~QScriptDebuggerValuePropertyPrivate()
{
}

/*!
  Constructs an invalid QScriptDebuggerValueProperty.
*/
QScriptDebuggerValueProperty::QScriptDebuggerValueProperty()
    : d_ptr(0)
{
}

/*!
  Constructs a QScriptDebuggerValueProperty with the given \a name,
  \a value and \a flags.
*/
QScriptDebuggerValueProperty::QScriptDebuggerValueProperty(const QString &name,
                                                           const QScriptDebuggerValue &value,
                                                           const QString &valueAsString,
                                                           QScriptValue::PropertyFlags flags)
    : d_ptr(new QScriptDebuggerValuePropertyPrivate)
{
    d_ptr->name = name;
    d_ptr->value = value;
    d_ptr->valueAsString = valueAsString;
    d_ptr->flags = flags;
    d_ptr->ref.ref();
}

/*!
  Constructs a QScriptDebuggerValueProperty that is a copy of the \a other property.
*/
QScriptDebuggerValueProperty::QScriptDebuggerValueProperty(const QScriptDebuggerValueProperty &other)
    : d_ptr(other.d_ptr.data())
{
    if (d_ptr)
        d_ptr->ref.ref();
}

/*!
  Destroys this QScriptDebuggerValueProperty.
*/
QScriptDebuggerValueProperty::~QScriptDebuggerValueProperty()
{
}

/*!
  Assigns the \a other property to this QScriptDebuggerValueProperty.
*/
QScriptDebuggerValueProperty &QScriptDebuggerValueProperty::operator=(const QScriptDebuggerValueProperty &other)
{
    d_ptr.assign(other.d_ptr.data());
    return *this;
}

/*!
  Returns the name of this QScriptDebuggerValueProperty.
*/
QString QScriptDebuggerValueProperty::name() const
{
    Q_D(const QScriptDebuggerValueProperty);
    if (!d)
        return QString();
    return d->name;
}

/*!
  Returns the value of this QScriptDebuggerValueProperty.
*/
QScriptDebuggerValue QScriptDebuggerValueProperty::value() const
{
    Q_D(const QScriptDebuggerValueProperty);
    if (!d)
        return QScriptDebuggerValue();
    return d->value;
}

QString QScriptDebuggerValueProperty::valueAsString() const
{
    Q_D(const QScriptDebuggerValueProperty);
    if (!d)
        return QString();
    return d->valueAsString;
}

/*!
  Returns the flags of this QScriptDebuggerValueProperty.
*/
QScriptValue::PropertyFlags QScriptDebuggerValueProperty::flags() const
{
    Q_D(const QScriptDebuggerValueProperty);
    if (!d)
        return 0;
    return d->flags;
}

/*!
  Returns true if this QScriptDebuggerValueProperty is valid, otherwise
  returns false.
*/
bool QScriptDebuggerValueProperty::isValid() const
{
    Q_D(const QScriptDebuggerValueProperty);
    return (d != 0);
}

/*!
  \fn QDataStream &operator<<(QDataStream &stream, const QScriptDebuggerValueProperty &property)
  \relates QScriptDebuggerValueProperty

  Writes the given \a property to the specified \a stream.
*/
QDataStream &operator<<(QDataStream &out, const QScriptDebuggerValueProperty &property)
{
    out << property.name();
    out << property.value();
    out << property.valueAsString();
    out << (quint32)property.flags();
    return out;
}

/*!
  \fn QDataStream &operator>>(QDataStream &stream, QScriptDebuggerValueProperty &property)
  \relates QScriptDebuggerValueProperty

  Reads a QScriptDebuggerValueProperty from the specified \a stream into the
  given \a property.
*/
QDataStream &operator>>(QDataStream &in, QScriptDebuggerValueProperty &property)
{
    QString name;
    QScriptDebuggerValue value;
    QString valueAsString;
    quint32 flags;
    in >> name;
    in >> value;
    in >> valueAsString;
    in >> flags;
    property = QScriptDebuggerValueProperty(
        name, value, valueAsString, QScriptValue::PropertyFlags(flags));
    return in;
}

QDataStream &operator<<(QDataStream &out, const QScriptDebuggerObjectSnapshotDelta &delta)
{
    out << delta.removedProperties;
    out << delta.changedProperties;
    out << delta.addedProperties;
    return out;
}

QDataStream &operator>>(QDataStream &in, QScriptDebuggerObjectSnapshotDelta &delta)
{
    in >> delta.removedProperties;
    in >> delta.changedProperties;
    in >> delta.addedProperties;
    return in;
}

QT_END_NAMESPACE
