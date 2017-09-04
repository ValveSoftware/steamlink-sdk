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

#include "qgeocodingmanagerengine_nokia.h"
#include "qgeocodereply_nokia.h"
#include "marclanguagecodes.h"
#include "qgeonetworkaccessmanager.h"
#include "qgeouriprovider.h"
#include "uri_constants.h"

#include <QtPositioning/QGeoAddress>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoShape>

#include <QUrl>
#include <QMap>
#include <QStringList>

QT_BEGIN_NAMESPACE

QGeoCodingManagerEngineNokia::QGeoCodingManagerEngineNokia(
        QGeoNetworkAccessManager *networkManager,
        const QVariantMap &parameters,
        QGeoServiceProvider::Error *error,
        QString *errorString)
        : QGeoCodingManagerEngine(parameters)
        , m_networkManager(networkManager)
        , m_uriProvider(new QGeoUriProvider(this, parameters, QStringLiteral("here.geocoding.host"), GEOCODING_HOST))
        , m_reverseGeocodingUriProvider(new QGeoUriProvider(this, parameters, QStringLiteral("here.reversegeocoding.host"), REVERSE_GEOCODING_HOST))
{
    Q_ASSERT(networkManager);
    m_networkManager->setParent(this);

    if (parameters.contains(QStringLiteral("here.token")))
        m_token = parameters.value(QStringLiteral("here.token")).toString();

    if (parameters.contains(QStringLiteral("here.app_id")))
        m_applicationId = parameters.value(QStringLiteral("here.app_id")).toString();

    if (error)
        *error = QGeoServiceProvider::NoError;

    if (errorString)
        *errorString = "";
}

QGeoCodingManagerEngineNokia::~QGeoCodingManagerEngineNokia() {}

QString QGeoCodingManagerEngineNokia::getAuthenticationString() const
{
    QString authenticationString;

    if (!m_token.isEmpty() && !m_applicationId.isEmpty()) {
        authenticationString += "?app_code=";
        authenticationString += m_token;

        authenticationString += "&app_id=";
        authenticationString += m_applicationId;
    }

    return authenticationString;
}


QGeoCodeReply *QGeoCodingManagerEngineNokia::geocode(const QGeoAddress &address,
                                                     const QGeoShape &bounds)
{
    QString requestString = "https://";
    requestString += m_uriProvider->getCurrentHost();
    requestString += "/6.2/geocode.json";

    requestString += getAuthenticationString();
    requestString += "&gen=9";

    requestString += "&language=";
    requestString += languageToMarc(locale().language());

    bool manualBoundsRequired = false;
    if (bounds.type() == QGeoShape::RectangleType) {
        QGeoRectangle rect(bounds);
        if (rect.isValid()) {
            requestString += "&bbox=";
            requestString += trimDouble(rect.topLeft().latitude());
            requestString += ",";
            requestString += trimDouble(rect.topLeft().longitude());
            requestString += ";";
            requestString += trimDouble(rect.bottomRight().latitude());
            requestString += ",";
            requestString += trimDouble(rect.bottomRight().longitude());
        }
    } else if (bounds.type() == QGeoShape::CircleType) {
        QGeoCircle circ(bounds);
        if (circ.isValid()) {
            requestString += "?prox=";
            requestString += trimDouble(circ.center().latitude());
            requestString += ",";
            requestString += trimDouble(circ.center().longitude());
            requestString += ",";
            requestString += trimDouble(circ.radius());
        }
    } else {
        manualBoundsRequired = true;
    }

    if (address.country().isEmpty()) {
        QStringList parts;

        if (!address.state().isEmpty())
            parts << address.state();

        if (!address.city().isEmpty())
            parts << address.city();

        if (!address.postalCode().isEmpty())
            parts << address.postalCode();

        if (!address.street().isEmpty())
            parts << address.street();

        requestString += "&searchtext=";
        requestString += parts.join("+").replace(' ', '+');
    } else {
        requestString += "&country=";
        requestString += address.country();

        if (!address.state().isEmpty()) {
            requestString += "&state=";
            requestString += address.state();
        }

        if (!address.city().isEmpty()) {
            requestString += "&city=";
            requestString += address.city();
        }

        if (!address.postalCode().isEmpty()) {
            requestString += "&postalcode=";
            requestString += address.postalCode();
        }

        if (!address.street().isEmpty()) {
            requestString += "&street=";
            requestString += address.street();
        }
    }

    return geocode(requestString, bounds, manualBoundsRequired);
}

