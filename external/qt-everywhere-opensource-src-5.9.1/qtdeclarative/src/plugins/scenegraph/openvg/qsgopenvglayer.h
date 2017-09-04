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

#ifndef QSGOPENVGLAYER_H
#define QSGOPENVGLAYER_H

#include <private/qsgadaptationlayer_p.h>
#include <private/qsgcontext_p.h>

#include "qopenvgcontext_p.h"
#include "qopenvgoffscreensurface.h"

QT_BEGIN_NAMESPACE

class QSGOpenVGRenderer;
class QSGOpenVGRenderContext;

class QSGOpenVGLayer : public QSGLayer
{
public:
    QSGOpenVGLayer(QSGRenderContext *renderContext);
    ~QSGOpenVGLayer();

    // QSGTexture interface
public:
    int textureId() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;
    void bind() override;

    // QSGDynamicTexture interface
public:
    bool updateTexture() override;

    // QSGLayer interface
public:
    void setItem(QSGNode *item) override;
    void setRect(const QRectF &rect) override;
    void setSize(const QSize &size) override;
    void scheduleUpdate() override;
    QImage toImage() const override;
    void setLive(bool live) override;
    void setRecursive(bool recursive) override;
    void setFormat(uint format) override;
    void setHasMipmaps(bool mipmap) override;
    void setDevicePixelRatio(qreal ratio) override;
    void setMirrorHorizontal(bool mirror) override;
    void setMirrorVertical(bool mirror) override;

public slots:
    void markDirtyTexture() override;
    void invalidated() override;

private:
    void grab();

    QSGNode *m_item;
    QSGOpenVGRenderContext *m_context;
    QSGOpenVGRenderer *m_renderer;
    QRectF m_rect;
    QSize m_size;
    qreal m_device_pixel_ratio;
    bool m_mirrorHorizontal;
    bool m_mirrorVertical;
    bool m_live;
    bool m_grab;
    bool m_recursive;
    bool m_dirtyTexture;

    QOpenVGOffscreenSurface *m_offscreenSurface;
    QOpenVGOffscreenSurface *m_secondaryOffscreenSurface;
};

QT_END_NAMESPACE

#endif // QSGOPENVGLAYER_H
