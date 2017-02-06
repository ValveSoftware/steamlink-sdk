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
#ifndef QSGDEFAULTLAYER_P_H
#define QSGDEFAULTLAYER_P_H

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
#include <private/qsgcontext_p.h>
#include <qsgsimplerectnode.h>

QT_BEGIN_NAMESPACE

#define QSG_DEBUG_FBO_OVERLAY

class QOpenGLFramebufferObject;
class QSGDepthStencilBuffer;
class QSGDefaultRenderContext;

class Q_QUICK_PRIVATE_EXPORT QSGDefaultLayer : public QSGLayer
{
    Q_OBJECT
public:
    QSGDefaultLayer(QSGRenderContext *context);
    ~QSGDefaultLayer();

    bool updateTexture() Q_DECL_OVERRIDE;

    // The item's "paint node", not effect node.
    QSGNode *item() const { return m_item; }
    void setItem(QSGNode *item) Q_DECL_OVERRIDE;

    QRectF rect() const { return m_rect; }
    void setRect(const QRectF &rect) Q_DECL_OVERRIDE;

    QSize size() const { return m_size; }
    void setSize(const QSize &size) Q_DECL_OVERRIDE;

    void setHasMipmaps(bool mipmap) Q_DECL_OVERRIDE;

    void bind() Q_DECL_OVERRIDE;

    bool hasAlphaChannel() const Q_DECL_OVERRIDE;
    bool hasMipmaps() const Q_DECL_OVERRIDE;
    int textureId() const Q_DECL_OVERRIDE;
    QSize textureSize() const Q_DECL_OVERRIDE { return m_size; }

    GLenum format() const { return m_format; }
    void setFormat(GLenum format) Q_DECL_OVERRIDE;

    bool live() const { return bool(m_live); }
    void setLive(bool live) Q_DECL_OVERRIDE;

    bool recursive() const { return bool(m_recursive); }
    void setRecursive(bool recursive) Q_DECL_OVERRIDE;

    void setDevicePixelRatio(qreal ratio) Q_DECL_OVERRIDE { m_device_pixel_ratio = ratio; }

    bool mirrorHorizontal() const { return bool(m_mirrorHorizontal); }
    void setMirrorHorizontal(bool mirror) Q_DECL_OVERRIDE;

    bool mirrorVertical() const { return bool(m_mirrorVertical); }
    void setMirrorVertical(bool mirror) Q_DECL_OVERRIDE;

    void scheduleUpdate() Q_DECL_OVERRIDE;

    QImage toImage() const Q_DECL_OVERRIDE;

    QRectF normalizedTextureSubRect() const Q_DECL_OVERRIDE;

public Q_SLOTS:
    void markDirtyTexture() Q_DECL_OVERRIDE;
    void invalidated() Q_DECL_OVERRIDE;

private:
    void grab();

    QSGNode *m_item;
    QRectF m_rect;
    QSize m_size;
    qreal m_device_pixel_ratio;
    GLenum m_format;

    QSGRenderer *m_renderer;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLFramebufferObject *m_secondaryFbo;
    QSharedPointer<QSGDepthStencilBuffer> m_depthStencilBuffer;

    GLuint m_transparentTexture;

#ifdef QSG_DEBUG_FBO_OVERLAY
    QSGSimpleRectNode *m_debugOverlay;
#endif

    QSGDefaultRenderContext *m_context;

    uint m_mipmap : 1;
    uint m_live : 1;
    uint m_recursive : 1;
    uint m_dirtyTexture : 1;
    uint m_multisamplingChecked : 1;
    uint m_multisampling : 1;
    uint m_grab : 1;
    uint m_mirrorHorizontal : 1;
    uint m_mirrorVertical : 1;
};

QT_END_NAMESPACE

#endif // QSGDEFAULTLAYER_P_H
