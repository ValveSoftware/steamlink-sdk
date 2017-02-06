/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "openglversionchecker_p.h"
#include "canvas3d_p.h"
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

OpenGLVersionChecker::OpenGLVersionChecker() :
    m_softwareRenderer(false)
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    QOffscreenSurface *surface = 0;
    bool createdContext = false;
    if (!context) {
        context = new QOpenGLContext;
        createdContext = context->create();
        if (!createdContext) {
            qCWarning(canvas3drendering,
                      "%s Warning: Couldn't resolve context for version checking.", __FUNCTION__);
            delete context;
            return;
        } else {
            surface = new QOffscreenSurface();
            surface->setFormat(context->format());
            surface->create();
            context->makeCurrent(surface);
        }
    }

    const GLubyte *glVersion = context->functions()->glGetString(GL_VERSION);
    qCDebug(canvas3drendering, "%s OpenGL version: %s", __FUNCTION__, (const char*)glVersion);

    const GLubyte *glslVersion = context->functions()->glGetString(GL_SHADING_LANGUAGE_VERSION);
    qCDebug(canvas3drendering, "%s GLSL version: %s", __FUNCTION__, (const char*)glslVersion);

    qCDebug(canvas3drendering) << __FUNCTION__ << "EXTENSIONS: " << context->extensions();

    QString versionStr = QString::fromLatin1((const char *)glVersion).toLower();
    if (versionStr.contains(QStringLiteral("mesa"))
            || versionStr.contains(QStringLiteral("llvmpipe"))) {
        m_softwareRenderer = true;
    }
    if (createdContext) {
        context->doneCurrent();
        delete context;
        delete surface;
    }
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
