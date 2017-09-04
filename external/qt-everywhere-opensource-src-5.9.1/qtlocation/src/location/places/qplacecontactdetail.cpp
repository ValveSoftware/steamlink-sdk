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

#include "qplacecontactdetail_p.h"
#include "qplacecontactdetail.h"

QT_USE_NAMESPACE

QPlaceContactDetailPrivate::QPlaceContactDetailPrivate(const QPlaceContactDetailPrivate &other)
    : QSharedData(other),
      label(other.label),
      value(other.value)
{
}

bool QPlaceContactDetailPrivate::operator== (const QPlaceContactDetailPrivate &other) const
{
    return label == other.label
            && value == other.value;
}

/*!
\class QPlaceContactDetail
\brief The QPlaceContactDetail class represents a contact detail such as a phone number or website url.
\inmodule QtLocation

\ingroup QtLocation-places
\ingroup QtLocation-places-data

The detail consists of a label and value.  The label is a localized string that can be presented
to the end user that describes that detail value which is the actual phone number, email address and so on.

\section2 Contact Types

The QPlaceContactDetail class defines some constant strings which characterize standard \e {contact types}.
\list
    \li QPlaceContactDetail::Phone
    \li QPlaceContactDetail::Email
    \li QPlaceContactDetail::Website
    \li QPlaceContactDetail::Fax
\endlist

These types are used to access and modify contact details in QPlace via:
\list
    \li QPlace::contactDetails()
    \li QPlace::setContactDetails()
    \li QPlace::appendContactDetail()
    \li QPlace::contactTypes()
\endlist

The \e {contact type} is intended to be a string type so that providers are able to introduce new contact
types if necessary.
*/

/*!
   \variable QPlaceContactDetail::Phone
   The constant to specify phone contact details
*/
const QString QPlaceContactDetail::Phone(QLatin1String("phone"));

/*!
   \variable QPlaceContactDetail::Email
   The constant to specify email contact details.
*/
const QString QPlaceContactDetail::Email(QLatin1String("email"));

/*!
   \variable QPlaceContactDetail::Website
   The constant used to specify website contact details.
*/
const QString QPlaceContactDetail::Website(QLatin1String("website"));

/*!
   \variable QPlaceContactDetail::Fax
   The constant used to specify fax contact details.
*/
const QString QPlaceContactDetail::Fax(QLatin1String("fax"));

/*!
    Constructs a contact detail.
*/
QPlaceContactDetail::QPlaceContactDetail()
    : d_ptr(new QPlaceContactDetailPrivate)
{
}

/*!
    Destroys the contact detail.
*/
QPlaceContactDetail::~QPlaceContactDetail()
{
}

/*!
    Creates a copy of \a other.
*/
QPlaceContactDetail::QPlaceContactDetail(const QPlaceContactDetail &other)
    :d_ptr(other.d_ptr)
{
}

/*!
    Assigns \a other to this contact detail and returns a reference to this
    contact detail.
*/
QPlaceContactDetail &QPlaceContactDetail::operator=(const QPlaceContactDetail &other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

/*!
    Returns true if \a other is equal to this contact detail, otherwise
    returns false.
*/
bool QPlaceContactDetail::operator== (const QPlaceContactDetail &other) const
{
    if (d_ptr == other.d_ptr)
        return true;
    return ( *(d_ptr.constData()) == *(other.d_ptr.constData()));
}

/*!
    Returns true if \a other is not equal to this contact detail,
    otherwise returns false.
*/
bool QPlaceContactDetail::operator!= (const QPlaceContactDetail &other) const
{
    return (!this->operator ==(other));
}

/*!
    Returns a label describing the contact detail.

    The label can potentially be localized. The language is dependent on the entity that sets it,
    typically this is the manager from which the places are sourced.
    The QPlaceManager::locales() field defines what language is used.
*/
QString QPlaceContactDetail::label() const
{
    return d_ptr->label;
}

/*!
    Sets the \a label of the contact detail.
*/
void QPlaceContactDetail::setLabel(const QString &label)
{
    d_ptr->label = label;
}

/*!
    Returns the value of the contact detail.
*/
QString QPlaceContactDetail::value() const
{
    return d_ptr->value;
}

/*!
    Sets the \a value of this contact detail.
*/
void QPlaceContactDetail::setValue(const QString &value)
{
    d_ptr->value = value;
}

/*!
    Clears the contact detail.
*/
void QPlaceContactDetail::clear()
{
    d_ptr->label.clear();
    d_ptr->value.clear();
}
