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

#include "qdeclarativeplaceuser_p.h"

QT_USE_NAMESPACE

/*!
    \qmltype User
    \instantiates QDeclarativePlaceUser
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-places
    \ingroup qml-QtLocation5-places-data
    \since Qt Location 5.5

    \brief The User type identifies a user who contributed a particular \l Place content item.

    Each \l Place content item has an associated user who contributed the content.  This type
    provides information about that user.

    \sa ImageModel, ReviewModel, EditorialModel

    \section1 Example

    The following example shows how to display information about the user who
    submitted an editorial:

    \snippet declarative/places.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/places.qml EditorialModel
*/

QDeclarativePlaceUser::QDeclarativePlaceUser(QObject *parent)
        : QObject(parent) {}

QDeclarativePlaceUser::QDeclarativePlaceUser(const QPlaceUser &user,
        QObject *parent)
        : QObject(parent),
        m_user(user) {}

QDeclarativePlaceUser::~QDeclarativePlaceUser() {}

/*!
    \qmlproperty QPlaceUser QtLocation::User::user

    For details on how to use this property to interface between C++ and QML see
    "\l {User - QPlaceUser} {Interfaces between C++ and QML Code}".
*/
void QDeclarativePlaceUser::setUser(const QPlaceUser &user)
{
    QPlaceUser previousUser = m_user;
    m_user = user;

    if (m_user.userId() != previousUser.userId())
        emit userIdChanged();

    if (m_user.name() != previousUser.name())
        emit nameChanged();
}

QPlaceUser QDeclarativePlaceUser::user() const
{
    return m_user;
}

/*!
    \qmlproperty string QtLocation::User::userId

    This property holds the unique identifier of the user.
*/

void QDeclarativePlaceUser::setUserId(const QString &id)
{
    if (m_user.userId() == id)
        return;

    m_user.setUserId(id);
    emit userIdChanged();
}

QString QDeclarativePlaceUser::userId() const
{
    return m_user.userId();
}

/*!
    \qmlproperty string QtLocation::User::name

    This property holds the name of a user.
*/
void QDeclarativePlaceUser::setName(const QString &name)
{
    if (m_user.name() == name)
        return;

    m_user.setName(name);
    emit nameChanged();
}

QString QDeclarativePlaceUser::name() const
{
    return m_user.name();
}

