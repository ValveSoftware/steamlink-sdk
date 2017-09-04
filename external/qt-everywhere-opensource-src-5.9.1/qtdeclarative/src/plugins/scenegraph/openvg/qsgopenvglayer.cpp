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

#include "qsgopenvglayer.h"
#include "qsgopenvgrenderer_p.h"
#include "qsgopenvgcontext_p.h"

QT_BEGIN_NAMESPACE

QSGOpenVGLayer::QSGOpenVGLayer(QSGRenderContext *renderContext)
    : m_item(nullptr)
    , m_renderer(nullptr)
    , m_device_pixel_ratio(1)
    , m_mirrorHorizontal(false)
    , m_mirrorVertical(false)
    , m_live(true)
    , m_grab(true)
    , m_recursive(false)
    , m_dirtyTexture(true)
    , m_offscreenSurface(nullptr)
    , m_secondaryOffscreenSurface(nullptr)
{
    m_context = static_cast<QSGOpenVGRenderContext*>(renderContext);
}

QSGOpenVGLayer::~QSGOpenVGLayer()
{
    invalidated();
}

int QSGOpenVGLayer::textureId() const
{
    if (m_offscreenSurface)
        return static_cast<int>(m_offscreenSurface->image());
    else
        return 0;
}

QSize QSGOpenVGLayer::textureSize() const
{
    if (m_offscreenSurface) {
        return m_offscreenSurface->size();
    }

    return QSize();
}

bool QSGOpenVGLayer::hasAlphaChannel() const
{
    return true;
}

bool QSGOpenVGLayer::hasMipmaps() const
{
    return false;
}

void QSGOpenVGLayer::bind()
{
}

bool QSGOpenVGLayer::updateTexture()
{
    bool doGrab = (m_live || m_grab) && m_dirtyTexture;
    if (doGrab)
        grab();
    if (m_grab)
        emit scheduledUpdateCompleted();
    m_grab = false;
    return doGrab;
}

void QSGOpenVGLayer::setItem(QSGNode *item)
{
    if (item == m_item)
        return;
    m_item = item;

    if (m_live && !m_item) {
        delete m_offscreenSurface;
        delete m_secondaryOffscreenSurface;
        m_offscreenSurface = nullptr;
        m_secondaryOffscreenSurface = nullptr;
    }

    markDirtyTexture();
}

void QSGOpenVGLayer::setRect(const QRectF &rect)
{
    if (rect == m_rect)
        return;
    m_rect = rect;
    markDirtyTexture();
}

void QSGOpenVGLayer::setSize(const QSize &size)
{
    if (size == m_size)
        return;
    m_size = size;

    if (m_live && m_size.isNull()) {
        delete m_offscreenSurface;
        delete m_secondaryOffscreenSurface;
        m_offscreenSurface = nullptr;
        m_secondaryOffscreenSurface = nullptr;
    }

    markDirtyTexture();
}

void QSGOpenVGLayer::scheduleUpdate()
{
    if (m_grab)
        return;
    m_grab = true;
    if (m_dirtyTexture) {
        emit updateRequested();
    }
}

QImage QSGOpenVGLayer::toImage() const
{
    return m_offscreenSurface->readbackQImage();
}

void QSGOpenVGLayer::setLive(bool live)
{
    if (live == m_live)
        return;
    m_live = live;

    if (m_live && (!m_item || m_size.isNull())) {
        delete m_offscreenSurface;
        delete m_secondaryOffscreenSurface;
        m_offscreenSurface = nullptr;
        m_secondaryOffscreenSurface = nullptr;
    }

    markDirtyTexture();
}

void QSGOpenVGLayer::setRecursive(bool recursive)
{
    m_recursive = recursive;
}

void QSGOpenVGLayer::setFormat(uint format)
{
    Q_UNUSED(format)
}

void QSGOpenVGLayer::setHasMipmaps(bool mipmap)
{
    Q_UNUSED(mipmap)
}

void QSGOpenVGLayer::setDevicePixelRatio(qreal ratio)
{
    m_device_pixel_ratio = ratio;
}

void QSGOpenVGLayer::setMirrorHorizontal(bool mirror)
{
    if (m_mirrorHorizontal == mirror)
        return;
    m_mirrorHorizontal = mirror;
    markDirtyTexture();
}

