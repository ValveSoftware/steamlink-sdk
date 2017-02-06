/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qraycastingservice_p.h"

#include <Qt3DRender/private/qray3d_p.h>
#include <Qt3DRender/private/sphere_p.h>
#include <Qt3DRender/private/qboundingvolumeprovider_p.h>

#include <QMap>
#include <QtConcurrent>

#include "math.h"

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

namespace {

struct Hit
{
    Hit()
        : intersects(false)
        , distance(-1.0f)
    {}

    bool intersects;
    float distance;
    Qt3DCore::QNodeId id;
    QVector3D intersection;
};

bool compareHitsDistance(const Hit &a, const Hit &b)
{
    return a.distance < b.distance;
}

Hit volumeRayIntersection(const QBoundingVolume *volume, const QRay3D &ray)
{
    Hit hit;
    if ((hit.intersects = volume->intersects(ray, &hit.intersection))) {
        hit.distance = ray.projectedDistance(hit.intersection);
        hit.id = volume->id();
    }
    return hit;
}

Hit reduceToFirstHit(Hit &result, const Hit &intermediate)
{
    if (intermediate.intersects) {
        if (result.distance == -1.0f ||
                (intermediate.distance >= 0.0f &&
                 intermediate.distance < result.distance))
            result = intermediate;
    }
    return result;
}

// Unordered
QVector<Hit> reduceToAllHits(QVector<Hit> &results, const Hit &intermediate)
{
    if (intermediate.intersects)
        results.push_back(intermediate);
    return results;
}

struct CollisionGathererFunctor
{
    QRay3D ray;

    typedef Hit result_type;

    Hit operator ()(const QBoundingVolume *volume) const
    {
        return volumeRayIntersection(volume, ray);
    }
};

} // anonymous


QCollisionQueryResult QRayCastingServicePrivate::collides(const QRay3D &ray, QBoundingVolumeProvider *provider,
                                                          QAbstractCollisionQueryService::QueryMode mode, const QQueryHandle &handle)
{
    Q_Q(QRayCastingService);

    const QVector<QBoundingVolume *> volumes(provider->boundingVolumes());
    QCollisionQueryResult result;
    q->setResultHandle(result, handle);

    CollisionGathererFunctor gathererFunctor;
    gathererFunctor.ray = ray;

    if (mode == QAbstractCollisionQueryService::FirstHit) {
        Hit firstHit = QtConcurrent::blockingMappedReduced<Hit>(volumes, gathererFunctor, reduceToFirstHit);
        if (firstHit.intersects)
            q->addEntityHit(result, firstHit.id, firstHit.intersection, firstHit.distance);
    } else {
        QVector<Hit> hits = QtConcurrent::blockingMappedReduced<QVector<Hit> >(volumes, gathererFunctor, reduceToAllHits);
        std::sort(hits.begin(), hits.end(), compareHitsDistance);
        for (const Hit &hit : qAsConst(hits))
            q->addEntityHit(result, hit.id, hit.intersection, hit.distance);
    }

    return result;
}

QCollisionQueryResult::Hit QRayCastingServicePrivate::collides(const QRay3D &ray, const Qt3DRender::QBoundingVolume *volume)
{
    QCollisionQueryResult::Hit result;
    Hit hit = volumeRayIntersection(volume, ray);
    if (hit.intersects)
    {
        result.m_distance = hit.distance;
        result.m_entityId = hit.id;
        result.m_intersection = hit.intersection;
    }
    return result;
}

QRayCastingServicePrivate::QRayCastingServicePrivate(const QString &description)
    : QAbstractCollisionQueryServicePrivate(description)
    , m_handlesCount(0)
{
}

QRayCastingService::QRayCastingService()
    : QAbstractCollisionQueryService(*new QRayCastingServicePrivate(QStringLiteral("Collision detection service using Ray Casting")))
{
}

QQueryHandle QRayCastingService::query(const QRay3D &ray,
                                       QAbstractCollisionQueryService::QueryMode mode,
                                       QBoundingVolumeProvider *provider)
{
    Q_D(QRayCastingService);

    QQueryHandle handle = d->m_handlesCount.fetchAndStoreOrdered(1);


    // Blocking mapReduce

    FutureQueryResult future = QtConcurrent::run(d, &QRayCastingServicePrivate::collides,
                                                 ray, provider, mode, handle);
    d->m_results.insert(handle, future);

    return handle;
}

QCollisionQueryResult::Hit QRayCastingService::query(const QRay3D &ray, const QBoundingVolume *volume)
{
    Q_D(QRayCastingService);

    return d->collides(ray, volume);
}

QCollisionQueryResult QRayCastingService::fetchResult(const QQueryHandle &handle)
{
    Q_D(QRayCastingService);

    return d->m_results.value(handle).result();
}

QVector<QCollisionQueryResult> QRayCastingService::fetchAllResults() const
{
    Q_D(const QRayCastingService);

    QVector<QCollisionQueryResult> results;
    results.reserve(d->m_results.size());

    for (const FutureQueryResult &future : d->m_results)
        results.append(future.result());

    return results;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
