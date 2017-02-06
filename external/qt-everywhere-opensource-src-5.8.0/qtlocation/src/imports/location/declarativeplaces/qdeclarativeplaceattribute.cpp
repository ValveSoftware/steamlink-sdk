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

#include "qdeclarativeplaceattribute_p.h"

/*!
    \qmltype ExtendedAttributes
    \instantiates QQmlPropertyMap
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief The ExtendedAttributes type holds additional data about a \l Place.

    The ExtendedAttributes type is a map of \l {PlaceAttribute}{PlaceAttributes}.  To access
    attributes in the map use the \l keys() method to get the list of keys stored in the map and
    use the \c {[]} operator to access the \l PlaceAttribute items.

    The following are standard keys that are defined by the API.  \l Plugin
    implementations are free to define additional keys.  Custom keys should
    be qualified by a unique prefix to avoid clashes.
    \table
        \header
            \li key
            \li description
        \row
            \li openingHours
            \li The trading hours of the place
        \row
            \li payment
            \li The types of payment the place accepts, for example visa, mastercard.
        \row
            \li x_provider
            \li The name of the provider that a place is sourced from
        \row
            \li x_id_<provider> (for example x_id_here)
            \li An alternative identifier which identifies the place from the
               perspective of the specified provider.
    \endtable

    Some plugins may not support attributes at all, others may only support a
    certain set, others still may support a dynamically changing set of attributes
    over time or even allow attributes to be arbitrarily defined by the client
    application.  The attributes could also vary on a place by place basis,
    for example one place may have opening hours while another does not.
    Consult the \l {Plugin References and Parameters}{plugin
    references} for details.

    Some attributes may not be intended to be readable by end users, the label field
    of such attributes is empty to indicate this fact.

    \note ExtendedAttributes instances are only ever used in the context of \l {Place}s.  It is not
    possible to create an ExtendedAttributes instance directly or re-assign a \l {Place}'s
    ExtendedAttributes property.  Modification of ExtendedAttributes can only be accomplished
    via Javascript.

    The following example shows how to access all \l {PlaceAttribute}{PlaceAttributes} and print
    them to the console:

    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml ExtendedAttributes read

    The following example shows how to assign and modify an attribute:
    \snippet declarative/places.qml ExtendedAttributes write

    \sa PlaceAttribute, QQmlPropertyMap
*/

/*!
    \qmlmethod variant ExtendedAttributes::keys()

    Returns an array of place attribute keys currently stored in the map.
*/

/*!
    \qmlsignal void ExtendedAttributes::valueChanged(string key, variant value)

    This signal is emitted when the set of attributes changes. \a key is the key
    corresponding to the \a value that was changed.

    The corresponding handler is \c onValueChanged.
*/

/*!
    \qmltype PlaceAttribute
    \instantiates QDeclarativePlaceAttribute
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief The PlaceAttribute type holds generic place attribute information.

    A place attribute stores an additional piece of information about a \l Place that is not
    otherwise exposed through the \l Place type.  A PlaceAttribute is a textual piece of data,
    accessible through the \l text property, and a \l label.  Both the \l text and \l label
    properties are intended to be displayed to the user.  PlaceAttributes are stored in an
    \l ExtendedAttributes map with a unique key.

    The following example shows how to display all attributes in a list:

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml ExtendedAttributes

    The following example shows how to assign and modify an attribute:
    \snippet declarative/places.qml ExtendedAttributes write
*/

QDeclarativePlaceAttribute::QDeclarativePlaceAttribute(QObject *parent)
    : QObject(parent)
{
}

QDeclarativePlaceAttribute::QDeclarativePlaceAttribute(const QPlaceAttribute &src, QObject *parent)
    : QObject(parent),m_attribute(src)
{
}

QDeclarativePlaceAttribute::~QDeclarativePlaceAttribute()
{
}

/*!
    \qmlproperty QPlaceAttribute PlaceAttribute::attribute

    For details on how to use this property to interface between C++ and QML see
    "\l {PlaceAttribute - QPlaceAttribute} {Interfaces between C++ and QML Code}".
*/
void QDeclarativePlaceAttribute::setAttribute(const QPlaceAttribute &src)
{
    QPlaceAttribute prevAttribute = m_attribute;
    m_attribute = src;

    if (m_attribute.label() != prevAttribute.label())
        emit labelChanged();
    if (m_attribute.text() != prevAttribute.text())
        emit textChanged();
}

QPlaceAttribute QDeclarativePlaceAttribute::attribute() const
{
    return m_attribute;
}

/*!
    \qmlproperty string PlaceAttribute::label

    This property holds the attribute label which is a user visible string
    describing the attribute.
*/
void QDeclarativePlaceAttribute::setLabel(const QString &label)
{
    if (m_attribute.label() != label) {
        m_attribute.setLabel(label);
        emit labelChanged();
    }
}

QString QDeclarativePlaceAttribute::label() const
{
    return m_attribute.label();
}

/*!
    \qmlproperty string PlaceAttribute::text

    This property holds the attribute text which can be used to show additional information about the place.
*/
void QDeclarativePlaceAttribute::setText(const QString &text)
{
    if (m_attribute.text() != text) {
        m_attribute.setText(text);
        emit  textChanged();
    }
}

QString QDeclarativePlaceAttribute::text() const
{
    return m_attribute.text();
}
