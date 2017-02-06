/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include <QVariant>

#include "qabstractdatetime_p.h"
#include "qabstractfloat_p.h"
#include "qatomicstring_p.h"
#include "qatomictype_p.h"
#include "qboolean_p.h"
#include "qbuiltintypes_p.h"
#include "qdate_p.h"
#include "qschemadatetime_p.h"
#include "qderivedinteger_p.h"
#include "qdynamiccontext_p.h"
#include "qgenericsequencetype_p.h"
#include "qhexbinary_p.h"
#include "qinteger_p.h"
#include "qpatternistlocale_p.h"
#include "qqnamevalue_p.h"
#include "qschematime_p.h"
#include "qvalidationerror_p.h"

#include "qitem_p.h"

QT_BEGIN_NAMESPACE

/**
 * @file
 * @short Contains the implementation for AtomicValue. The definition is in qitem_p.h.
 */

using namespace QPatternist;

AtomicValue::~AtomicValue()
{
}

bool AtomicValue::evaluateEBV(const QExplicitlySharedDataPointer<DynamicContext> &context) const
{
    context->error(QtXmlPatterns::tr("A value of type %1 cannot have an "
                                     "Effective Boolean Value.")
                      .arg(formatType(context->namePool(), type())),
                      ReportContext::FORG0006,
                      QSourceLocation());
    return false; /* Silence GCC warning. */
}

bool AtomicValue::hasError() const
{
    return false;
}

QVariant AtomicValue::toQt(const AtomicValue *const value)
{
    Q_ASSERT_X(value, Q_FUNC_INFO,
               "Internal error, a null pointer cannot be passed.");

    const ItemType::Ptr t(value->type());

    if(BuiltinTypes::xsString->xdtTypeMatches(t)
       || BuiltinTypes::xsUntypedAtomic->xdtTypeMatches(t)
       || BuiltinTypes::xsAnyURI->xdtTypeMatches(t))
        return value->stringValue();
    /* Note, this occurs before the xsInteger test, since xs:unsignedLong
     * is a subtype of it. */
    else if(*BuiltinTypes::xsUnsignedLong == *t)
        return QVariant(value->as<DerivedInteger<TypeUnsignedLong> >()->storedValue());
    else if(BuiltinTypes::xsInteger->xdtTypeMatches(t))
        return QVariant(value->as<Numeric>()->toInteger());
    else if(BuiltinTypes::xsFloat->xdtTypeMatches(t)
            || BuiltinTypes::xsDouble->xdtTypeMatches(t)
            || BuiltinTypes::xsDecimal->xdtTypeMatches(t))
        return QVariant(value->as<Numeric>()->toDouble());
    /* We currently does not support xs:time. */
    else if(BuiltinTypes::xsDateTime->xdtTypeMatches(t))
        return QVariant(value->as<AbstractDateTime>()->toDateTime());
    else if(BuiltinTypes::xsDate->xdtTypeMatches(t))
        return QVariant(value->as<AbstractDateTime>()->toDateTime().toUTC().date());
    else if(BuiltinTypes::xsBoolean->xdtTypeMatches(t))
        return QVariant(value->as<Boolean>()->value());
    else if(BuiltinTypes::xsBase64Binary->xdtTypeMatches(t)
            || BuiltinTypes::xsHexBinary->xdtTypeMatches(t))
        return QVariant(value->as<Base64Binary>()->asByteArray());
    else if(BuiltinTypes::xsQName->xdtTypeMatches(t))
        return QVariant::fromValue(value->as<QNameValue>()->qName());
    else
    {
        /* A type we don't support in Qt. Includes xs:time currently. */
        return QVariant();
    }
}

Item AtomicValue::toXDM(const QVariant &value)
{
    Q_ASSERT_X(value.isValid(), Q_FUNC_INFO,
               "QVariants sent to Patternist must be valid.");

    switch(value.userType())
    {
        case QVariant::Char:
        /* Fallthrough. A single codepoint is a string in XQuery. */
        case QVariant::String:
            return AtomicString::fromValue(value.toString());
        case QVariant::Url:
        {
            /* QUrl doesn't follow the spec properly, so we
             * have to let it be an xs:string. Calling QVariant::toString()
             * on a QVariant that contains a QUrl returns, surprisingly,
             * an empty string. */
            return AtomicString::fromValue(QString::fromLatin1(value.toUrl().toEncoded()));
        }
        case QVariant::ByteArray:
            return HexBinary::fromValue(value.toByteArray());
        case QVariant::Int:
        /* Fallthrough. */
        case QVariant::LongLong:
        /* Fallthrough. */
        case QVariant::UInt:
            return Integer::fromValue(value.toLongLong());
        case QVariant::ULongLong:
            return DerivedInteger<TypeUnsignedLong>::fromValueUnchecked(value.toULongLong());
        case QVariant::Bool:
            return Boolean::fromValue(value.toBool());
        case QVariant::Time:
            return SchemaTime::fromDateTime(value.toDateTime());
        case QVariant::Date:
            return Date::fromDateTime(QDateTime(value.toDate(), QTime(), Qt::UTC));
        case QVariant::DateTime:
            return DateTime::fromDateTime(value.toDateTime());
        case QMetaType::Float:
            return Item(Double::fromValue(value.toFloat()));
        case QVariant::Double:
            return Item(Double::fromValue(value.toDouble()));
        default:
        {
            if (value.userType() == qMetaTypeId<float>())
            {
                return Item(Float::fromValue(value.value<float>()));
            }
            else {
                Q_ASSERT_X(false,
                           Q_FUNC_INFO,
                           qPrintable(QString::fromLatin1(
                               "QVariants of type %1 are not supported in "
                               "Patternist, see the documentation")
                                  .arg(QLatin1String(value.typeName()))));
                return AtomicValue::Ptr();
            }
        }
    }
}

ItemType::Ptr AtomicValue::qtToXDMType(const QXmlItem &item)
{
    Q_ASSERT(!item.isNull());

    if(item.isNull())
        return ItemType::Ptr();

    if(item.isNode())
        return BuiltinTypes::node;

    Q_ASSERT(item.isAtomicValue());
    const QVariant v(item.toAtomicValue());

    switch(int(v.type()))
    {
        case QVariant::Char:
        /* Fallthrough. */
        case QVariant::String:
        /* Fallthrough. */
        case QVariant::Url:
            return BuiltinTypes::xsString;
        case QVariant::Bool:
            return BuiltinTypes::xsBoolean;
        case QVariant::ByteArray:
            return BuiltinTypes::xsBase64Binary;
        case QVariant::Int:
        /* Fallthrough. */
        case QVariant::LongLong:
            return BuiltinTypes::xsInteger;
        case QVariant::ULongLong:
            return BuiltinTypes::xsUnsignedLong;
        case QVariant::Date:
            return BuiltinTypes::xsDate;
        case QVariant::DateTime:
        /* Fallthrough. */
        case QVariant::Time:
            return BuiltinTypes::xsDateTime;
        case QMetaType::Float:
            return BuiltinTypes::xsFloat;
        case QVariant::Double:
            return BuiltinTypes::xsDouble;
        default:
            return ItemType::Ptr();
    }
}

QT_END_NAMESPACE
