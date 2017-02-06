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

#ifndef QSGDEFAULTRENDERCONTEXT_H
#define QSGDEFAULTRENDERCONTEXT_H

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

#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/private/qsgdepthstencilbuffer_p.h>

QT_BEGIN_NAMESPACE

class QOpenGLContext;
class QSGMaterialShader;
class QOpenGLFramebufferObject;

namespace QSGAtlasTexture {
    class Manager;
}

class Q_QUICK_PRIVATE_EXPORT QSGDefaultRenderContext : public QSGRenderContext
{
    Q_OBJECT
public:
    QSGDefaultRenderContext(QSGContext *context);

    QOpenGLContext *openglContext() const { return m_gl; }
    bool isValid() const override { return m_gl; }

    void initialize(void *context) override;
    void invalidate() override;
    void renderNextFrame(QSGRenderer *renderer, uint fboId) override;

    QSGDistanceFieldGlyphCache *distanceFieldGlyphCache(const QRawFont &font) override;

    virtual QSharedPointer<QSGDepthStencilBuffer> depthStencilBufferForFbo(QOpenGLFramebufferObject *fbo);
    QSGDepthStencilBufferManager *depthStencilBufferManager();

    QSGTexture *createTexture(const QImage &image, uint flags) const override;
    QSGRenderer *createRenderer() override;

    virtual void compileShader(QSGMaterialShader *shader, QSGMaterial *material, const char *vertexCode = 0, const char *fragmentCode = 0);
    virtual void initializeShader(QSGMaterialShader *shader);

    void setAttachToGraphicsContext(bool attach) override;

    static QSGDefaultRenderContext *from(QOpenGLContext *context);

    bool hasBrokenIndexBufferObjects() const { return m_brokenIBOs; }
    int maxTextureSize() const override { return m_maxTextureSize; }

protected:
    QOpenGLContext *m_gl;
    QSGDepthStencilBufferManager *m_depthStencilManager;
    int m_maxTextureSize;
    bool m_brokenIBOs;
    bool m_serializedRender;
    bool m_attachToGLContext;
    QSGAtlasTexture::Manager *m_atlasManager;


};

QT_END_NAMESPACE

#endif // QSGDEFAULTRENDERCONTEXT_H