void QSGOpenVGLayer::setMirrorVertical(bool mirror)
{
    if (m_mirrorVertical == mirror)
        return;
    m_mirrorVertical = mirror;
    markDirtyTexture();
}

void QSGOpenVGLayer::markDirtyTexture()
{
    m_dirtyTexture = true;
    if (m_live || m_grab) {
        emit updateRequested();
    }
}

void QSGOpenVGLayer::invalidated()
{
    delete m_offscreenSurface;
    delete m_secondaryOffscreenSurface;
    delete m_renderer;
    m_renderer = nullptr;
    m_offscreenSurface = nullptr;
    m_secondaryOffscreenSurface = nullptr;
}

void QSGOpenVGLayer::grab()
{
    if (!m_item || m_size.isNull()) {
        delete m_offscreenSurface;
        delete m_secondaryOffscreenSurface;
        m_offscreenSurface = nullptr;
        m_secondaryOffscreenSurface = nullptr;
        m_dirtyTexture = false;
        return;
    }
    QSGNode *root = m_item;
    while (root->firstChild() && root->type() != QSGNode::RootNodeType)
        root = root->firstChild();
    if (root->type() != QSGNode::RootNodeType)
        return;

    if (!m_renderer) {
        m_renderer = new QSGOpenVGRenderer(m_context);
        connect(m_renderer, SIGNAL(sceneGraphChanged()), this, SLOT(markDirtyTexture()));
    }
    m_renderer->setDevicePixelRatio(m_device_pixel_ratio);
    m_renderer->setRootNode(static_cast<QSGRootNode *>(root));

    bool deleteOffscreenSurfaceLater = false;
    if (m_offscreenSurface == nullptr || m_offscreenSurface->size() != m_size ) {
        if (m_recursive) {
            deleteOffscreenSurfaceLater = true;
            delete m_secondaryOffscreenSurface;
            m_secondaryOffscreenSurface = new QOpenVGOffscreenSurface(m_size);
        } else {
            delete m_offscreenSurface;
            delete m_secondaryOffscreenSurface;
            m_offscreenSurface = new QOpenVGOffscreenSurface(m_size);
            m_secondaryOffscreenSurface = nullptr;
        }
    }

    if (m_recursive && !m_secondaryOffscreenSurface)
        m_secondaryOffscreenSurface = new QOpenVGOffscreenSurface(m_size);

    // Render texture.
    root->markDirty(QSGNode::DirtyForceUpdate); // Force matrix, clip and opacity update.
    m_renderer->nodeChanged(root, QSGNode::DirtyForceUpdate); // Force render list update.

    m_dirtyTexture = false;

    m_renderer->setDeviceRect(m_size);
    m_renderer->setViewportRect(m_size);
    QRect mirrored(m_mirrorHorizontal ? m_rect.right() * m_device_pixel_ratio : m_rect.left() * m_device_pixel_ratio,
                   m_mirrorVertical ? m_rect.top() * m_device_pixel_ratio : m_rect.bottom() * m_device_pixel_ratio,
                   m_mirrorHorizontal ? -m_rect.width() * m_device_pixel_ratio : m_rect.width() * m_device_pixel_ratio,
                   m_mirrorVertical ? m_rect.height() * m_device_pixel_ratio : -m_rect.height() * m_device_pixel_ratio);
    m_renderer->setProjectionMatrixToRect(mirrored);
    m_renderer->setClearColor(Qt::transparent);


    if (m_recursive)
        m_secondaryOffscreenSurface->makeCurrent();
    else
        m_offscreenSurface->makeCurrent();

    m_renderer->renderScene();

    // Make the previous surface and context active again
    if (m_recursive) {
        if (deleteOffscreenSurfaceLater) {
            delete m_offscreenSurface;
            m_offscreenSurface = new QOpenVGOffscreenSurface(m_size);
        }
        m_secondaryOffscreenSurface->doneCurrent();
        qSwap(m_offscreenSurface, m_secondaryOffscreenSurface);
    } else {
        m_offscreenSurface->doneCurrent();
    }

    root->markDirty(QSGNode::DirtyForceUpdate); // Force matrix, clip, opacity and render list update.

    if (m_recursive)
        markDirtyTexture(); // Continuously update if 'live' and 'recursive'.
}

QT_END_NAMESPACE
