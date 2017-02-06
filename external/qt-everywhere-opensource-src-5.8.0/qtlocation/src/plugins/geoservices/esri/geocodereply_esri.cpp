/****************************************************************************
**
** Copyright (C) 2013-2016 Esri <contracts@esri.com>
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

#include "geocodereply_esri.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGeoCoordinate>
#include <QGeoAddress>
#include <QGeoLocation>
#include <QGeoRectangle>

QT_BEGIN_NAMESPACE

GeoCodeReplyEsri::GeoCodeReplyEsri(QNetworkReply *reply, OperationType operationType,
                                   QObject *parent) :
    QGeoCodeReply(parent), m_reply(reply), m_operationType(operationType)
{
    connect(m_reply, SIGNAL(finished()), this, SLOT(networkReplyFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkReplyError(QNetworkReply::NetworkError)));

    setLimit(1);
    setOffset(0);
}

GeoCodeReplyEsri::~GeoCodeReplyEsri()
{
    if (m_reply)
        m_reply->deleteLater();
}

void GeoCodeReplyEsri::abort()
{
    if (!m_reply)
        return;

    m_reply->abort();
    QGeoCodeReply::abort();

    m_reply->deleteLater();
    m_reply = Q_NULLPTR;
}

void GeoCodeReplyEsri::networkReplyError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    if (!m_reply)
        return;

    setError(QGeoCodeReply::CommunicationError, m_reply->errorString());

    m_reply->deleteLater();
    m_reply = Q_NULLPTR;
}

void GeoCodeReplyEsri::networkReplyFinished()
{
    if (!m_reply)
        return;

    if (m_reply->error() != QNetworkReply::NoError)
    {
        setError(QGeoCodeReply::CommunicationError, m_reply->errorString());
        m_reply->deleteLater();
        m_reply = Q_NULLPTR;
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());

    if (document.isObject()) {
        QJsonObject object = document.object();

        switch (operationType()) {
        case Geocode:
        {
            QJsonArray candidates = object.value(QStringLiteral("candidates")).toArray();

            QList<QGeoLocation> locations;

            for (int i = 0; i < candidates.count(); i++) {
                if (!candidates.at(i).isObject())
                    continue;

                QJsonObject candidate = candidates.at(i).toObject();

                QGeoLocation location = parseCandidate(candidate);
                locations.append(location);
            }

            setLocations(locations);
            setFinished(true);
        }
            break;

        case ReverseGeocode:
        {
            QGeoLocation location = parseAddress(object);

            QList<QGeoLocation> locations;
            locations.append(location);

            setLocations(locations);
            setFinished(true);
        }
            break;
        }

    } else {
        setError(QGeoCodeReply::CommunicationError, QStringLiteral("Unknown document"));
    }

    m_reply->deleteLater();
    m_reply = Q_NULLPTR;
}

QGeoLocation GeoCodeReplyEsri::parseAddress(const QJsonObject& object)
{
    QJsonObject addressObject = object.value(QStringLiteral("address")).toObject();

    QGeoAddress address;

    address.setCountryCode(addressObject.value(QStringLiteral("CountryCode")).toString());
    address.setState(addressObject.value(QStringLiteral("Region")).toString());
    address.setCity(addressObject.value(QStringLiteral("City")).toString());
    address.setDistrict(addressObject.value(QStringLiteral("Subregion")).toString());
    address.setPostalCode(addressObject.value(QStringLiteral("Postal")).toString());
    address.setStreet(addressObject.value(QStringLiteral("Address")).toString());

    QGeoCoordinate coordinate;

    QJsonObject locationObject = object.value(QStringLiteral("location")).toObject();

    coordinate.setLongitude(locationObject.value(QStringLiteral("x")).toDouble());
    coordinate.setLatitude(locationObject.value(QStringLiteral("y")).toDouble());

    QGeoLocation location;

    location.setCoordinate(coordinate);
    location.setAddress(address);

    return location;
}

QGeoLocation GeoCodeReplyEsri::parseCandidate(const QJsonObject& candidate)
{
    QGeoCoordinate coordinate;

    QJsonObject locationObject = candidate.value(QStringLiteral("location")).toObject();

    coordinate.setLongitude(locationObject.value(QStringLiteral("x")).toDouble());
    coordinate.setLatitude(locationObject.value(QStringLiteral("y")).toDouble());

    QGeoRectangle extent;

    if (candidate.contains(QStringLiteral("extent"))) {
        QJsonObject extentObject = candidate.value(QStringLiteral("extent")).toObject();

        extent.setTopLeft(QGeoCoordinate(extentObject.value(QStringLiteral("ymin")).toDouble(),
                                         extentObject.value(QStringLiteral("xmin")).toDouble()));

        extent.setBottomRight(QGeoCoordinate(extentObject.value(QStringLiteral("ymax")).toDouble(),
                                             extentObject.value(QStringLiteral("xmax")).toDouble()));
    }

    QJsonObject attributesObject = candidate.value(QStringLiteral("attributes")).toObject();

    QGeoAddress address;

    address.setText(candidate.value(QStringLiteral("address")).toString());

    address.setCountry(attributesObject.value(QStringLiteral("Country")).toString());
    address.setCountryCode(attributesObject.value(QStringLiteral("Country")).toString());
    address.setState(attributesObject.value(QStringLiteral("Region")).toString());
    address.setCity(attributesObject.value(QStringLiteral("City")).toString());
    address.setDistrict(attributesObject.value(QStringLiteral("Subregion")).toString());
    address.setPostalCode(attributesObject.value(QStringLiteral("Postal")).toString());

    QGeoLocation location;

    location.setCoordinate(coordinate);
    location.setBoundingBox(extent);
    location.setAddress(address);

    return location;
}

QT_END_NAMESPACE
