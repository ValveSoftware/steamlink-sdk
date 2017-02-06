/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickcontext2dtile_p.h"
#ifndef QT_NO_OPENGL
# include <QOpenGLFramebufferObject>
# include <QOpenGLFramebufferObjectFormat>
# include <QOpenGLPaintDevice>
#endif

QT_BEGIN_NAMESPACE

QQuickContext2DTile::QQuickContext2DTile()
    : m_dirty(true)
    , m_rect(QRect(0, 0, 1, 1))
    , m_device(0)
{
}

QQuickContext2DTile::~QQuickContext2DTile()
{
    if (m_painter.isActive())
        m_painter.end();
}

QPainter* QQuickContext2DTile::createPainter(bool smooth, bool antialiasing)
{
    if (m_painter.isActive())
        m_painter.end();

    aboutToDraw();
    if (m_device) {
        m_painter.begin(m_device);
        m_painter.resetTransform();
        m_painter.setCompositionMode(QPainter::CompositionMode_Source);

#ifdef QQUICKCONTEXT2D_DEBUG
        int v = 100;
        int gray = (m_rect.x() / m_rect.width() + m_rect.y() / m_rect.height()) % 2;
        if (gray)
            v = 150;
        m_painter.fillRect(QRect(0, 0, m_rect.width(), m_rect.height()), QColor(v, v, v, 255));
#endif

        if (antialiasing)
            m_painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
        else
            m_painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);

        if (smooth)
            m_painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        else
            m_painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

        m_painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        m_painter.translate(-m_rect.left(), -m_rect.top());
        m_painter.setClipRect(m_rect);
        m_painter.setClipping(false);
        return &m_painter;
    }

    return 0;
}
#ifndef QT_NO_OPENGL
QQuickContext2DFBOTile::QQuickContext2DFBOTile()
    : QQuickContext2DTile()
    , m_fbo(0)
{
}


QQuickContext2DFBOTile::~QQuickContext2DFBOTile()
{
    if (m_fbo)
        m_fbo->release();
    delete m_fbo;
}

void QQuickContext2DFBOTile::aboutToDraw()
{
    m_fbo->bind();
    if (!m_device) {
        QOpenGLPaintDevice *gl_device = new QOpenGLPaintDevice(rect().size());
        m_device = gl_device;
        QPainter p(m_device);
        p.fillRect(QRectF(0, 0, m_fbo->width(), m_fbo->height()), QColor(qRgba(0, 0, 0, 0)));
        p.end();
    }
}

void QQuickContext2DFBOTile::drawFinished()
{
}

void QQuickContext2DFBOTile::setRect(const QRect& r)
{
    if (m_rect == r)
        return;
    m_rect = r;
    m_dirty = true;
    if (!m_fbo || m_fbo->size() != r.size()) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setInternalTextureFormat(GL_RGBA);
        format.setMipmap(false);

        if (m_painter.isActive())
            m_painter.end();

        delete m_fbo;
        m_fbo = new QOpenGLFramebufferObject(r.size(), format);
    }
}
#endif

QQuickContext2DImageTile::QQuickContext2DImageTile()
    : QQuickContext2DTile()
{
}

QQuickContext2DImageTile::~QQuickContext2DImageTile()
{
}

void QQuickContext2DImageTile::setRect(const QRect& r)
{
    if (m_rect == r)
        return;
    m_rect = r;
    m_dirty = true;
    if (m_image.size() != r.size()) {
        m_image = QImage(r.size(), QImage::Format_ARGB32_Premultiplied);
    }
    m_device = &m_image;
}

QT_END_NAMESPACE