QGeoCodeReply *QGeoCodingManagerEngineNokia::geocode(const QString &address,
                                                     int limit,
                                                     int offset,
                                                     const QGeoShape &bounds)
{
    QString requestString = "https://";
    requestString += m_uriProvider->getCurrentHost();
    requestString += "/6.2/geocode.json";

    requestString += getAuthenticationString();
    requestString += "&gen=9";

    requestString += "&language=";
    requestString += languageToMarc(locale().language());

    requestString += "&searchtext=";
    requestString += QString(address).replace(' ', '+');

    if (limit > 0) {
        requestString += "&maxresults=";
        requestString += QString::number(limit);
    }
    if (offset > 0) {
        // We cannot do this precisely, since HERE doesn't allow
        // precise result-set offset to be supplied; instead, it
        // returns "pages" of results at a time.
        // So, we tell HERE which page of results we want, and the
        // client has to filter out duplicates if they changed
        // the limit param since the last call.
        requestString += "&pageinformation=";
        requestString += QString::number(offset/limit);
    }

    bool manualBoundsRequired = false;
    if (bounds.type() == QGeoShape::RectangleType) {
        QGeoRectangle rect(bounds);
        if (rect.isValid()) {
            requestString += "&bbox=";
            requestString += trimDouble(rect.topLeft().latitude());
            requestString += ",";
            requestString += trimDouble(rect.topLeft().longitude());
            requestString += ";";
            requestString += trimDouble(rect.bottomRight().latitude());
            requestString += ",";
            requestString += trimDouble(rect.bottomRight().longitude());
        }
    } else if (bounds.type() == QGeoShape::CircleType) {
        QGeoCircle circ(bounds);
        if (circ.isValid()) {
            requestString += "?prox=";
            requestString += trimDouble(circ.center().latitude());
            requestString += ",";
            requestString += trimDouble(circ.center().longitude());
            requestString += ",";
            requestString += trimDouble(circ.radius());
        }
    } else {
        manualBoundsRequired = true;
    }

    return geocode(requestString, bounds, manualBoundsRequired, limit, offset);
}

QGeoCodeReply *QGeoCodingManagerEngineNokia::geocode(QString requestString,
                                                     const QGeoShape &bounds,
                                                     bool manualBoundsRequired,
                                                     int limit,
                                                     int offset)
{
    QGeoCodeReplyNokia *reply = new QGeoCodeReplyNokia(
                m_networkManager->get(QNetworkRequest(QUrl(requestString))),
                limit, offset, bounds, manualBoundsRequired, this);

    connect(reply, &QGeoCodeReplyNokia::finished,
            this, &QGeoCodingManagerEngineNokia::placesFinished);

    connect(reply, static_cast<void (QGeoCodeReply::*)(QGeoCodeReply::Error, const QString &)>(&QGeoCodeReplyNokia::error),
            this, &QGeoCodingManagerEngineNokia::placesError);

    return reply;
}

QGeoCodeReply *QGeoCodingManagerEngineNokia::reverseGeocode(const QGeoCoordinate &coordinate,
                                                            const QGeoShape &bounds)
{
    QString requestString = "https://";
    requestString += m_reverseGeocodingUriProvider->getCurrentHost();
    requestString += "/6.2/reversegeocode.json";

    requestString += getAuthenticationString();
    requestString += "&gen=9";

    requestString += "&mode=retrieveAddresses";

    requestString += "&prox=";
    requestString += trimDouble(coordinate.latitude());
    requestString += ",";
    requestString += trimDouble(coordinate.longitude());

    bool manualBoundsRequired = false;
    if (bounds.type() == QGeoShape::CircleType) {
        QGeoCircle circ(bounds);
        if (circ.isValid() && circ.center() == coordinate) {
            requestString += ",";
            requestString += trimDouble(circ.radius());
        } else {
            manualBoundsRequired = true;
        }
    } else {
        manualBoundsRequired = true;
    }

    requestString += "&language=";
    requestString += languageToMarc(locale().language());

    return geocode(requestString, bounds, manualBoundsRequired);
}

QString QGeoCodingManagerEngineNokia::trimDouble(double degree, int decimalDigits)
{
    QString sDegree = QString::number(degree, 'g', decimalDigits);

    int index = sDegree.indexOf('.');

    if (index == -1)
        return sDegree;
    else
        return QString::number(degree, 'g', decimalDigits + index);
}

void QGeoCodingManagerEngineNokia::placesFinished()
{
    QGeoCodeReply *reply = qobject_cast<QGeoCodeReply *>(sender());

    if (!reply)
        return;

    if (receivers(SIGNAL(finished(QGeoCodeReply*))) == 0) {
        reply->deleteLater();
        return;
    }

    emit finished(reply);
}

void QGeoCodingManagerEngineNokia::placesError(QGeoCodeReply::Error error, const QString &errorString)
{
    QGeoCodeReply *reply = qobject_cast<QGeoCodeReply *>(sender());

    if (!reply)
        return;

    if (receivers(SIGNAL(error(QGeoCodeReply*,QGeoCodeReply::Error,QString))) == 0) {
        reply->deleteLater();
        return;
    }

    emit this->error(reply, error, errorString);
}

QString QGeoCodingManagerEngineNokia::languageToMarc(QLocale::Language language)
{
    uint offset = 3 * (uint(language));
    if (language == QLocale::C || offset + 3 > sizeof(marc_language_code_list))
        return QLatin1String("eng");

    const unsigned char *c = marc_language_code_list + offset;
    if (c[0] == 0)
        return QLatin1String("eng");

    QString code(3, Qt::Uninitialized);
    code[0] = ushort(c[0]);
    code[1] = ushort(c[1]);
    code[2] = ushort(c[2]);

    return code;
}

QT_END_NAMESPACE
