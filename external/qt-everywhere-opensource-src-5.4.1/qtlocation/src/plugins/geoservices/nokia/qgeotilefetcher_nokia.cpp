/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
** This file is part of the Nokia services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file NOKIA_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Nokia services
** plugin source code.
**
****************************************************************************/

#include "qgeotilefetcher_nokia.h"
#include "qgeomapreply_nokia.h"
#include "qgeotiledmapdata_nokia.h"
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
    QString sizeToStr(const QSize &size)
    {
        if (size.height() >= 512 || size.width() >= 512)
            return QStringLiteral("512");
        else if (size.height() >= 256 || size.width() >= 256)
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
                                           const QSize &tileSize)
:   QGeoTileFetcher(engine), m_engineNokia(engine), m_networkManager(networkManager),
    m_tileSize(tileSize), m_copyrightsReply(0),
    m_baseUriProvider(new QGeoUriProvider(this, parameters, "mapping.host", MAP_TILES_HOST)),
    m_aerialUriProvider(new QGeoUriProvider(this, parameters, "mapping.host.aerial", MAP_TILES_HOST_AERIAL))
{
    Q_ASSERT(networkManager);
    m_networkManager->setParent(this);

    m_applicationId = parameters.value(QStringLiteral("app_id")).toString();
    m_token = parameters.value(QStringLiteral("token")).toString();
}

QGeoTileFetcherNokia::~QGeoTileFetcherNokia()
{
}

QGeoTiledMapReply *QGeoTileFetcherNokia::getTileImage(const QGeoTileSpec &spec)
{
    // TODO add error detection for if request.connectivityMode() != QGraphicsGeoMap::OnlineMode
    QString rawRequest = getRequestString(spec);
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

QString QGeoTileFetcherNokia::getRequestString(const QGeoTileSpec &spec)
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
    requestString += sizeToStr(m_tileSize);
    static const QString slashpng("/png8");
    requestString += slashpng;

    if (!m_token.isEmpty() && !m_applicationId.isEmpty()) {
        requestString += "?token=";
        requestString += m_token;

        requestString += "&app_id=";
        requestString += m_applicationId;
    }

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
