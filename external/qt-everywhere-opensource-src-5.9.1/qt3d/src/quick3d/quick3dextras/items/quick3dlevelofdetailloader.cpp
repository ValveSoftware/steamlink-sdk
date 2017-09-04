/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "quick3dlevelofdetailloader_p_p.h"
#include <Qt3DRender/qlevelofdetailboundingsphere.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DQuick/private/quick3dentityloader_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {
namespace Extras {
namespace Quick {

Quick3DLevelOfDetailLoaderPrivate::Quick3DLevelOfDetailLoaderPrivate()
    : QEntityPrivate()
    , m_loader(new Qt3DCore::Quick::Quick3DEntityLoader)
    , m_lod(new Qt3DRender::QLevelOfDetail)
{
}

Quick3DLevelOfDetailLoader::Quick3DLevelOfDetailLoader(QNode *parent)
    : QEntity(*new Quick3DLevelOfDetailLoaderPrivate, parent)
{
    Q_D(Quick3DLevelOfDetailLoader);
    d->m_loader->setParent(this);
    d->m_loader->addComponent(d->m_lod);

    connect(d->m_lod, &Qt3DRender::QLevelOfDetail::cameraChanged,
            this, &Quick3DLevelOfDetailLoader::cameraChanged);
    connect(d->m_lod, &Qt3DRender::QLevelOfDetail::currentIndexChanged,
            this, &Quick3DLevelOfDetailLoader::currentIndexChanged);
    connect(d->m_lod, &Qt3DRender::QLevelOfDetail::thresholdTypeChanged,
            this, &Quick3DLevelOfDetailLoader::thresholdTypeChanged);
    connect(d->m_lod, &Qt3DRender::QLevelOfDetail::thresholdsChanged,
            this, &Quick3DLevelOfDetailLoader::thresholdsChanged);
    connect(d->m_lod, &Qt3DRender::QLevelOfDetail::volumeOverrideChanged,
            this, &Quick3DLevelOfDetailLoader::volumeOverrideChanged);
    connect(d->m_loader, &Qt3DCore::Quick::Quick3DEntityLoader::entityChanged,
            this, &Quick3DLevelOfDetailLoader::entityChanged);
    connect(d->m_loader, &Qt3DCore::Quick::Quick3DEntityLoader::sourceChanged,
            this, &Quick3DLevelOfDetailLoader::sourceChanged);

    connect(this, &Quick3DLevelOfDetailLoader::enabledChanged,
            d->m_lod, &Qt3DRender::QLevelOfDetail::setEnabled);

    auto applyCurrentSource = [this] {
        Q_D(Quick3DLevelOfDetailLoader);
        const auto index = currentIndex();
        if (index >= 0 && index < d->m_sources.size())
            d->m_loader->setSource(d->m_sources.at(index).toUrl());
    };

    connect(this, &Quick3DLevelOfDetailLoader::sourcesChanged,
            this, applyCurrentSource);
    connect(this, &Quick3DLevelOfDetailLoader::currentIndexChanged,
            this, applyCurrentSource);
}

QVariantList Quick3DLevelOfDetailLoader::sources() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_sources;
}

void Quick3DLevelOfDetailLoader::setSources(const QVariantList &sources)
{
    Q_D(Quick3DLevelOfDetailLoader);
    if (d->m_sources != sources) {
        d->m_sources = sources;
        emit sourcesChanged();
    }
}

Qt3DRender::QCamera *Quick3DLevelOfDetailLoader::camera() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_lod->camera();
}

void Quick3DLevelOfDetailLoader::setCamera(Qt3DRender::QCamera *camera)
{
    Q_D(Quick3DLevelOfDetailLoader);
    d->m_lod->setCamera(camera);
}

int Quick3DLevelOfDetailLoader::currentIndex() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_lod->currentIndex();
}

void Quick3DLevelOfDetailLoader::setCurrentIndex(int currentIndex)
{
    Q_D(Quick3DLevelOfDetailLoader);
    d->m_lod->setCurrentIndex(currentIndex);
}

Qt3DRender::QLevelOfDetail::ThresholdType Quick3DLevelOfDetailLoader::thresholdType() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_lod->thresholdType();
}

void Quick3DLevelOfDetailLoader::setThresholdType(Qt3DRender::QLevelOfDetail::ThresholdType thresholdType)
{
    Q_D(Quick3DLevelOfDetailLoader);
    d->m_lod->setThresholdType(thresholdType);
}

QVector<qreal> Quick3DLevelOfDetailLoader::thresholds() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_lod->thresholds();
}

void Quick3DLevelOfDetailLoader::setThresholds(const QVector<qreal> &thresholds)
{
    Q_D(Quick3DLevelOfDetailLoader);
    d->m_lod->setThresholds(thresholds);
}

Qt3DRender::QLevelOfDetailBoundingSphere Quick3DLevelOfDetailLoader::createBoundingSphere(const QVector3D &center, float radius)
{
    return Qt3DRender::QLevelOfDetailBoundingSphere(center, radius);
}

Qt3DRender::QLevelOfDetailBoundingSphere Quick3DLevelOfDetailLoader::volumeOverride() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_lod->volumeOverride();
}

void Quick3DLevelOfDetailLoader::setVolumeOverride(const Qt3DRender::QLevelOfDetailBoundingSphere &volumeOverride)
{
    Q_D(Quick3DLevelOfDetailLoader);
    d->m_lod->setVolumeOverride(volumeOverride);
}

QObject *Quick3DLevelOfDetailLoader::entity() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_loader->entity();
}

QUrl Quick3DLevelOfDetailLoader::source() const
{
    Q_D(const Quick3DLevelOfDetailLoader);
    return d->m_loader->source();
}

} // namespace Quick
} // namespace Extras
} // namespace Qt3DExtras

QT_END_NAMESPACE


