/****************************************************************************
**
** Copyright (C) 2016 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qgeocodereplyosm.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoAddress>
#include <QtPositioning/QGeoLocation>
#include <QtPositioning/QGeoRectangle>

QT_BEGIN_NAMESPACE

QGeoCodeReplyOsm::QGeoCodeReplyOsm(QNetworkReply *reply, QObject *parent)
:   QGeoCodeReply(parent), m_reply(reply)
{
    connect(m_reply, SIGNAL(finished()), this, SLOT(networkReplyFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkReplyError(QNetworkReply::NetworkError)));

    setLimit(1);
    setOffset(0);
}

QGeoCodeReplyOsm::~QGeoCodeReplyOsm()
{
    if (m_reply)
        m_reply->deleteLater();
}

void QGeoCodeReplyOsm::abort()
{
    if (!m_reply)
        return;

    m_reply->abort();

    m_reply->deleteLater();
    m_reply = 0;
}

static QGeoAddress parseAddressObject(const QJsonObject &object)
{
    QGeoAddress address;
    address.setText(object.value(QStringLiteral("display_name")).toString());
    QJsonObject ao = object.value(QStringLiteral("address")).toObject();
    // setCountry
    address.setCountry(ao.value(QStringLiteral("country")).toString());
    // setCountryCode
    address.setCountryCode(ao.value(QStringLiteral("country_code")).toString());
    // setState
    address.setState(ao.value(QStringLiteral("state")).toString());
    // setCity
    if (ao.contains(QLatin1String("city")))
        address.setCity(ao.value(QStringLiteral("city")).toString());
    else if (ao.contains(QLatin1String("town")))
        address.setCity(ao.value(QLatin1String("town")).toString());
    else if (ao.contains(QLatin1String("village")))
        address.setCity(ao.value(QLatin1String("village")).toString());
    else
        address.setCity(ao.value(QLatin1String("hamlet")).toString());
    // setDistrict
    address.setDistrict(ao.value(QStringLiteral("suburb")).toString());
    // setPostalCode
    address.setPostalCode(ao.value(QStringLiteral("postcode")).toString());
    // setStreet
    address.setStreet(ao.value(QStringLiteral("road")).toString());
    return address;
}

void QGeoCodeReplyOsm::networkReplyFinished()
{
    if (!m_reply)
        return;

    if (m_reply->error() != QNetworkReply::NoError)
        return;

    QList<QGeoLocation> locations;
    QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());

    if (document.isObject()) {
        QJsonObject object = document.object();

        QGeoCoordinate coordinate;

        coordinate.setLatitude(object.value(QStringLiteral("lat")).toString().toDouble());
        coordinate.setLongitude(object.value(QStringLiteral("lon")).toString().toDouble());

        QGeoLocation location;
        location.setCoordinate(coordinate);
        location.setAddress(parseAddressObject(object));

        locations.append(location);

        setLocations(locations);
    } else if (document.isArray()) {
        QJsonArray results = document.array();

        for (int i = 0; i < results.count(); ++i) {
            if (!results.at(i).isObject())
                continue;

            QJsonObject object = results.at(i).toObject();

            QGeoCoordinate coordinate;

            coordinate.setLatitude(object.value(QStringLiteral("lat")).toString().toDouble());
            coordinate.setLongitude(object.value(QStringLiteral("lon")).toString().toDouble());

            QGeoRectangle rectangle;

            if (object.contains(QStringLiteral("boundingbox"))) {
                QJsonArray a = object.value(QStringLiteral("boundingbox")).toArray();
                if (a.count() == 4) {
                    rectangle.setTopLeft(QGeoCoordinate(a.at(1).toString().toDouble(),
                                                        a.at(2).toString().toDouble()));
                    rectangle.setBottomRight(QGeoCoordinate(a.at(0).toString().toDouble(),
                                                            a.at(3).toString().toDouble()));
                }
            }

            QGeoLocation location;
            location.setCoordinate(coordinate);
            location.setBoundingBox(rectangle);
            location.setAddress(parseAddressObject(object));
            locations.append(location);
        }

    }

    setLocations(locations);
    setFinished(true);

    m_reply->deleteLater();
    m_reply = 0;
}

void QGeoCodeReplyOsm::networkReplyError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    if (!m_reply)
        return;

    setError(QGeoCodeReply::CommunicationError, m_reply->errorString());

    m_reply->deleteLater();
    m_reply = 0;
}

QT_END_NAMESPACE
