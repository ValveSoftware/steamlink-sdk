/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2017 Mapbox, Inc.
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

#ifndef QGEOMAPMAPBOXGL_P_H
#define QGEOMAPMAPBOXGL_P_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtLocation/private/qgeomap_p_p.h>
#include <QtLocation/private/qgeomapparameter_p.h>

class QMapboxGL;
class QMapboxGLStyleChange;

class QGeoMapMapboxGLPrivate : public QGeoMapPrivate
{
    Q_DECLARE_PUBLIC(QGeoMapMapboxGL)

public:
    QGeoMapMapboxGLPrivate(QGeoMappingManagerEngineMapboxGL *engine);

    ~QGeoMapMapboxGLPrivate();

    QSGNode *updateSceneGraph(QSGNode *oldNode, QQuickWindow *window);

    void addParameter(QGeoMapParameter *param) Q_DECL_OVERRIDE;
    void removeParameter(QGeoMapParameter *param) Q_DECL_OVERRIDE;

    QGeoMap::ItemTypes supportedMapItemTypes() const Q_DECL_OVERRIDE;
    void addMapItem(QDeclarativeGeoMapItemBase *item) Q_DECL_OVERRIDE;
    void removeMapItem(QDeclarativeGeoMapItemBase *item) Q_DECL_OVERRIDE;

    /* Data members */
    enum SyncState : int {
        NoSync = 0,
        ViewportSync   = 1 << 0,
        CameraDataSync = 1 << 1,
        MapTypeSync    = 1 << 2
    };
    Q_DECLARE_FLAGS(SyncStates, SyncState);

    QMapboxGLSettings m_settings;
    bool m_useFBO = true;
    bool m_developmentMode = false;
    QString m_mapItemsBefore;

    QTimer m_refresh;
    bool m_shouldRefresh = true;
    bool m_warned = false;
    bool m_threadedRendering = false;
    bool m_styleLoaded = false;

    SyncStates m_syncState = NoSync;

    QList<QSharedPointer<QMapboxGLStyleChange>> m_styleChanges;

protected:
    void changeViewportSize(const QSize &size) Q_DECL_OVERRIDE;
    void changeCameraData(const QGeoCameraData &oldCameraData) Q_DECL_OVERRIDE;
    void changeActiveMapType(const QGeoMapType mapType) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(QGeoMapMapboxGLPrivate);

    void syncStyleChanges(QMapboxGL *map);
    void threadedRenderingHack(QQuickWindow *window, QMapboxGL *map);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGeoMapMapboxGLPrivate::SyncStates)

#endif // QGEOMAPMAPBOXGL_P_H
