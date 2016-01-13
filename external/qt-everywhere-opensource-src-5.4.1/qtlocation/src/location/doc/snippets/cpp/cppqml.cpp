/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtLocation/QPlaceCategory>
#include <QtLocation/QPlaceContactDetail>
#include <QtLocation/QPlace>
#include <QtLocation/QPlaceAttribute>
#include <QtLocation/QPlaceIcon>
#include <QtLocation/QPlaceUser>
#include <QtLocation/QPlaceRatings>
#include <QtLocation/QPlaceSupplier>

void cppQmlInterface(QObject *qmlObject)
{
    //! [Category get]
    QPlaceCategory category = qmlObject->property("category").value<QPlaceCategory>();
    //! [Category get]

    //! [Category set]
    qmlObject->setProperty("category", QVariant::fromValue(category));
    //! [Category set]

    //! [ContactDetail get]
    QPlaceContactDetail contactDetail = qmlObject->property("contactDetail").value<QPlaceContactDetail>();
    //! [ContactDetail get]

    //! [ContactDetail set]
    qmlObject->setProperty("contactDetail", QVariant::fromValue(contactDetail));
    //! [ContactDetail set]

    //! [Place get]
    QPlace place = qmlObject->property("place").value<QPlace>();
    //! [Place get]

    //! [Place set]
    qmlObject->setProperty("place", QVariant::fromValue(place));
    //! [Place set]

    //! [PlaceAttribute get]
    QPlaceAttribute placeAttribute = qmlObject->property("attribute").value<QPlaceAttribute>();
    //! [PlaceAttribute get]

    //! [PlaceAttribute set]
    qmlObject->setProperty("attribute", QVariant::fromValue(placeAttribute));
    //! [PlaceAttribute set]

    //! [Icon get]
    QPlaceIcon placeIcon = qmlObject->property("icon").value<QPlaceIcon>();
    //! [Icon get]

    //! [Icon set]
    qmlObject->setProperty("icon", QVariant::fromValue(placeIcon));
    //! [Icon set]

    //! [User get]
    QPlaceUser placeUser = qmlObject->property("user").value<QPlaceUser>();
    //! [User get]

    //! [User set]
    qmlObject->setProperty("user", QVariant::fromValue(placeUser));
    //! [User set]

    //! [Ratings get]
    QPlaceRatings placeRatings = qmlObject->property("ratings").value<QPlaceRatings>();
    //! [Ratings get]

    //! [Ratings set]
    qmlObject->setProperty("ratings", QVariant::fromValue(placeRatings));
    //! [Ratings set]

    //! [Supplier get]
    QPlaceSupplier placeSupplier = qmlObject->property("supplier").value<QPlaceSupplier>();
    //! [Supplier get]

    //! [Supplier set]
    qmlObject->setProperty("supplier", QVariant::fromValue(placeSupplier));
    //! [Supplier set]
}

