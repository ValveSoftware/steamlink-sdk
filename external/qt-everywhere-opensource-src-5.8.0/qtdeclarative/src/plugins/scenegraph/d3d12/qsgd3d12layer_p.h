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

#ifndef QSGD3D12LAYER_P_H
#define QSGD3D12LAYER_P_H

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

#include <private/qsgadaptationlayer_p.h>

QT_BEGIN_NAMESPACE

class QSGD3D12RenderContext;

class QSGD3D12Layer : public QSGLayer
{
    Q_OBJECT

public:
    QSGD3D12Layer(QSGD3D12RenderContext *rc);
    ~QSGD3D12Layer();

    int textureId() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;
    QRectF normalizedTextureSubRect() const override;
    void bind() override;

    bool updateTexture() override;

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

public Q_SLOTS:
    void markDirtyTexture() override;
    void invalidated() override;

private:
    void cleanup();
    void resetRenderTarget();
    void updateContent();

    QSGD3D12RenderContext *m_rc;
    uint m_rt = 0;
    uint m_secondaryRT = 0;
    QSize m_rtSize;
    QSize m_size;
    QRectF m_rect;
    QSGNode *m_item = nullptr;
    QSGRenderer *m_renderer = nullptr;
    float m_dpr = 1;
    bool m_mirrorHorizontal = false;
    bool m_mirrorVertical = true;
    bool m_live = true;
    bool m_recursive = false;
    bool m_dirtyTexture = true;
    bool m_updateContentPending = false;
};

QT_END_NAMESPACE

#endif // QSGD3D12LAYER_P_H
