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

#include <QtCore/QTimerEvent>

#include "qgeomappingmanagerengine_p.h"
#include "qgeotilefetcher_p.h"
#include "qgeotilefetcher_p_p.h"
#include "qgeotiledmapreply_p.h"
#include "qgeotilespec_p.h"
#include "qgeotiledmap_p.h"

QT_BEGIN_NAMESPACE

QGeoTileFetcher::QGeoTileFetcher(QObject *parent)
:   QObject(*new QGeoTileFetcherPrivate(), parent)
{
    Q_D(QGeoTileFetcher);

    d->enabled_ = true;

//    if (!d->queue_.isEmpty())
//        d->timer_.start(0, this);
}

QGeoTileFetcher::QGeoTileFetcher(QGeoTileFetcherPrivate &dd, QObject *parent)
:   QObject(dd,parent)
{
    Q_D(QGeoTileFetcher);
    d->enabled_ = true;

//    if (!d->queue_.isEmpty())
//        d->timer_.start(0, this);
}

QGeoTileFetcher::~QGeoTileFetcher()
{
}

void QGeoTileFetcher::updateTileRequests(const QSet<QGeoTileSpec> &tilesAdded,
                                                  const QSet<QGeoTileSpec> &tilesRemoved)
{
    Q_D(QGeoTileFetcher);

    QMutexLocker ml(&d->queueMutex_);

    cancelTileRequests(tilesRemoved);

    d->queue_ += tilesAdded.toList();

    if (d->enabled_ && initialized() && !d->queue_.isEmpty() && !d->timer_.isActive())
        d->timer_.start(0, this);
}

void QGeoTileFetcher::cancelTileRequests(const QSet<QGeoTileSpec> &tiles)
{
    Q_D(QGeoTileFetcher);

    typedef QSet<QGeoTileSpec>::const_iterator tile_iter;
    tile_iter tile = tiles.constBegin();
    tile_iter end = tiles.constEnd();
    for (; tile != end; ++tile) {
        QGeoTiledMapReply *reply = d->invmap_.value(*tile, 0);
        if (reply) {
            d->invmap_.remove(*tile);
            reply->abort();
            if (reply->isFinished())
                reply->deleteLater();
        }
        d->queue_.removeAll(*tile);
    }
}

void QGeoTileFetcher::requestNextTile()
{
    Q_D(QGeoTileFetcher);

    QMutexLocker ml(&d->queueMutex_);

    if (!d->enabled_)
        return;

    if (d->queue_.isEmpty())
        return;

    QGeoTileSpec ts = d->queue_.takeFirst();
    if (d->queue_.isEmpty())
        d->timer_.stop();

    QGeoTiledMapReply *reply = getTileImage(ts);
    if (!reply)
        return;

    if (reply->isFinished()) {
        handleReply(reply, ts);
    } else {
        connect(reply,
                SIGNAL(finished()),
                this,
                SLOT(finished()),
                Qt::QueuedConnection);

        d->invmap_.insert(ts, reply);
    }
}

void QGeoTileFetcher::finished()
{
    Q_D(QGeoTileFetcher);

    QMutexLocker ml(&d->queueMutex_);

    QGeoTiledMapReply *reply = qobject_cast<QGeoTiledMapReply *>(sender());
    if (!reply)
        return;

    QGeoTileSpec spec = reply->tileSpec();

    if (!d->invmap_.contains(spec)) {
        reply->deleteLater();
        return;
    }

    d->invmap_.remove(spec);

    handleReply(reply, spec);
}

void QGeoTileFetcher::timerEvent(QTimerEvent *event)
{
    Q_D(QGeoTileFetcher);
    if (event->timerId() != d->timer_.timerId()) {
        QObject::timerEvent(event);
        return;
    }

    if (d->queue_.isEmpty() || !initialized()) {
        d->timer_.stop();
        return;
    }

    requestNextTile();
}

bool QGeoTileFetcher::initialized() const
{
    return true;
}

void QGeoTileFetcher::handleReply(QGeoTiledMapReply *reply, const QGeoTileSpec &spec)
{
    Q_D(QGeoTileFetcher);

    if (!d->enabled_) {
        reply->deleteLater();
        return;
    }

    if (reply->error() == QGeoTiledMapReply::NoError) {
        emit tileFinished(spec, reply->mapImageData(), reply->mapImageFormat());
    } else {
        emit tileError(spec, reply->errorString());
    }

    reply->deleteLater();
}

/*******************************************************************************
*******************************************************************************/

QGeoTileFetcherPrivate::QGeoTileFetcherPrivate()
:   QObjectPrivate(), enabled_(false)
{
}

QGeoTileFetcherPrivate::~QGeoTileFetcherPrivate()
{
}

QT_END_NAMESPACE
