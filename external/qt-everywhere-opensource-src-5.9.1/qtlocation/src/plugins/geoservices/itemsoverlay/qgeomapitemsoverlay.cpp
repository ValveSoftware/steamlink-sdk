/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qgeomapitemsoverlay.h"
#include "qgeomappingmanagerengineitemsoverlay.h"
#include <QtLocation/private/qgeomap_p_p.h>
#include <QtQuick/qsgnode.h>

QT_BEGIN_NAMESPACE

class QGeoMapItemsOverlayPrivate : public QGeoMapPrivate
{
    Q_DECLARE_PUBLIC(QGeoMap)
public:
    QGeoMapItemsOverlayPrivate(QGeoMappingManagerEngineItemsOverlay *engine);

    virtual ~QGeoMapItemsOverlayPrivate();

protected:
    void changeViewportSize(const QSize &size) Q_DECL_OVERRIDE;
    void changeCameraData(const QGeoCameraData &oldCameraData) Q_DECL_OVERRIDE;
    void changeActiveMapType(const QGeoMapType mapType) Q_DECL_OVERRIDE;
};

QGeoMapItemsOverlay::QGeoMapItemsOverlay(QGeoMappingManagerEngineItemsOverlay *engine, QObject *parent)
    : QGeoMap(*(new QGeoMapItemsOverlayPrivate(engine)), parent)
{

}

QGeoMapItemsOverlay::~QGeoMapItemsOverlay()
{
}

QSGNode *QGeoMapItemsOverlay::updateSceneGraph(QSGNode *node, QQuickWindow *window)
{
    Q_UNUSED(window)
    return node;
}

QGeoMapItemsOverlayPrivate::QGeoMapItemsOverlayPrivate(QGeoMappingManagerEngineItemsOverlay *engine)
    : QGeoMapPrivate(engine, new QGeoProjectionWebMercator)
{
}

QGeoMapItemsOverlayPrivate::~QGeoMapItemsOverlayPrivate()
{
}

void QGeoMapItemsOverlayPrivate::changeViewportSize(const QSize &size)
{
    Q_UNUSED(size)
}

void QGeoMapItemsOverlayPrivate::changeCameraData(const QGeoCameraData &oldCameraData)
{
    Q_UNUSED(oldCameraData)
}

void QGeoMapItemsOverlayPrivate::changeActiveMapType(const QGeoMapType mapType)
{
    Q_UNUSED(mapType)
}

QT_END_NAMESPACE




