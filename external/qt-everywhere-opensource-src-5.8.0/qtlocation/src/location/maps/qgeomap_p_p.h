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
#ifndef QGEOMAP_P_P_H
#define QGEOMAP_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtLocation/private/qlocationglobal_p.h>
#include <QtLocation/private/qgeocameradata_p.h>
#include <QtLocation/private/qgeomaptype_p.h>
#include <QtCore/private/qobject_p.h>
#include <QtCore/QSize>


QT_BEGIN_NAMESPACE

class QGeoMappingManagerEngine;
class QGeoMap;
class QGeoMapController;

class Q_LOCATION_PRIVATE_EXPORT QGeoMapPrivate :  public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGeoMap)
public:
    QGeoMapPrivate(QGeoMappingManagerEngine *engine);
    virtual ~QGeoMapPrivate();

protected:
    /* Hooks into the actual map implementations */
    virtual void changeViewportSize(const QSize &size) = 0; // called by QGeoMap::setSize()
    virtual void changeCameraData(const QGeoCameraData &oldCameraData) = 0; // called by QGeoMap::setCameraData()
    virtual void changeActiveMapType(const QGeoMapType mapType) = 0; // called by QGeoMap::setActiveMapType()

protected:
    QSize m_viewportSize;
    QPointer<QGeoMappingManagerEngine> m_engine;
    QGeoMapController *m_controller;
    QGeoCameraData m_cameraData;
    QGeoMapType m_activeMapType;
};

QT_END_NAMESPACE

#endif // QGEOMAP_P_P_H
