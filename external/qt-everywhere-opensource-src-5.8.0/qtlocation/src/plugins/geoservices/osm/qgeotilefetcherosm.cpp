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

#include "qgeotilefetcherosm.h"
#include "qgeomapreplyosm.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtLocation/private/qgeotilespec_p.h>
#include <QtLocation/private/qgeotilefetcher_p_p.h>


QT_BEGIN_NAMESPACE

static bool providersResolved(const QVector<QGeoTileProviderOsm *> &providers)
{
    foreach (const QGeoTileProviderOsm *provider, providers)
        if (!provider->isResolved())
            return false;
    return true;
}

class QGeoTileFetcherOsmPrivate : public QGeoTileFetcherPrivate
{
    Q_DECLARE_PUBLIC(QGeoTileFetcherOsm)
public:
    QGeoTileFetcherOsmPrivate();
    virtual ~QGeoTileFetcherOsmPrivate();

private:
    Q_DISABLE_COPY(QGeoTileFetcherOsmPrivate)
};

QGeoTileFetcherOsmPrivate::QGeoTileFetcherOsmPrivate() : QGeoTileFetcherPrivate()
{
}

QGeoTileFetcherOsmPrivate::~QGeoTileFetcherOsmPrivate()
{
}


QGeoTileFetcherOsm::QGeoTileFetcherOsm(const QVector<QGeoTileProviderOsm *> &providers,
                                       QNetworkAccessManager *nm,
                                       QObject *parent)
:   QGeoTileFetcher(*new QGeoTileFetcherOsmPrivate(), parent), m_userAgent("Qt Location based application"),
    m_providers(providers), m_nm(nm), m_ready(true)
{
    m_nm->setParent(this);
    foreach (QGeoTileProviderOsm *provider, m_providers) {
        if (!provider->isResolved()) {
            m_ready = false;
            connect(provider, &QGeoTileProviderOsm::resolutionFinished,
                    this, &QGeoTileFetcherOsm::onProviderResolutionFinished);
            connect(provider, &QGeoTileProviderOsm::resolutionError,
                    this, &QGeoTileFetcherOsm::onProviderResolutionError);
            connect(provider, &QGeoTileProviderOsm::resolutionRequired,
                    this, &QGeoTileFetcherOsm::restartTimer, Qt::QueuedConnection);
            provider->resolveProvider();
        }
    }
    if (m_ready)
        readyUpdated();
}

void QGeoTileFetcherOsm::setUserAgent(const QByteArray &userAgent)
{
    m_userAgent = userAgent;
}

bool QGeoTileFetcherOsm::initialized() const
{
    if (!m_ready) {
        foreach (QGeoTileProviderOsm *provider, m_providers)
            if (!provider->isResolved())
                provider->resolveProvider();
    }
    return m_ready;
}

void QGeoTileFetcherOsm::onProviderResolutionFinished(const QGeoTileProviderOsm *provider)
{
    if ((m_ready = providersResolved(m_providers))) {
        qWarning("QGeoTileFetcherOsm: all providers resolved");
        readyUpdated();
    }
    emit providerDataUpdated(provider);
}

void QGeoTileFetcherOsm::onProviderResolutionError(const QGeoTileProviderOsm *provider)
{
    if ((m_ready = providersResolved(m_providers))) {
        qWarning("QGeoTileFetcherOsm: all providers resolved");
        readyUpdated();
    }
    emit providerDataUpdated(provider);
}

void QGeoTileFetcherOsm::restartTimer()
{
    Q_D(QGeoTileFetcherOsm);

    if (!d->queue_.isEmpty())
        d->timer_.start(0, this);
}

QGeoTiledMapReply *QGeoTileFetcherOsm::getTileImage(const QGeoTileSpec &spec)
{
    int id = spec.mapId();
    if (id < 1 || id > m_providers.size()) {
        qWarning("Unknown map id %d\n", spec.mapId());
        if (m_providers.isEmpty())
            return Q_NULLPTR;
        else
            id = 1;
    }
    id -= 1; // TODO: make OSM map ids start from 0.

    if (spec.zoom() > m_providers[id]->maximumZoomLevel() || spec.zoom() < m_providers[id]->minimumZoomLevel())
        return Q_NULLPTR;

    const QUrl url = m_providers[id]->tileAddress(spec.x(), spec.y(), spec.zoom());
    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    request.setUrl(url);
    QNetworkReply *reply = m_nm->get(request);
    return new QGeoMapReplyOsm(reply, spec, m_providers[id]->format());
}

void QGeoTileFetcherOsm::readyUpdated()
{
    updateTileRequests(QSet<QGeoTileSpec>(), QSet<QGeoTileSpec>());
}

QT_END_NAMESPACE
