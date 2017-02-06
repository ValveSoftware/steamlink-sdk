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

#include <QtLocation/QLocation>

QT_BEGIN_NAMESPACE

namespace QLocation {

/*!
    \namespace QLocation
    \inmodule QtLocation
    \keyword QLocation Namespace

    \brief The QLocation namespace contains miscellaneous identifiers used throughout the
           QtLocation module.
*/

/*!
    \enum QLocation::Visibility

    Defines the visibility of a QPlace or QPlaceCategory.

    \value UnspecifiedVisibility    No explicit visibility has been defined.
    \value DeviceVisibility         Places and categories with DeviceVisibility are only stored on
                                    the local device.
    \value PrivateVisibility        Places and categories with PrivateVisibility are only visible
                                    to the current user.  The data may be stored either locally or
                                    on a remote service or both.
    \value PublicVisibility         Places and categories with PublicVisibility are visible to
                                    everyone.

    A particular manager may support one or more visibility scopes.  For example a manager from one provider may only provide places
    that are public to everyone, whilst another may provide both public and private places.

    \note The meaning of unspecified visibility depends on the context it is used.

    When \e saving a place or category, the
    default visibility is unspecified meaning that the manager chooses an appropriate visibility scope for the item.

    When \e searching for places, unspecified means that places of any scope is returned.
*/
}

QT_END_NAMESPACE
