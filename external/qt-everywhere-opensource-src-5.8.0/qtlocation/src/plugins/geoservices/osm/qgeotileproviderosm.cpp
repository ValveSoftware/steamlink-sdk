/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qgeotileproviderosm.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QDebug>

QT_BEGIN_NAMESPACE

static const int maxValidZoom = 30;
static const QDateTime defaultTs = QDateTime::fromString(QStringLiteral("2016-06-01T00:00:00"), Qt::ISODate);

QGeoTileProviderOsm::QGeoTileProviderOsm(QNetworkAccessManager *nm,
                                         const QGeoMapType &mapType,
                                         const QVector<TileProvider *> &providers)
:   m_nm(nm), m_provider(nullptr), m_mapType(mapType), m_status(Idle)
{
    for (int i = 0; i < providers.size(); ++i) {
        TileProvider *p = providers[i];
        if (!m_provider)
            m_providerId = i;
        addProvider(p);
    }

    if (!m_provider || m_provider->isValid())
        m_status = Resolved;
}

QGeoTileProviderOsm::~QGeoTileProviderOsm()
{
}

QUrl QGeoTileProviderOsm::tileAddress(int x, int y, int z) const
{
    if (m_status != Resolved || !m_provider)
        return QUrl();
    return m_provider->tileAddress(x, y, z);
}

QString QGeoTileProviderOsm::mapCopyRight() const
{
    if (m_status != Resolved || !m_provider)
        return QString();
    return m_provider->mapCopyRight();
}

QString QGeoTileProviderOsm::dataCopyRight() const
{
    if (m_status != Resolved || !m_provider)
        return QString();
    return m_provider->dataCopyRight();
}

QString QGeoTileProviderOsm::styleCopyRight() const
{
    if (m_status != Resolved || !m_provider)
        return QString();
    return m_provider->styleCopyRight();
}

QString QGeoTileProviderOsm::format() const
{
    if (m_status != Resolved || !m_provider)
        return QString();
    return m_provider->format();
}

int QGeoTileProviderOsm::minimumZoomLevel() const
{
    if (m_status != Resolved || !m_provider)
        return 0;
    return m_provider->minimumZoomLevel();
}

int QGeoTileProviderOsm::maximumZoomLevel() const
{
    if (m_status != Resolved || !m_provider)
        return 20;
    return m_provider->maximumZoomLevel();
}

bool QGeoTileProviderOsm::isHighDpi() const
{
    if (!m_provider)
        return false;
    return m_provider->isHighDpi();
}

const QDateTime QGeoTileProviderOsm::timestamp() const
{
    if (!m_provider)
        return QDateTime();
    return m_provider->timestamp();
}

const QGeoMapType &QGeoTileProviderOsm::mapType() const
{
    return m_mapType;
}

bool QGeoTileProviderOsm::isValid() const
{
    if (m_status != Resolved || !m_provider)
        return false;
    return m_provider->isValid();
}

bool QGeoTileProviderOsm::isResolved() const
{
    return (m_status == Resolved);
}

void QGeoTileProviderOsm::resolveProvider()
{
    if (m_status == Resolved || m_status == Resolving)
        return;

    m_status = Resolving;
    // Provider can't be null while on Idle status.
    connect(m_provider, &TileProvider::resolutionFinished, this, &QGeoTileProviderOsm::onResolutionFinished);
    connect(m_provider, &TileProvider::resolutionError, this, &QGeoTileProviderOsm::onResolutionError);
    m_provider->resolveProvider();
}

void QGeoTileProviderOsm::disableRedirection()
{
    if (m_provider && m_provider->isValid())
        return;
    bool found = false;
    for (TileProvider *p: m_providerList) {
        if (p->isValid() && !found) {
            m_provider = p;
            found = true;
        }
        p->disconnect(this);
    }
}

void QGeoTileProviderOsm::onResolutionFinished(TileProvider *provider)
{
    Q_UNUSED(provider)
    // provider and m_provider are the same, at this point. m_status is Resolving.
    m_status = Resolved;
    emit resolutionFinished(this);
}

