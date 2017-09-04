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

#include "qsgopenvgcontext_p.h"
#include "qsgopenvgrenderer_p.h"
#include "qsgopenvgpublicnodes.h"
#include "qsgopenvgtexture.h"
#include "qsgopenvglayer.h"
#include "qsgopenvgglyphnode_p.h"
#include "qsgopenvgfontglyphcache.h"
#include "qsgopenvgpainternode.h"
#if QT_CONFIG(quick_sprite)
#include "qsgopenvgspritenode.h"
#endif

#include "qopenvgcontext_p.h"

#include <private/qsgrenderer_p.h>
#include "qsgopenvginternalrectanglenode.h"
#include "qsgopenvginternalimagenode.h"

// polish, animations, sync, render and swap in the render loop
Q_LOGGING_CATEGORY(QSG_OPENVG_LOG_TIME_RENDERLOOP,     "qt.scenegraph.time.renderloop")

QT_BEGIN_NAMESPACE

QSGOpenVGRenderContext::QSGOpenVGRenderContext(QSGContext *context)
    : QSGRenderContext(context)
    , m_vgContext(nullptr)
    , m_glyphCacheManager(nullptr)
{

}

void QSGOpenVGRenderContext::initialize(void *context)
{
    m_vgContext = static_cast<QOpenVGContext*>(context);
    QSGRenderContext::initialize(context);
}

void QSGOpenVGRenderContext::invalidate()
{
    m_vgContext = nullptr;
    delete m_glyphCacheManager;
    m_glyphCacheManager = nullptr;
    QSGRenderContext::invalidate();
}

void QSGOpenVGRenderContext::renderNextFrame(QSGRenderer *renderer, uint fboId)
{
    renderer->renderScene(fboId);
}

QSGTexture *QSGOpenVGRenderContext::createTexture(const QImage &image, uint flags) const
{
    QImage tmp = image;

    // Make sure image is not larger than maxTextureSize
    int maxSize = maxTextureSize();
    if (tmp.width() > maxSize || tmp.height() > maxSize) {
        tmp = tmp.scaled(qMin(maxSize, tmp.width()), qMin(maxSize, tmp.height()), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    return new QSGOpenVGTexture(tmp, flags);
}

QSGRenderer *QSGOpenVGRenderContext::createRenderer()
{
    return new QSGOpenVGRenderer(this);
}

QSGOpenVGContext::QSGOpenVGContext(QObject *parent)
{
    Q_UNUSED(parent)
}

QSGRenderContext *QSGOpenVGContext::createRenderContext()
{
    return new QSGOpenVGRenderContext(this);
}

QSGRectangleNode *QSGOpenVGContext::createRectangleNode()
{
    return new QSGOpenVGRectangleNode;
}

QSGImageNode *QSGOpenVGContext::createImageNode()
{
    return new QSGOpenVGImageNode;
}

QSGPainterNode *QSGOpenVGContext::createPainterNode(QQuickPaintedItem *item)
{
    Q_UNUSED(item)
    return new QSGOpenVGPainterNode(item);
}

QSGGlyphNode *QSGOpenVGContext::createGlyphNode(QSGRenderContext *rc, bool preferNativeGlyphNode)
{
    Q_UNUSED(preferNativeGlyphNode)
    return new QSGOpenVGGlyphNode(rc);
}

QSGNinePatchNode *QSGOpenVGContext::createNinePatchNode()
{
    return new QSGOpenVGNinePatchNode;
}

QSGLayer *QSGOpenVGContext::createLayer(QSGRenderContext *renderContext)
{
    return new QSGOpenVGLayer(renderContext);
}

QSurfaceFormat QSGOpenVGContext::defaultSurfaceFormat() const
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setRenderableType(QSurfaceFormat::OpenVG);
    format.setMajorVersion(1);
    return format;
}

QSGInternalRectangleNode *QSGOpenVGContext::createInternalRectangleNode()
{
    return new QSGOpenVGInternalRectangleNode();
}

QSGInternalImageNode *QSGOpenVGContext::createInternalImageNode()
{
    return new QSGOpenVGInternalImageNode();
}

int QSGOpenVGRenderContext::maxTextureSize() const
{
    VGint width = vgGeti(VG_MAX_IMAGE_WIDTH);
    VGint height = vgGeti(VG_MAX_IMAGE_HEIGHT);

    return qMin(width, height);
}

#if QT_CONFIG(quick_sprite)
QSGSpriteNode *QSGOpenVGContext::createSpriteNode()
{
    return new QSGOpenVGSpriteNode();
}
#endif

QSGRendererInterface *QSGOpenVGContext::rendererInterface(QSGRenderContext *renderContext)
{
    return static_cast<QSGOpenVGRenderContext *>(renderContext);
}

QSGRendererInterface::GraphicsApi QSGOpenVGRenderContext::graphicsApi() const
{
    return OpenVG;
}

QSGRendererInterface::ShaderType QSGOpenVGRenderContext::shaderType() const
{
    return UnknownShadingLanguage;
}

QSGRendererInterface::ShaderCompilationTypes QSGOpenVGRenderContext::shaderCompilationType() const
{
    return 0;
}

QSGRendererInterface::ShaderSourceTypes QSGOpenVGRenderContext::shaderSourceType() const
{
    return 0;
}

QSGOpenVGFontGlyphCache *QSGOpenVGRenderContext::glyphCache(const QRawFont &rawFont)
{
    if (!m_glyphCacheManager)
        m_glyphCacheManager = new QSGOpenVGFontGlyphCacheManager;

    QSGOpenVGFontGlyphCache *cache = m_glyphCacheManager->cache(rawFont);
    if (!cache) {
        cache = new QSGOpenVGFontGlyphCache(m_glyphCacheManager, rawFont);
        m_glyphCacheManager->insertCache(rawFont, cache);
    }

    return cache;
}

QT_END_NAMESPACE
