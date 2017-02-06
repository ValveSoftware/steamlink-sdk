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

#include "qsgsoftwarelayer_p.h"

#include "qsgsoftwarecontext_p.h"
#include "qsgsoftwarepixmaprenderer_p.h"

QT_BEGIN_NAMESPACE

QSGSoftwareLayer::QSGSoftwareLayer(QSGRenderContext *renderContext)
    : m_item(0)
    , m_context(renderContext)
    , m_renderer(0)
    , m_device_pixel_ratio(1)
    , m_mirrorHorizontal(false)
    , m_mirrorVertical(false)
    , m_live(true)
    , m_grab(true)
    , m_recursive(false)
    , m_dirtyTexture(true)
{

}

QSGSoftwareLayer::~QSGSoftwareLayer()
{
    invalidated();
}

int QSGSoftwareLayer::textureId() const
{
    return 0;
}

QSize QSGSoftwareLayer::textureSize() const
{
    return m_pixmap.size();
}

bool QSGSoftwareLayer::hasAlphaChannel() const
{
    return m_pixmap.hasAlphaChannel();
}

bool QSGSoftwareLayer::hasMipmaps() const
{
    return false;
}

void QSGSoftwareLayer::bind()
{
}

bool QSGSoftwareLayer::updateTexture()
{
    bool doGrab = (m_live || m_grab) && m_dirtyTexture;
    if (doGrab)
        grab();
    if (m_grab)
        emit scheduledUpdateCompleted();
    m_grab = false;
    return doGrab;
}

void QSGSoftwareLayer::setItem(QSGNode *item)
{
    if (item == m_item)
        return;
    m_item = item;

    if (m_live && !m_item)
        m_pixmap = QPixmap();

    markDirtyTexture();
}

void QSGSoftwareLayer::setRect(const QRectF &rect)
{
    if (rect == m_rect)
        return;
    m_rect = rect;
    markDirtyTexture();
}

void QSGSoftwareLayer::setSize(const QSize &size)
{
    if (size == m_size)
        return;
    m_size = size;

    if (m_live && m_size.isNull())
        m_pixmap = QPixmap();

    markDirtyTexture();
}

void QSGSoftwareLayer::scheduleUpdate()
{
    if (m_grab)
        return;
    m_grab = true;
    if (m_dirtyTexture) {
        emit updateRequested();
    }
}

QImage QSGSoftwareLayer::toImage() const
{
    return m_pixmap.toImage();
}

void QSGSoftwareLayer::setLive(bool live)
{
    if (live == m_live)
        return;
    m_live = live;

    if (m_live && (!m_item || m_size.isNull()))
        m_pixmap = QPixmap();

    markDirtyTexture();
}

void QSGSoftwareLayer::setRecursive(bool recursive)
{
    m_recursive = recursive;
}

void QSGSoftwareLayer::setFormat(uint)
{
}

void QSGSoftwareLayer::setHasMipmaps(bool)
{
}

void QSGSoftwareLayer::setDevicePixelRatio(qreal ratio)
{
    m_device_pixel_ratio = ratio;
}

void QSGSoftwareLayer::setMirrorHorizontal(bool mirror)
{
    if (m_mirrorHorizontal == mirror)
        return;
    m_mirrorHorizontal = mirror;
    markDirtyTexture();
}

void QSGSoftwareLayer::setMirrorVertical(bool mirror)
{
    if (m_mirrorVertical == mirror)
        return;
    m_mirrorVertical = mirror;
    markDirtyTexture();
}

void QSGSoftwareLayer::markDirtyTexture()
{
    m_dirtyTexture = true;
    if (m_live || m_grab) {
        emit updateRequested();
    }
}

void QSGSoftwareLayer::invalidated()
{
    delete m_renderer;
    m_renderer = 0;
}

void QSGSoftwareLayer::grab()
{
    if (!m_item || m_size.isNull()) {
        m_pixmap = QPixmap();
        m_dirtyTexture = false;
        return;
    }
    QSGNode *root = m_item;
    while (root->firstChild() && root->type() != QSGNode::RootNodeType)
        root = root->firstChild();
    if (root->type() != QSGNode::RootNodeType)
        return;

    if (!m_renderer) {
        m_renderer = new QSGSoftwarePixmapRenderer(m_context);
        connect(m_renderer, SIGNAL(sceneGraphChanged()), this, SLOT(markDirtyTexture()));
    }
    m_renderer->setDevicePixelRatio(m_device_pixel_ratio);
    m_renderer->setRootNode(static_cast<QSGRootNode *>(root));

    if (m_pixmap.size() != m_size) {
        m_pixmap = QPixmap(m_size);
        m_pixmap.setDevicePixelRatio(m_device_pixel_ratio);
        // This fill here is wasteful, but necessary because it is the only way
        // to force a QImage based pixmap to have an alpha channel.
        m_pixmap.fill(Qt::transparent);
    }

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
    m_renderer->setProjectionRect(mirrored);
    m_renderer->setClearColor(Qt::transparent);

    m_renderer->renderScene();
    m_renderer->render(&m_pixmap);

    root->markDirty(QSGNode::DirtyForceUpdate); // Force matrix, clip, opacity and render list update.

    if (m_recursive)
        markDirtyTexture(); // Continuously update if 'live' and 'recursive'.
}

QT_END_NAMESPACE
