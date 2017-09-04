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

#ifndef QSGOPENVGCONTEXT_H
#define QSGOPENVGCONTEXT_H

#include <private/qsgcontext_p.h>
#include <qsgrendererinterface.h>

Q_DECLARE_LOGGING_CATEGORY(QSG_OPENVG_LOG_TIME_RENDERLOOP)

QT_BEGIN_NAMESPACE

class QOpenVGContext;
class QSGOpenVGFontGlyphCache;
class QSGOpenVGFontGlyphCacheManager;

class QSGOpenVGRenderContext : public QSGRenderContext, public QSGRendererInterface
{
    Q_OBJECT
public:
    QSGOpenVGRenderContext(QSGContext *context);

    void initialize(void *context) override;
    void invalidate() override;
    void renderNextFrame(QSGRenderer *renderer, uint fboId) override;
    QSGTexture *createTexture(const QImage &image, uint flags) const override;
    QSGRenderer *createRenderer() override;
    int maxTextureSize() const override;

    // QSGRendererInterface interface
    GraphicsApi graphicsApi() const override;
    ShaderType shaderType() const override;
    ShaderCompilationTypes shaderCompilationType() const override;
    ShaderSourceTypes shaderSourceType() const override;

    QOpenVGContext* vgContext() { return m_vgContext; }
    QSGOpenVGFontGlyphCache* glyphCache(const QRawFont &rawFont);

private:
    QOpenVGContext *m_vgContext;
    QSGOpenVGFontGlyphCacheManager *m_glyphCacheManager;

};

class QSGOpenVGContext : public QSGContext
{
    Q_OBJECT
public:
    QSGOpenVGContext(QObject *parent = nullptr);

    QSGRenderContext *createRenderContext() override;
    QSGRectangleNode *createRectangleNode() override;
    QSGImageNode *createImageNode() override;
    QSGPainterNode *createPainterNode(QQuickPaintedItem *item) override;
    QSGGlyphNode *createGlyphNode(QSGRenderContext *rc, bool preferNativeGlyphNode) override;
    QSGNinePatchNode *createNinePatchNode() override;
    QSGLayer *createLayer(QSGRenderContext *renderContext) override;
    QSurfaceFormat defaultSurfaceFormat() const override;
    QSGInternalRectangleNode *createInternalRectangleNode() override;
    QSGInternalImageNode *createInternalImageNode() override;
#if QT_CONFIG(quick_sprite)
    QSGSpriteNode *createSpriteNode() override;
#endif
    QSGRendererInterface *rendererInterface(QSGRenderContext *renderContext) override;
};

#endif // QSGOPENVGCONTEXT_H

QT_END_NAMESPACE
