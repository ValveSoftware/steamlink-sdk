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

#ifndef QGEOMAPMAPBOXGL_H
#define QGEOMAPMAPBOXGL_H

#include "qgeomappingmanagerenginemapboxgl.h"
#include <QtLocation/private/qgeomap_p.h>
#include <QtLocation/private/qgeomapparameter_p.h>

class QGeoMapMapboxGLPrivate;

class QGeoMapMapboxGL : public QGeoMap
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGeoMapMapboxGL)

public:
    QGeoMapMapboxGL(QGeoMappingManagerEngineMapboxGL *engine, QObject *parent);
    virtual ~QGeoMapMapboxGL();

    QString copyrightsStyleSheet() const Q_DECL_OVERRIDE;
    void setMapboxGLSettings(const QMapboxGLSettings &);
    void setUseFBO(bool);
    void setMapItemsBefore(const QString &);

private Q_SLOTS:
    // QMapboxGL
    void onMapChanged(QMapboxGL::MapChange);

    // QDeclarativeGeoMapItemBase
    void onMapItemPropertyChanged();
    void onMapItemSubPropertyChanged();
    void onMapItemUnsupportedPropertyChanged();
    void onMapItemGeometryChanged();

    // QGeoMapParameter
    void onParameterPropertyUpdated(QGeoMapParameter *param, const char *propertyName);

public Q_SLOTS:
    void copyrightsChanged(const QString &copyrightsHtml);

private:
    QSGNode *updateSceneGraph(QSGNode *oldNode, QQuickWindow *window) Q_DECL_OVERRIDE;

    QGeoMappingManagerEngineMapboxGL *m_engine;
};

#endif // QGEOMAPMAPBOXGL_H