void QGeoTileProviderOsm::onResolutionError(TileProvider *provider)
{
    Q_UNUSED(provider)
    // provider and m_provider are the same at this point. m_status is Resolving.
    if (m_provider->isInvalid()) {
        m_provider = nullptr;
        m_status = Resolved;
        if (m_providerId >= m_providerList.size() -1) { // no hope left
            emit resolutionError(this);
            return;
        }
        // Advance the pointer in the provider list, and possibly start resolution on the next in the list.
        for (int i = m_providerId + 1; i < m_providerList.size(); ++i) {
            m_providerId = i;
            TileProvider *p = m_providerList[m_providerId];
            if (!p->isInvalid()) {
                m_provider = p;
                if (!p->isValid()) {
                    m_status = Idle;
#if 0  // leaving triggering the retry to the tile fetcher, instead of constantly spinning it in here.
                    m_status = Resolving;
                    p->resolveProvider();
#endif
                    emit resolutionRequired();
                }
                break;
            }
        }
        if (!m_provider)
            emit resolutionError(this);
    } else if (m_provider->isValid()) {
        m_status = Resolved;
        emit resolutionFinished(this);
    } else { // still not resolved. But network error is recoverable.
        m_status = Idle;
#if 0   // leaving triggering the retry to the tile fetcher
        m_provider->resolveProvider();
#endif
    }
}

void QGeoTileProviderOsm::addProvider(TileProvider *provider)
{
    if (!provider)
        return;
    QScopedPointer<TileProvider> p(provider);
    if (provider->status() == TileProvider::Invalid)
        return; // if the provider is already resolved and invalid, no point in adding it.

    provider = p.take();
    provider->setNetworkManager(m_nm);
    provider->setParent(this);
    m_providerList.append(provider);
    if (!m_provider)
        m_provider = provider;
}


/*
    Class TileProvder
*/

static void sort2(int &a, int &b)
{
    if (a > b) {
        int temp=a;
        a=b;
        b=temp;
    }
}

TileProvider::TileProvider() : m_status(Invalid), m_nm(nullptr), m_timestamp(defaultTs), m_highDpi(false)
{

}

TileProvider::TileProvider(const QUrl &urlRedirector, bool highDpi)
:   m_status(Idle), m_urlRedirector(urlRedirector), m_nm(nullptr), m_timestamp(defaultTs), m_highDpi(highDpi)
{
    if (!m_urlRedirector.isValid())
        m_status = Invalid;
}

TileProvider::TileProvider(const QString &urlTemplate,
                           const QString &format,
                           const QString &copyRightMap,
                           const QString &copyRightData,
                           bool highDpi,
                           int minimumZoomLevel,
                           int maximumZoomLevel)
:   m_status(Invalid), m_nm(nullptr), m_urlTemplate(urlTemplate),
    m_format(format), m_copyRightMap(copyRightMap), m_copyRightData(copyRightData),
    m_minimumZoomLevel(minimumZoomLevel), m_maximumZoomLevel(maximumZoomLevel), m_timestamp(defaultTs), m_highDpi(highDpi)
{
    setupProvider();
}

TileProvider::~TileProvider()
{
}

void TileProvider::resolveProvider()
{
    if (!m_nm)
        return;

    switch (m_status) {
    case Resolving:
    case Invalid:
    case Valid:
        return;
    case Idle:
        m_status = Resolving;
        break;
    }

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("QGeoTileFetcherOsm"));
    request.setUrl(m_urlRedirector);
    request.setAttribute(QNetworkRequest::BackgroundRequestAttribute, true);
    QNetworkReply *reply = m_nm->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onNetworkReplyFinished()) );
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onNetworkReplyError(QNetworkReply::NetworkError)));
}

void TileProvider::handleError(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::ContentGoneError:
    case QNetworkReply::OperationNotImplementedError:
    case QNetworkReply::ServiceUnavailableError:
        // Errors we don't expect to recover from in the near future, which
        // prevent accessing the redirection info but not the actual providers.
        m_status = Invalid;
    default:
        //qWarning() << "QGeoTileProviderOsm network error:" << error;
        break;
    }
}

