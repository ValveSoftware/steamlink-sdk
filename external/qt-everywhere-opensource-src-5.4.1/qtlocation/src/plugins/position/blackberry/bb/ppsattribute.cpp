/****************************************************************************
**
** Copyright (C) 2012 - 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include <bb/PpsAttribute>
#include "ppsattribute_p.h"

#include <QDebug>
#include <QVariant>

Q_DECLARE_METATYPE(QList<bb::PpsAttribute>)
typedef QMap<QString, bb::PpsAttribute> PpsAttributeMap;
Q_DECLARE_METATYPE(PpsAttributeMap)

namespace bb
{

///////////////////////////
//
// PpsAttributePrivate
//
///////////////////////////

PpsAttributePrivate::PpsAttributePrivate():
    _type(PpsAttribute::None)
{
}

/*static*/ PpsAttribute PpsAttributePrivate::ppsAttribute( int value, PpsAttributeFlag::Types flags )
{
    PpsAttribute attribute;
    attribute.d->_type = PpsAttribute::Number;
    attribute.d->_data = value;
    attribute.d->_flags = flags;
    return attribute;
}

/*static*/ PpsAttribute PpsAttributePrivate::ppsAttribute( long long value, PpsAttributeFlag::Types flags )
{
    PpsAttribute attribute;
    attribute.d->_type = PpsAttribute::Number;
    attribute.d->_data = value;
    attribute.d->_flags = flags;
    return attribute;
}

/*static*/ PpsAttribute PpsAttributePrivate::ppsAttribute( double value, PpsAttributeFlag::Types flags )
{
    PpsAttribute attribute;
    attribute.d->_type = PpsAttribute::Number;
    attribute.d->_data = value;
    attribute.d->_flags = flags;
    return attribute;
}

/*static*/ PpsAttribute PpsAttributePrivate::ppsAttribute( bool value, PpsAttributeFlag::Types flags )
{
    PpsAttribute attribute;
    attribute.d->_type = PpsAttribute::Bool;
    attribute.d->_data = value;
    attribute.d->_flags = flags;
    return attribute;
}

/*static*/ PpsAttribute PpsAttributePrivate::ppsAttribute( const QString &value, PpsAttributeFlag::Types flags )
{
    PpsAttribute attribute;
    attribute.d->_type = PpsAttribute::String;
    attribute.d->_data = value;
    attribute.d->_flags = flags;
    return attribute;
}

/*static*/ PpsAttribute PpsAttributePrivate::ppsAttribute( const QList<PpsAttribute> &value, PpsAttributeFlag::Types flags )
{
    PpsAttribute attribute;
    attribute.d->_type = PpsAttribute::Array;
    attribute.d->_data = QVariant::fromValue(value);
    attribute.d->_flags = flags;
    return attribute;
}

/*static*/ PpsAttribute PpsAttributePrivate::ppsAttribute( const QMap<QString, PpsAttribute> &value, PpsAttributeFlag::Types flags )
{
    PpsAttribute attribute;
    attribute.d->_type = PpsAttribute::Object;
    attribute.d->_data = QVariant::fromValue(value);
    attribute.d->_flags = flags;
    return attribute;
}

///////////////////////////
//
// PpsAttribute
//
///////////////////////////

PpsAttribute::PpsAttribute():
    d(new PpsAttributePrivate())
{
}

PpsAttribute::~PpsAttribute()
{
}

PpsAttribute::PpsAttribute(const PpsAttribute & other):
    d(other.d)
{
}

PpsAttribute &PpsAttribute::operator=(const PpsAttribute & other)
{
    d = other.d;
    return *this;
}

bool PpsAttribute::operator==(const PpsAttribute & other) const
{
    if ( type() != other.type() ) {
        return false;
    }
    if ( flags() != other.flags() ) {
        return false;
    }

    switch ( type() ) {
    case PpsAttribute::Number:
    case PpsAttribute::Bool:
    case PpsAttribute::String:
        // QVariant can compare double, int, longlong, bool, and QString for us.
        return d->_data == other.d->_data;
    case PpsAttribute::Array:
        // QVariant can't compare custom types (like QList<PpsAttribute>), always returning false.  So we pull
        // the lists out manually and compare them.
        return toList() == other.toList();
    case PpsAttribute::Object:
        // QVariant can't compare custom types (like QMap<QString, PpsAttribute>), always returning false.  So
        // we pull the maps out manually and compare them.
        return toMap() == other.toMap();
    case PpsAttribute::None:
        // Both are "None" type, so the actual content doesn't matter.
        return true;
    }
    return d->_data == other.d->_data;
}

bool PpsAttribute::operator!=(const PpsAttribute & other) const
{
    return !(*this == other);
}

bool PpsAttribute::isValid() const
{
    return d->_type != PpsAttribute::None;
}

PpsAttribute::Type PpsAttribute::type() const
{
    return d->_type;
}

bool PpsAttribute::isNumber() const
{
    return type() == PpsAttribute::Number;
}

bool PpsAttribute::isBool() const
{
    return type() == PpsAttribute::Bool;
}

bool PpsAttribute::isString() const
{
    return type() == PpsAttribute::String;
}

bool PpsAttribute::isArray() const
{
    return type() == PpsAttribute::Array;
}

bool PpsAttribute::isObject() const
{
    return type() == PpsAttribute::Object;
}

double PpsAttribute::toDouble() const
{
    return d->_data.toDouble();
}

qlonglong PpsAttribute::toLongLong() const
{
    return d->_data.toLongLong();
}

int PpsAttribute::toInt() const
{
    return d->_data.toInt();
}

bool PpsAttribute::toBool() const
{
    return d->_data.toBool();
}

QString PpsAttribute::toString() const
{
    return d->_data.toString();
}

QList<PpsAttribute> PpsAttribute::toList() const
{
    return d->_data.value< QList<PpsAttribute> >();
}

QMap<QString, PpsAttribute> PpsAttribute::toMap() const
{
    return d->_data.value< QMap<QString, PpsAttribute> >();
}

PpsAttributeFlag::Types PpsAttribute::flags() const
{
    return d->_flags;
}

QVariant PpsAttribute::toVariant() const
{
    return d->_data;
}

QDebug operator<<(QDebug dbg, const PpsAttribute &attribute)
{
    dbg << "PpsAttribute(";

    switch ( attribute.type() ) {
    case PpsAttribute::Number:
        switch (attribute.toVariant().type()) {
        case QVariant::Int:
            dbg << "Number, " << attribute.flags() << ", " << attribute.toInt();
            break;
        case QVariant::LongLong:
            dbg << "Number, " << attribute.flags() << ", " << attribute.toLongLong();
            break;
        default:
            dbg << "Number, " << attribute.flags() << ", " << attribute.toDouble();
            break;
        }
        break;
    case PpsAttribute::Bool:
        dbg << "Bool, " << attribute.flags() << ", " << attribute.toBool();
        break;
    case PpsAttribute::String:
        dbg << "String, " << attribute.flags() << ", " << attribute.toString();
        break;
    case PpsAttribute::Array:
        dbg << "Array, " << attribute.flags() << ", " << attribute.toList();
        break;
    case PpsAttribute::Object:
        dbg << "Object, " << attribute.flags() << ", " << attribute.toMap();
        break;
    case PpsAttribute::None:
        dbg << "None";
        break;
    }

    dbg << ')';

    return dbg;
}

} // namespace bb
