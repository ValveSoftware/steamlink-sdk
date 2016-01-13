/****************************************************************************
**
** Copyright (C) 2013 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

        QJsonObject ao = object.value(QStringLiteral("address")).toObject();

        QGeoAddress address;
        address.setText(object.value(QStringLiteral("display_name")).toString());
        address.setCountry(ao.value(QStringLiteral("country")).toString());
        address.setCountryCode(ao.value(QStringLiteral("country_code")).toString());
        address.setState(ao.value(QStringLiteral("state")).toString());
        address.setCity(ao.value(QStringLiteral("city")).toString());
        address.setDistrict(ao.value(QStringLiteral("suburb")).toString());
        address.setPostalCode(ao.value(QStringLiteral("postcode")).toString());
        address.setStreet(ao.value(QStringLiteral("road")).toString());

        QGeoLocation location;
        location.setCoordinate(coordinate);
        location.setAddress(address);

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

            QJsonObject ao = object.value(QStringLiteral("address")).toObject();

            QGeoAddress address;
            address.setText(object.value(QStringLiteral("display_name")).toString());
            address.setCountry(ao.value(QStringLiteral("country")).toString());
            address.setCountryCode(ao.value(QStringLiteral("country_code")).toString());
            address.setState(ao.value(QStringLiteral("state")).toString());
            address.setCity(ao.value(QStringLiteral("city")).toString());
            address.setDistrict(ao.value(QStringLiteral("suburb")).toString());
            address.setPostalCode(ao.value(QStringLiteral("postcode")).toString());
            address.setStreet(ao.value(QStringLiteral("road")).toString());

            QGeoLocation location;
            location.setCoordinate(coordinate);
            location.setBoundingBox(rectangle);
            location.setAddress(address);
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
