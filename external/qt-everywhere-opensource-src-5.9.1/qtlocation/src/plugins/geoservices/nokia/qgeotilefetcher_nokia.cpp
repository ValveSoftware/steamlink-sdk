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

#include "qgeotilefetcher_nokia.h"
#include "qgeomapreply_nokia.h"
#include "qgeotiledmap_nokia.h"
#include "qgeotiledmappingmanagerengine_nokia.h"
#include "qgeonetworkaccessmanager.h"
#include "qgeouriprovider.h"
#include "uri_constants.h"

#include <QtLocation/private/qgeotilespec_p.h>

#include <QDebug>
#include <QSize>
#include <QDir>
#include <QUrl>
#include <QTime>

#include <map>

QT_BEGIN_NAMESPACE

namespace
{
    QString sizeToStr(int size)
    {
        if (size > 256)
            return QStringLiteral("512");
        else if (size > 128)
            return QStringLiteral("256");
        else
            return QStringLiteral("128");   // 128 pixel tiles are deprecated.
    }

    bool isAerialType(const QString mapScheme)
    {
        return mapScheme.startsWith("satellite") || mapScheme.startsWith("hybrid") || mapScheme.startsWith("terrain");
    }
}
QGeoTileFetcherNokia::QGeoTileFetcherNokia(const QVariantMap &parameters,
                                           QGeoNetworkAccessManager *networkManager,
                                           QGeoTiledMappingManagerEngineNokia *engine,
                                           const QSize &tileSize,
                                           int ppi)
:   QGeoTileFetcher(engine), m_engineNokia(engine), m_networkManager(networkManager), m_ppi(ppi), m_copyrightsReply(0),
    m_baseUriProvider(new QGeoUriProvider(this, parameters, QStringLiteral("here.mapping.host"), MAP_TILES_HOST)),
    m_aerialUriProvider(new QGeoUriProvider(this, parameters, QStringLiteral("here.mapping.host.aerial"), MAP_TILES_HOST_AERIAL))
{
    Q_ASSERT(networkManager);
    m_tileSize = qMax(tileSize.width(), tileSize.height());
    m_networkManager->setParent(this);

    m_applicationId = parameters.value(QStringLiteral("here.app_id")).toString();
    m_token = parameters.value(QStringLiteral("here.token")).toString();
}

QGeoTileFetcherNokia::~QGeoTileFetcherNokia()
{
}

QGeoTiledMapReply *QGeoTileFetcherNokia::getTileImage(const QGeoTileSpec &spec)
{
    // TODO add error detection for if request.connectivityMode() != QGraphicsGeoMap::OnlineMode
    int ppi = m_ppi;
    if ((spec.mapId() == 2) || (spec.mapId() == 12) || (spec.mapId() == 21)) {
        ppi = 72;  // HiDpi apparently not supported for these maps
    } else if ((spec.mapId() >= 7 && spec.mapId() <= 11)
            || (spec.mapId() == 14)
            || (spec.mapId() == 16)
            || (spec.mapId() == 18)
            || (spec.mapId() == 20)) {
        ppi = 250; // LoDpi apparently not supported for these maps
    }

    QString rawRequest = getRequestString(spec, ppi);
    if (rawRequest.isEmpty()) {
        return new QGeoTiledMapReply(QGeoTiledMapReply::UnknownError,
                                     tr("Mapping manager no longer exists"), this);
    }

    QNetworkRequest netRequest((QUrl(rawRequest))); // The extra pair of parens disambiguates this from a function declaration
    netRequest.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

    QNetworkReply *netReply = m_networkManager->get(netRequest);

    QGeoTiledMapReply *mapReply = new QGeoMapReplyNokia(netReply, spec);

    return mapReply;
}

QString QGeoTileFetcherNokia::getRequestString(const QGeoTileSpec &spec, int ppi)
{
    if (!m_engineNokia)
        return QString();

    static const QString http("http://");
    static const QString path("/maptile/2.1/maptile/newest/");
    static const QChar slash('/');

    QString requestString = http;

    const QString mapScheme = m_engineNokia->getScheme(spec.mapId());
    if (isAerialType(mapScheme))
        requestString += m_aerialUriProvider->getCurrentHost();
    else
        requestString += m_baseUriProvider->getCurrentHost();

    requestString += path;
    requestString += mapScheme;
    requestString += slash;
    requestString += QString::number(spec.zoom());
    requestString += slash;
    requestString += QString::number(spec.x());
    requestString += slash;
    requestString += QString::number(spec.y());
    requestString += slash;
    requestString += ((ppi > 72)) ? sizeToStr(m_tileSize * 2) : sizeToStr(m_tileSize);
    static const QString slashpng("/png8");
    requestString += slashpng;

    if (!m_token.isEmpty() && !m_applicationId.isEmpty()) { // TODO: remove the if
        requestString += "?token=";
        requestString += m_token;

        requestString += "&app_id=";
        requestString += m_applicationId;
    }

    requestString += "&ppi=" + QString::number(ppi);

    requestString += "&lg=";
    requestString += getLanguageString();
    return requestString;
}