void TileProvider::onNetworkReplyFinished()
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    reply->deleteLater();

    switch (m_status) {
    case Resolving:
        m_status = Idle;
    case Idle:    // should not happen
    case Invalid: // should not happen
        break;
    case Valid: // should not happen
        emit resolutionFinished(this);
        return;
    }

    QObject errorEmitter;
    QMetaObject::Connection errorEmitterConnection = connect(&errorEmitter, &QObject::destroyed, [this](){ this->resolutionError(this); });

    if (reply->error() != QNetworkReply::NoError) {
        handleError(reply->error());
        return;
    }
    m_status = Invalid;

    /*
     * The content of a provider information file must be in JSON format, containing
     * (as of Qt 5.6.2) the following fields:
     *
     * {
     *     "Enabled" : bool,  (optional)
     *     "UrlTemplate" : "<url template>", (mandatory)
     *     "ImageFormat" : "<image format>", (mandatory)
     *     "MapCopyRight" : "<copyright>", (mandatory)
     *     "DataCopyRight" : "<copyright>", (mandatory)
     *     "StyleCopyRight" : "<copyright>", (optional)
     *     "MinimumZoomLevel" : <minimumZoomLevel>, (optional)
     *     "MaximumZoomLevel" : <maximumZoomLevel>,  (optional)
     *     "Timestamp" : <timestamp>, (optional)
     * }
     *
     * Enabled is optional, and allows to temporarily disable a tile provider if it becomes
     * unavailable, without making the osm plugin fire requests to it. Default is true.
     *
     * MinimumZoomLevel and MaximumZoomLevel are also optional, and allow to prevent invalid tile
     * requests to the providers, if they do not support the specific ZL. Default is 0 and 20,
     * respectively.
     *
     * UrlTemplate is required, and is the tile url template, with %x, %y and %z as
     * placeholders for the actual parameters.
     * Example:
     * http://localhost:8080/maps/%z/%x/%y.png
     *
     * ImageFormat is required, and is the format of the tile.
     * Examples:
     * "png", "jpg"
     *
     * MapCopyRight is required and is the string that will be displayed in the "Map (c)" part
     * of the on-screen copyright notice. Can be an empty string.
     * Example:
     * "<a href='http://www.mapquest.com/'>MapQuest</a>"
     *
     * DataCopyRight is required and is the string that will be displayed in the "Data (c)" part
     * of the on-screen copyright notice. Can be an empty string.
     * Example:
     * "<a href='http://www.openstreetmap.org/copyright'>OpenStreetMap</a> contributors"
     *
     * StyleCopyRight is optional and is the string that will be displayed in the optional "Style (c)" part
     * of the on-screen copyright notice.
     *
     * Timestamp is optional, and if set will cause QtLocation to clear the content of the cache older
     * than this timestamp. The purpose is to prevent mixing tiles from different providers in the cache
     * upon provider change. The value must be a string in ISO 8601 format (see Qt::ISODate)
     */

    QJsonParseError error;
    QJsonDocument d = QJsonDocument::fromJson(reply->readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "QGeoTileProviderOsm: Error parsing redirection data: "<<error.errorString() << "at "<<m_urlRedirector;
        return;
    }
    if (!d.isObject()) {
        qWarning() << "QGeoTileProviderOsm: Invalid redirection data" << "at "<<m_urlRedirector;
        return;
    }
    const QJsonObject json = d.object();
    const QJsonValue urlTemplate = json.value(QLatin1String("UrlTemplate"));
    const QJsonValue imageFormat = json.value(QLatin1String("ImageFormat"));
    const QJsonValue copyRightMap = json.value(QLatin1String("MapCopyRight"));
    const QJsonValue copyRightData = json.value(QLatin1String("DataCopyRight"));
    if ( urlTemplate == QJsonValue::Undefined
         || imageFormat == QJsonValue::Undefined
         || copyRightMap  == QJsonValue::Undefined
         || copyRightData  == QJsonValue::Undefined
         || !urlTemplate.isString()
         || !imageFormat.isString()
         || !copyRightMap.isString()
         || !copyRightData.isString()) {
        qWarning() << "QGeoTileProviderOsm: Incomplete redirection data" << "at "<<m_urlRedirector;
        return;
    }

    m_urlTemplate = urlTemplate.toString();
    m_format = imageFormat.toString();
    m_copyRightMap = copyRightMap.toString();
    m_copyRightData = copyRightData.toString();

    const QJsonValue enabled = json.value(QLatin1String("Enabled"));
    if (enabled.isBool() && ! enabled.toBool()) {
        qWarning() << "QGeoTileProviderOsm: Tileserver disabled" << "at "<<m_urlRedirector;
        return;
    }

    const QJsonValue copyRightStyle = json.value(QLatin1String("StyleCopyRight"));
    if (copyRightStyle != QJsonValue::Undefined && copyRightStyle.isString())
        m_copyRightStyle = copyRightStyle.toString();

    m_minimumZoomLevel = 0;
    m_maximumZoomLevel = 20;
    const QJsonValue minZoom = json.value(QLatin1String("MinimumZoomLevel"));
    if (minZoom.isDouble())
        m_minimumZoomLevel = qBound(0, int(minZoom.toDouble()), maxValidZoom);
    const QJsonValue maxZoom = json.value(QLatin1String("MaximumZoomLevel"));
    if (maxZoom.isDouble())
        m_maximumZoomLevel = qBound(0, int(maxZoom.toDouble()), maxValidZoom);

    const QJsonValue ts = json.value(QLatin1String("Timestamp"));
    if (ts.isString())
        m_timestamp = QDateTime::fromString(ts.toString(), Qt::ISODate);

    setupProvider();
    if (isValid()) {
        QObject::disconnect(errorEmitterConnection);
        emit resolutionFinished(this);
    }
}

