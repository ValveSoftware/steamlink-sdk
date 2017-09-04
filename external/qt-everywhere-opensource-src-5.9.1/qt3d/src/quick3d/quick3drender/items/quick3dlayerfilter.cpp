/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include <Qt3DRender/qtexture.h>

#include <Qt3DQuickRender/private/quick3dlayerfilter_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace Quick {

Quick3DLayerFilter::Quick3DLayerFilter(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QLayer> Quick3DLayerFilter::qmlLayers()
{
    return QQmlListProperty<QLayer>(this, 0,
                                    &Quick3DLayerFilter::appendLayer,
                                    &Quick3DLayerFilter::layerCount,
                                    &Quick3DLayerFilter::layerAt,
                                    &Quick3DLayerFilter::clearLayers);
}

void Quick3DLayerFilter::appendLayer(QQmlListProperty<QLayer> *list, QLayer *layer)
{
    Quick3DLayerFilter *filter = qobject_cast<Quick3DLayerFilter *>(list->object);
    if (filter) {
        filter->parentFilter()->addLayer(layer);
    }
}

QLayer *Quick3DLayerFilter::layerAt(QQmlListProperty<QLayer> *list, int index)
{
    Quick3DLayerFilter *filter = qobject_cast<Quick3DLayerFilter *>(list->object);
    if (filter) {
        return filter->parentFilter()->layers().at(index);
    }
    return 0;
}

int Quick3DLayerFilter::layerCount(QQmlListProperty<QLayer> *list)
{
    Quick3DLayerFilter *filter = qobject_cast<Quick3DLayerFilter *>(list->object);
    if (filter) {
        return filter->parentFilter()->layers().count();
    }
    return 0;
}

void Quick3DLayerFilter::clearLayers(QQmlListProperty<QLayer> *list)
{
    Quick3DLayerFilter *filter = qobject_cast<Quick3DLayerFilter *>(list->object);
    if (filter) {
        const auto layers = filter->parentFilter()->layers();
        for (QLayer *layer : layers)
            filter->parentFilter()->removeLayer(layer);
    }
}

} // Quick

} // namespace Render

} // Qt3D

QT_END_NAMESPACE
