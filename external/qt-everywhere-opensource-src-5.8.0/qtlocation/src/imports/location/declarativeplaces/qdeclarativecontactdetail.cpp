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

#include "qdeclarativecontactdetail_p.h"

/*!
    \qmltype ContactDetails
    \instantiates QDeclarativeContactDetails
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief The ContactDetails type holds contact details for a \l Place.

    The ContactDetails type is a map of \l {QtLocation::ContactDetail}{ContactDetail} objects.
    To access contact details in the map use the \l keys() method to get the list of keys stored in
    the map and then use the \c {[]} operator to access the
    \l {QtLocation::ContactDetail}{ContactDetail} items.

    The following keys are defined in the API.  \l Plugin implementations are free to define
    additional keys.

    \list
        \li phone
        \li fax
        \li email
        \li website
    \endlist

    ContactDetails instances are only ever used in the context of \l {Place}{Places}.  It is not possible
    to create a ContactDetails instance directly or re-assign ContactDetails instances to \l {Place}{Places}.
    Modification of ContactDetails can only be accomplished via Javascript.

    \section1 Examples

    The following example shows how to access all \l {QtLocation::ContactDetail}{ContactDetails}
    and print them to the console:

    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml ContactDetails read

    The returned list of contact details is an \l {QObjectList-based model}{object list} and so can be used directly as a data model.  For example, the
    following demonstrates how to display a list of contact phone numbers in a list view:

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml ContactDetails phoneList

    The following example demonstrates how to assign a single phone number to a place in JavaScript:
    \snippet declarative/places.qml  ContactDetails write single

    The following demonstrates how to assign multiple phone numbers to a place in JavaScript:
    \snippet declarative/places.qml  ContactDetails write multiple
*/

/*!
    \qmlmethod variant ContactDetails::keys()

    Returns an array of contact detail keys currently stored in the map.
*/
QDeclarativeContactDetails::QDeclarativeContactDetails(QObject *parent)
    : QQmlPropertyMap(parent)
{
}

QVariant QDeclarativeContactDetails::updateValue(const QString &, const QVariant &input)
{
    if (input.userType() == QMetaType::QObjectStar) {
        QDeclarativeContactDetail *detail =
                        qobject_cast<QDeclarativeContactDetail *>(input.value<QObject *>());
        if (detail) {
            QVariantList varList;
            varList.append(input);
            return varList;
        }
    }

    return input;
}

/*!
    \qmltype ContactDetail
    \instantiates QDeclarativeContactDetail
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief The ContactDetail type holds a contact detail such as a phone number or a website
           address.

    The ContactDetail provides a single detail on how one could contact a \l Place.  The
    ContactDetail consists of a \l label, which is a localized string describing the contact
    method, and a \l value representing the actual contact detail.

    \section1 Examples

    The following example demonstrates how to assign a single phone number to a place in JavaScript:
    \snippet declarative/places.qml  ContactDetails write single

    The following demonstrates how to assign multiple phone numbers to a place in JavaScript:
    \snippet declarative/places.qml  ContactDetails write multiple

    Note, due to limitations of the QQmlPropertyMap, it is not possible
    to declaratively specify the contact details in QML, it can only be accomplished
    via JavaScript.
*/
QDeclarativeContactDetail::QDeclarativeContactDetail(QObject *parent)
    : QObject(parent)
{
}

QDeclarativeContactDetail::QDeclarativeContactDetail(const QPlaceContactDetail &src, QObject *parent)
    : QObject(parent), m_contactDetail(src)
{
}

QDeclarativeContactDetail::~QDeclarativeContactDetail()
{
}

/*!
    \qmlproperty QPlaceContactDetail QtLocation::ContactDetail::contactDetail

    For details on how to use this property to interface between C++ and QML see
    "\l {ContactDetail - QDeclarativeContactDetail} {Interfaces between C++ and QML Code}".
*/
void QDeclarativeContactDetail::setContactDetail(const QPlaceContactDetail &src)
{
    QPlaceContactDetail prevContactDetail = m_contactDetail;
    m_contactDetail = src;

    if (m_contactDetail.label() != prevContactDetail.label())
        emit labelChanged();
    if (m_contactDetail.value() != prevContactDetail.value())
        emit valueChanged();
}

QPlaceContactDetail QDeclarativeContactDetail::contactDetail() const
{
    return m_contactDetail;
}

/*!
    \qmlproperty string QtLocation::ContactDetail::label

    This property holds a label describing the contact detail.

    The label can potentially be localized. The language is dependent on the entity that sets it,
    typically this is the \l {Plugin}.  The \l {Plugin::locales} property defines
    what language is used.
*/
QString QDeclarativeContactDetail::label() const
{
    return m_contactDetail.label();
}

void QDeclarativeContactDetail::setLabel(const QString &label)
{
    if (m_contactDetail.label() != label) {
        m_contactDetail.setLabel(label);
        emit labelChanged();
    }
}

/*!
    \qmlproperty string QtLocation::ContactDetail::value

    This property holds the value of the contact detail which may be a phone number, an email
    address, a website url and so on.
*/
QString QDeclarativeContactDetail::value() const
{
    return m_contactDetail.value();
}

void QDeclarativeContactDetail::setValue(const QString &value)
{
    if (m_contactDetail.value() != value) {
        m_contactDetail.setValue(value);
        emit valueChanged();
    }
}