void TileProvider::onNetworkReplyError(QNetworkReply::NetworkError error)
{
    if (m_status == Resolving)
        m_status = Idle;

    handleError(error);
    static_cast<QNetworkReply *>(sender())->deleteLater();
    emit resolutionError(this);
}

void TileProvider::setupProvider()
{
    if (m_urlTemplate.isEmpty())
        return;

    if (m_format.isEmpty())
        return;

    if (m_minimumZoomLevel < 0 || m_minimumZoomLevel > 30)
        return;

    if (m_maximumZoomLevel < 0 || m_maximumZoomLevel > 30 ||  m_maximumZoomLevel < m_minimumZoomLevel)
        return;

    // Currently supporting only %x, %y and &z
    int offset[3];
    offset[0] = m_urlTemplate.indexOf(QLatin1String("%x"));
    if (offset[0] < 0)
        return;

    offset[1] = m_urlTemplate.indexOf(QLatin1String("%y"));
    if (offset[1] < 0)
        return;

    offset[2] = m_urlTemplate.indexOf(QLatin1String("%z"));
    if (offset[2] < 0)
        return;

    int sortedOffsets[3];
    std::copy(offset, offset + 3, sortedOffsets);
    sort2(sortedOffsets[0] ,sortedOffsets[1]);
    sort2(sortedOffsets[1] ,sortedOffsets[2]);
    sort2(sortedOffsets[0] ,sortedOffsets[1]);

    int min = sortedOffsets[0];
    int max = sortedOffsets[2];
    int mid = sortedOffsets[1];

    // Initing LUT
    for (int i=0; i<3; i++) {
        if (offset[0] == sortedOffsets[i])
            paramsLUT[i] = 0;
        else if (offset[1] == sortedOffsets[i])
            paramsLUT[i] = 1;
        else
            paramsLUT[i] = 2;
    }

    m_urlPrefix = m_urlTemplate.mid(0 , min);
    m_urlSuffix = m_urlTemplate.mid(max + 2, m_urlTemplate.size() - max - 2);

    paramsSep[0] = m_urlTemplate.mid(min + 2, mid - min - 2);
    paramsSep[1] = m_urlTemplate.mid(mid + 2, max - mid - 2);
    m_status = Valid;
}

bool TileProvider::isValid() const
{
    return m_status == Valid;
}

bool TileProvider::isInvalid() const
{
    return m_status == Invalid;
}

bool TileProvider::isResolved() const
{
    return (m_status == Valid || m_status == Invalid);
}

QString TileProvider::mapCopyRight() const
{
    return m_copyRightMap;
}

QString TileProvider::dataCopyRight() const
{
    return m_copyRightData;
}

QString TileProvider::styleCopyRight() const
{
    return m_copyRightStyle;
}

QString TileProvider::format() const
{
    return m_format;
}

int TileProvider::minimumZoomLevel() const
{
    return m_minimumZoomLevel;
}

int TileProvider::maximumZoomLevel() const
{
    return m_maximumZoomLevel;
}

const QDateTime &TileProvider::timestamp() const
{
    return m_timestamp;
}

bool TileProvider::isHighDpi() const
{
    return m_highDpi;
}

void TileProvider::setStyleCopyRight(const QString &copyright)
{
    m_copyRightStyle = copyright;
}

void TileProvider::setTimestamp(const QDateTime &timestamp)
{
    m_timestamp = timestamp;
}

QUrl TileProvider::tileAddress(int x, int y, int z) const
{
    if (z < m_minimumZoomLevel || z > m_maximumZoomLevel)
        return QUrl();
    int params[3] = { x, y, z};
    QString url;
    url += m_urlPrefix;
    url += QString::number(params[paramsLUT[0]]);
    url += paramsSep[0];
    url += QString::number(params[paramsLUT[1]]);
    url += paramsSep[1];
    url += QString::number(params[paramsLUT[2]]);
    url += m_urlSuffix;
    return QUrl(url);
}

void TileProvider::setNetworkManager(QNetworkAccessManager *nm)
{
    m_nm = nm;
}

TileProvider::Status TileProvider::status() const
{
    return m_status;
}


QT_END_NAMESPACE
