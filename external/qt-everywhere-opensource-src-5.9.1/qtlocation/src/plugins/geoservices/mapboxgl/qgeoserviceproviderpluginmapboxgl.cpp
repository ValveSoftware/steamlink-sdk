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

#include "qgeoserviceproviderpluginmapboxgl.h"
#include "qgeomappingmanagerenginemapboxgl.h"

#include <QtGui/QOpenGLContext>

static void initResources()
{
    Q_INIT_RESOURCE(mapboxgl);
}

QT_BEGIN_NAMESPACE

QGeoServiceProviderFactoryMapboxGL::QGeoServiceProviderFactoryMapboxGL()
{
    initResources();
}

QGeoCodingManagerEngine *QGeoServiceProviderFactoryMapboxGL::createGeocodingManagerEngine(
    const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString) const
{
    Q_UNUSED(parameters)
    Q_UNUSED(error)
    Q_UNUSED(errorString)

    return 0;
}

QGeoMappingManagerEngine *QGeoServiceProviderFactoryMapboxGL::createMappingManagerEngine(
    const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString) const
{
#ifdef Q_OS_WIN
    if (QOpenGLContext::openGLModuleType() != QOpenGLContext::LibGLES) {
        qWarning("The Mapbox GL plugin only supports OpenGL ES on Windows. \
            Try setting the environment variable QT_OPENGL to 'angle'.");

        return 0;
    }
#endif

    return new QGeoMappingManagerEngineMapboxGL(parameters, error, errorString);
}

QGeoRoutingManagerEngine *QGeoServiceProviderFactoryMapboxGL::createRoutingManagerEngine(
    const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString) const
{
    Q_UNUSED(parameters)
    Q_UNUSED(error)
    Q_UNUSED(errorString)

    return 0;
}

QPlaceManagerEngine *QGeoServiceProviderFactoryMapboxGL::createPlaceManagerEngine(
    const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString) const
{
    Q_UNUSED(parameters)
    Q_UNUSED(error)
    Q_UNUSED(errorString)

    return 0;
}

QT_END_NAMESPACE
