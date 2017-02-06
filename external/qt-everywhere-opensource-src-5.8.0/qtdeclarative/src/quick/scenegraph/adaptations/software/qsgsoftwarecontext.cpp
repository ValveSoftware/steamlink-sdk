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

#include "qsgsoftwarecontext_p.h"

#include "qsgsoftwareinternalrectanglenode_p.h"
#include "qsgsoftwareinternalimagenode_p.h"
#include "qsgsoftwarepainternode_p.h"
#include "qsgsoftwarepixmaptexture_p.h"
#include "qsgsoftwareglyphnode_p.h"
#include "qsgsoftwarepublicnodes_p.h"
#include "qsgsoftwarelayer_p.h"
#include "qsgsoftwarerenderer_p.h"
#if QT_CONFIG(quick_sprite)
#include "qsgsoftwarespritenode_p.h"
#endif

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>

#include <QtGui/QWindow>
#include <QtQuick/private/qquickwindow_p.h>

// Used for very high-level info about the renderering and gl context
// Includes GL_VERSION, type of render loop, atlas size, etc.
Q_LOGGING_CATEGORY(QSG_RASTER_LOG_INFO,                "qt.scenegraph.info")

// Used to debug the renderloop logic. Primarily useful for platform integrators
// and when investigating the render loop logic.
Q_LOGGING_CATEGORY(QSG_RASTER_LOG_RENDERLOOP,          "qt.scenegraph.renderloop")

// GLSL shader compilation
Q_LOGGING_CATEGORY(QSG_RASTER_LOG_TIME_COMPILATION,    "qt.scenegraph.time.compilation")

// polish, animations, sync, render and swap in the render loop
Q_LOGGING_CATEGORY(QSG_RASTER_LOG_TIME_RENDERLOOP,     "qt.scenegraph.time.renderloop")

// Texture uploads and swizzling
Q_LOGGING_CATEGORY(QSG_RASTER_LOG_TIME_TEXTURE,        "qt.scenegraph.time.texture")

// Glyph preparation (only for distance fields atm)
Q_LOGGING_CATEGORY(QSG_RASTER_LOG_TIME_GLYPH,          "qt.scenegraph.time.glyph")

// Timing inside the renderer base class
Q_LOGGING_CATEGORY(QSG_RASTER_LOG_TIME_RENDERER,       "qt.scenegraph.time.renderer")

QT_BEGIN_NAMESPACE

QSGSoftwareRenderContext::QSGSoftwareRenderContext(QSGContext *ctx)
    : QSGRenderContext(ctx)
    , m_initialized(false)
    , m_activePainter(nullptr)
{
}

QSGSoftwareContext::QSGSoftwareContext(QObject *parent)
    : QSGContext(parent)
{
}

QSGInternalRectangleNode *QSGSoftwareContext::createInternalRectangleNode()
{
    return new QSGSoftwareInternalRectangleNode();
}

QSGInternalImageNode *QSGSoftwareContext::createInternalImageNode()
{
    return new QSGSoftwareInternalImageNode();
}

QSGPainterNode *QSGSoftwareContext::createPainterNode(QQuickPaintedItem *item)
{
    return new QSGSoftwarePainterNode(item);
}

QSGGlyphNode *QSGSoftwareContext::createGlyphNode(QSGRenderContext *rc, bool preferNativeGlyphNode)
{
    Q_UNUSED(rc);
    Q_UNUSED(preferNativeGlyphNode);
    return new QSGSoftwareGlyphNode();
}

QSGLayer *QSGSoftwareContext::createLayer(QSGRenderContext *renderContext)
{
    return new QSGSoftwareLayer(renderContext);
}

QSurfaceFormat QSGSoftwareContext::defaultSurfaceFormat() const
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setRenderableType(QSurfaceFormat::DefaultRenderableType);
    format.setMajorVersion(0);
    format.setMinorVersion(0);
    return format;
}

void QSGSoftwareRenderContext::initializeIfNeeded()
{
    if (m_initialized)
        return;
    m_initialized = true;
    emit initialized();
}

void QSGSoftwareRenderContext::invalidate()
{
    m_sg->renderContextInvalidated(this);
    emit invalidated();
}

QSGTexture *QSGSoftwareRenderContext::createTexture(const QImage &image, uint flags) const
{
    return new QSGSoftwarePixmapTexture(image, flags);
}

QSGRenderer *QSGSoftwareRenderContext::createRenderer()
{
    return new QSGSoftwareRenderer(this);
}


void QSGSoftwareRenderContext::renderNextFrame(QSGRenderer *renderer, uint fbo)
{
    renderer->renderScene(fbo);
}

int QSGSoftwareRenderContext::maxTextureSize() const
{
    return 2048;
}

QSGRendererInterface *QSGSoftwareContext::rendererInterface(QSGRenderContext *renderContext)
{
    Q_UNUSED(renderContext);
    return this;
}

QSGRectangleNode *QSGSoftwareContext::createRectangleNode()
{
    return new QSGSoftwareRectangleNode;
}

QSGImageNode *QSGSoftwareContext::createImageNode()
{
    return new QSGSoftwareImageNode;
}

QSGNinePatchNode *QSGSoftwareContext::createNinePatchNode()
{
    return new QSGSoftwareNinePatchNode;
}

#if QT_CONFIG(quick_sprite)
QSGSpriteNode *QSGSoftwareContext::createSpriteNode()
{
    return new QSGSoftwareSpriteNode;
}
#endif

QSGRendererInterface::GraphicsApi QSGSoftwareContext::graphicsApi() const
{
    return Software;
}

QSGRendererInterface::ShaderType QSGSoftwareContext::shaderType() const
{
    return UnknownShadingLanguage;
}

QSGRendererInterface::ShaderCompilationTypes QSGSoftwareContext::shaderCompilationType() const
{
    return 0;
}

QSGRendererInterface::ShaderSourceTypes QSGSoftwareContext::shaderSourceType() const
{
    return 0;
}

void *QSGSoftwareContext::getResource(QQuickWindow *window, Resource resource) const
{
    if (resource == PainterResource && window && window->isSceneGraphInitialized())
        return static_cast<QSGSoftwareRenderContext *>(QQuickWindowPrivate::get(window)->context)->m_activePainter;

    return nullptr;
}

QT_END_NAMESPACE
