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

#include "canvasrendernode_p.h"
#include "canvas3d_p.h"
#include "glcommandqueue_p.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLFramebufferObject>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

CanvasRenderNode::CanvasRenderNode(QQuickWindow *window) :
    QObject(),
    QSGSimpleTextureNode(),
    m_defaultTexture(0),
    m_texture(0),
    m_window(window),
    m_textureOptions(QQuickWindow::TextureHasAlphaChannel)
{
    qCDebug(canvas3drendering).nospace() << "CanvasRenderNode::" << __FUNCTION__;

    // Our texture node must have a texture, so use a default fully transparent one pixel
    // texture. This way we don't get a black screen for a moment at startup, but instead show
    // whatever is behind the canvas.
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    funcs->glGenTextures(1, &m_defaultTexture);
    funcs->glBindTexture(GL_TEXTURE_2D, m_defaultTexture);
    uchar buf[4] = { 0, 0, 0, 0 };
    funcs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &buf);

    QQuickWindow::CreateTextureOptions defaultTextureOptions = QQuickWindow::CreateTextureOptions(
            QQuickWindow::TextureHasAlphaChannel | QQuickWindow::TextureOwnsGLTexture);
    m_texture = m_window->createTextureFromId(m_defaultTexture, QSize(1, 1), defaultTextureOptions);

    setTexture(m_texture);
    setFiltering(QSGTexture::Linear);
    setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
}

CanvasRenderNode::~CanvasRenderNode()
{
    delete m_texture;
}

void CanvasRenderNode::setAlpha(bool alpha)
{
     m_textureOptions = alpha
             ? QQuickWindow::TextureHasAlphaChannel
             : QQuickWindow::CreateTextureOption(0);
}

// Called in render thread as a response to CanvasRenderer::textureReady signal.
void CanvasRenderNode::newTexture(int id, const QSize &size)
{
    qCDebug(canvas3drendering).nospace() << "CanvasRenderNode::" << __FUNCTION__
                                         << "(" << id << ", " << size << ")";
    if (id) {
        qCDebug(canvas3drendering).nospace() << "CanvasRenderNode::" << __FUNCTION__
                                             << " showing new texture:" << id
                                             << " size:" << size
                                             << " targetRect:" << rect();

        delete m_texture;
        m_texture = m_window->createTextureFromId(id, size, m_textureOptions);
        setTexture(m_texture);

        qCDebug(canvas3drendering).nospace() << "CanvasRenderNode::" << __FUNCTION__
                                             << " SGTexture size:" << m_texture->textureSize()
                                             << " normalizedTextureSubRect:"
                                             << m_texture->normalizedTextureSubRect();
    } else {
        qCDebug(canvas3drendering).nospace() << "CanvasRenderNode::" << __FUNCTION__
                                             << " showing previous texture";
    }
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
