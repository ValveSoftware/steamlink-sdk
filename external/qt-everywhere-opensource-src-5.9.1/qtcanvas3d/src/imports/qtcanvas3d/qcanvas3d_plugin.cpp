/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "qcanvas3d_plugin.h"

#include <QtQml/qqml.h>

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtCanvas3D);
#endif
}

QT_BEGIN_NAMESPACE

QT_CANVAS3D_BEGIN_NAMESPACE

QtCanvas3DPlugin::QtCanvas3DPlugin(QObject *parent) : QQmlExtensionPlugin(parent)
{
    initResources();
}

void QtCanvas3DPlugin::registerTypes(const char *uri)
{
    // @uri com.digia.qtcanvas3d

    // QtCanvas3D 1.0

    // QTCANVAS3D CORE API
    qmlRegisterSingletonType<CanvasTextureImageFactory>(uri,
                                                        1, 0,
                                                        "TextureImageFactory",
                                                        CanvasTextureImageFactory::texture_image_factory_provider);

    qmlRegisterUncreatableType<CanvasTextureImage>(uri,
                                        1, 0,
                                        "TextureImage",
                                        QLatin1String("Trying to create uncreatable: TextureImage, use TextureImageFactory.newTexImage() instead."));
    qmlRegisterType<Canvas>(uri,
                            1, 0,
                            "Canvas3D");
    qmlRegisterType<CanvasContextAttributes>(uri,
                                             1, 0,
                                             "Canvas3DContextAttributes");
    qmlRegisterUncreatableType<CanvasShaderPrecisionFormat>(uri,
                                                            1, 0,
                                                            "Canvas3DShaderPrecisionFormat",
                                                            QLatin1String("Trying to create uncreatable: Canvas3DShaderPrecisionFormat."));
    qmlRegisterUncreatableType<CanvasContext>(uri,
                                              1, 0,
                                              "Context3D",
                                              QLatin1String("Trying to create uncreatable: Context3D, use Canvas3D.getContext() instead."));
    qmlRegisterUncreatableType<CanvasActiveInfo>(uri,
                                                 1, 0,
                                                 "Canvas3DActiveInfo",
                                                 QLatin1String("Trying to create uncreatable: Canvas3DActiveInfo, use Context3D.getActiveAttrib() or Context3D.getActiveUniform() instead."));
    qmlRegisterUncreatableType<CanvasTexture>(uri,
                                              1, 0,
                                              "Canvas3DTexture",
                                              QLatin1String("Trying to create uncreatable: Canvas3DTexture, use Context3D.createTexture() instead."));
    qmlRegisterUncreatableType<CanvasShader>(uri,
                                             1, 0,
                                             "Canvas3DShader",
                                             QLatin1String("Trying to create uncreatable: Canvas3DShader, use Context3D.createShader() instead."));
    qmlRegisterUncreatableType<CanvasFrameBuffer>(uri,
                                                  1, 0,
                                                  "Canvas3DFrameBuffer",
                                                  QLatin1String("Trying to create uncreatable: Canvas3DFrameBuffer, use Context3D.createFramebuffer() instead."));
    qmlRegisterUncreatableType<CanvasRenderBuffer>(uri,
                                                   1, 0,
                                                   "Canvas3DRenderBuffer",
                                                   QLatin1String("Trying to create uncreatable: Canvas3DRenderBuffer, use Context3D.createRenderbuffer() instead."));
    qmlRegisterUncreatableType<CanvasProgram>(uri,
                                              1, 0,
                                              "Canvas3DProgram",
                                              QLatin1String("Trying to create uncreatable: Canvas3DProgram, use Context3D.createProgram() instead."));
    qmlRegisterUncreatableType<CanvasBuffer>(uri,
                                             1, 0,
                                             "Canvas3DBuffer",
                                             QLatin1String("Trying to create uncreatable: Canvas3DBuffer, use Context3D.createBuffer() instead."));
    qmlRegisterUncreatableType<CanvasUniformLocation>(uri,
                                                      1, 0,
                                                      "Canvas3DUniformLocation",
                                                      QLatin1String("Trying to create uncreatable: Canvas3DUniformLocation, use Context3D.getUniformLocation() instead."));

    // EXTENSIONS
    qmlRegisterUncreatableType<CanvasGLStateDump>(uri,
                                                  1, 0,
                                                  "GLStateDumpExt",
                                                  QLatin1String("Trying to create uncreatable: GLStateDumpExt, use Context3D.getExtension(\"QTCANVAS3D_gl_state_dump\") instead."));

    // QtCanvas3D 1.1

    // New revisions
    qmlRegisterType<Canvas, 1>(uri, 1, 1, "Canvas3D");

    // New types
    qmlRegisterUncreatableType<CanvasTextureProvider>(uri,
                                                      1, 1,
                                                      "Canvas3DTextureProvider",
                                                      QLatin1String("Trying to create uncreatable: Canvas3DTextureProvider, use Context3D.getExtension(\"QTCANVAS3D_texture_provider\") instead."));
}

QT_CANVAS3D_END_NAMESPACE

QT_END_NAMESPACE