QString QGeoTileFetcherNokia::getLanguageString() const
{
    if (!m_engineNokia)
        return QStringLiteral("ENG");

    QLocale locale = m_engineNokia.data()->locale();

    // English is the default, where no ln is specified. We hardcode the languages
    // here even though the entire list is updated automagically from the server.
    // The current languages are Arabic, Chinese, Simplified Chinese, English
    // French, German, Italian, Polish, Russian and Spanish. The default is English.
    // These are actually available from the same host under the URL: /maptiler/v2/info

    switch (locale.language()) {
    case QLocale::Arabic:
        return QStringLiteral("ARA");
    case QLocale::Chinese:
        if (locale.script() == QLocale::TraditionalChineseScript)
            return QStringLiteral("CHI");
        else
            return QStringLiteral("CHT");
    case QLocale::Dutch:
        return QStringLiteral("DUT");
    case QLocale::French:
        return QStringLiteral("FRE");
    case QLocale::German:
        return QStringLiteral("GER");
    case QLocale::Gaelic:
        return QStringLiteral("GLE");
    case QLocale::Greek:
        return QStringLiteral("GRE");
    case QLocale::Hebrew:
        return QStringLiteral("HEB");
    case QLocale::Hindi:
        return QStringLiteral("HIN");
    case QLocale::Indonesian:
        return QStringLiteral("IND");
    case QLocale::Italian:
        return QStringLiteral("ITA");
    case QLocale::Persian:
        return QStringLiteral("PER");
    case QLocale::Polish:
        return QStringLiteral("POL");
    case QLocale::Portuguese:
        return QStringLiteral("POR");
    case QLocale::Russian:
        return QStringLiteral("RUS");
    case QLocale::Sinhala:
        return QStringLiteral("SIN");
    case QLocale::Spanish:
        return QStringLiteral("SPA");
    case QLocale::Thai:
        return QStringLiteral("THA");
    case QLocale::Turkish:
        return QStringLiteral("TUR");
    case QLocale::Ukrainian:
        return QStringLiteral("UKR");
    case QLocale::Urdu:
        return QStringLiteral("URD");
    case QLocale::Vietnamese:
        return QStringLiteral("VIE");

    default:
        return QStringLiteral("ENG");
    }
    // No "lg" param means that we want English.
}

QString QGeoTileFetcherNokia::token() const
{
    return m_token;
}

QString QGeoTileFetcherNokia::applicationId() const
{
    return m_applicationId;
}

void QGeoTileFetcherNokia::copyrightsFetched()
{
    if (m_engineNokia && m_copyrightsReply->error() == QNetworkReply::NoError) {
        QMetaObject::invokeMethod(m_engineNokia.data(),
                                  "loadCopyrightsDescriptorsFromJson",
                                  Qt::QueuedConnection,
                                  Q_ARG(QByteArray, m_copyrightsReply->readAll()));
    }

    m_copyrightsReply->deleteLater();
}

void QGeoTileFetcherNokia::versionFetched()
{
    if (m_engineNokia && m_versionReply->error() == QNetworkReply::NoError) {
        QMetaObject::invokeMethod(m_engineNokia.data(),
                                  "parseNewVersionInfo",
                                  Qt::QueuedConnection,
                                  Q_ARG(QByteArray, m_versionReply->readAll()));
    }

    m_versionReply->deleteLater();
}

void QGeoTileFetcherNokia::fetchCopyrightsData()
{
    QString copyrightUrl = QStringLiteral("http://");

    copyrightUrl += m_baseUriProvider->getCurrentHost();
    copyrightUrl += QStringLiteral("/maptile/2.1/copyright/newest?output=json");

    if (!token().isEmpty()) {
        copyrightUrl += QStringLiteral("&token=");
        copyrightUrl += token();
    }

    if (!applicationId().isEmpty()) {
        copyrightUrl += QStringLiteral("&app_id=");
        copyrightUrl += applicationId();
    }

    QNetworkRequest netRequest((QUrl(copyrightUrl)));
    m_copyrightsReply = m_networkManager->get(netRequest);
    if (m_copyrightsReply->error() != QNetworkReply::NoError) {
        qWarning() << __FUNCTION__ << m_copyrightsReply->errorString();
        m_copyrightsReply->deleteLater();
        return;
    }

    if (m_copyrightsReply->isFinished()) {
        copyrightsFetched();
    } else {
        connect(m_copyrightsReply, SIGNAL(finished()), this, SLOT(copyrightsFetched()));
    }
}

void QGeoTileFetcherNokia::fetchVersionData()
{
    QString versionUrl = QStringLiteral("http://");

    versionUrl += m_baseUriProvider->getCurrentHost();
    versionUrl += QStringLiteral("/maptile/2.1/version");

    if (!token().isEmpty()) {
        versionUrl += QStringLiteral("?token=");
        versionUrl += token();
    }

    if (!applicationId().isEmpty()) {
        versionUrl += QStringLiteral("&app_id=");
        versionUrl += applicationId();
    }

    QNetworkRequest netRequest((QUrl(versionUrl)));
    m_versionReply = m_networkManager->get(netRequest);

    if (m_versionReply->error() != QNetworkReply::NoError) {
        qWarning() << __FUNCTION__ << m_versionReply->errorString();
        m_versionReply->deleteLater();
        return;
    }

    if (m_versionReply->isFinished())
        versionFetched();
    else
        connect(m_versionReply, SIGNAL(finished()), this, SLOT(versionFetched()));
}

QT_END_NAMESPACE
