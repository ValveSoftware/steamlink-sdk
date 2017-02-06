/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qplaceattribute_p.h"
#include "qplaceattribute.h"

QT_USE_NAMESPACE

template<> QPlaceAttributePrivate *QSharedDataPointer<QPlaceAttributePrivate>::clone()
{
    return d->clone();
}

QPlaceAttributePrivate::QPlaceAttributePrivate(const QPlaceAttributePrivate &other)
    : QSharedData(other),
      label(other.label),
      text(other.text)
{
}

bool QPlaceAttributePrivate::operator== (const QPlaceAttributePrivate &other) const
{
    return label == other.label
            && text == other.text;
}

bool QPlaceAttributePrivate::isEmpty() const
{
    return  label.isEmpty()
            && text.isEmpty();
}


/*!
    \class QPlaceAttribute
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-data
    \since 5.6

    \brief The QPlaceAttribute class represents generic attribute information about a place.

    A QPlaceAttribute instance stores an additional piece of information about a place that is not
    otherwise exposed through the QPlace class.  A QPlaceAttribute encapsulates a
    localized label which describes the attribute and rich text string representing the attribute's value.
    Generally, both are intended to be displayed to the end-user as is.

    Some plugins may not support attributes at all, others may only support a
    certain set, others still may support a dynamically changing set of attributes
    over time or even allow attributes to be arbitrarily defined by the client
    application.  The attributes could also vary on a place by place basis,
    for example one place may have opening hours while another does not.
    Consult the \l {Plugin References and Parameters}{plugin
    references} for details.

    \section2 Attribute Types
    The QPlaceAttribute class defines some constant strings which characterize standard \e {attribute types}.
    \list
        \li QPlaceAttribute::OpeningHours
        \li QPlaceAttribute::Payment
        \li QPlaceAttribute::Provider
    \endlist

    There is a class of attribute types of the format x_id_<provider> for example x_id_here.
    This class of attributes is a set of alternative identifiers of the place, from the specified provider's
    perspective.

    The above types are used to access and modify attributes in QPlace via:
    \list
        \li QPlace::extendedAttribute()
        \li QPlace::setExtendedAttribute()
        \li QPlace::removeExtendedAttribute()
        \li QPlace::removeExtendedAttribute()
    \endlist

    The \e {attribute type} is a string type so that providers are able to introduce
    new attributes as necessary.  Custom attribute types should always be prefixed
    by a qualifier in order to avoid conflicts.

    \section3 User Readable and Non-User Readable Attributes
    Some attributes may not be intended to be readable by end users, the label field
    of such attributes are empty to indicate this fact.
*/

/*!
   \variable QPlaceAttribute::OpeningHours
   Specifies the opening hours.
*/
const QString QPlaceAttribute::OpeningHours(QLatin1String("openingHours"));

/*!
   \variable QPlaceAttribute::Payment
   The constant to specify an attribute that defines the methods of payment.
*/
const QString QPlaceAttribute::Payment(QLatin1String("payment"));

/*!
    \variable QPlaceAttribute::Provider
    The constant to specify an attribute that defines which
    provider the place came from.
*/
const QString QPlaceAttribute::Provider(QLatin1String("x_provider"));

/*!
    Constructs an attribute.
*/
QPlaceAttribute::QPlaceAttribute()
    : d_ptr(new QPlaceAttributePrivate)
{
}

/*!
    Destroys the attribute.
*/
QPlaceAttribute::~QPlaceAttribute()
{
}

/*!
    Creates a copy of \a other.
*/
QPlaceAttribute::QPlaceAttribute(const QPlaceAttribute &other)
    :d_ptr(other.d_ptr)
{
}

/*!
    Assigns \a other to this attribute and returns a reference to this
    attribute.
*/
QPlaceAttribute &QPlaceAttribute::operator=(const QPlaceAttribute &other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

/*!
    Returns true if \a other is equal to this attribute, otherwise
    returns false.
*/
bool QPlaceAttribute::operator== (const QPlaceAttribute &other) const
{
    if (d_ptr == other.d_ptr)
        return true;
    return ( *(d_ptr.constData()) == *(other.d_ptr.constData()));
}

/*!
    Returns true if \a other is not equal to this attribute,
    otherwise returns false.
*/
bool QPlaceAttribute::operator!= (const QPlaceAttribute &other) const
{
    return (!this->operator ==(other));
}

/*!
    Returns a localized label describing the attribute.
*/
QString QPlaceAttribute::label() const
{
    return d_ptr->label;
}

/*!
    Sets the \a label of the attribute.
*/
void QPlaceAttribute::setLabel(const QString &label)
{
    d_ptr->label = label;
}

/*!
    Returns a piece of rich text representing the attribute value.
*/
QString QPlaceAttribute::text() const
{
    return d_ptr->text;
}

/*!
    Sets the \a text of the attribute.
*/
void QPlaceAttribute::setText(const QString &text)
{
    d_ptr->text = text;
}

/*!
    Returns a boolean indicating whether the all the fields of the place attribute are empty or not.
*/
bool QPlaceAttribute::isEmpty() const
{
    return d_ptr->isEmpty();
}
