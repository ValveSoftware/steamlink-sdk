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

#include "qsgdefaultcontext_p.h"

#include <QtQuick/private/qsgdistancefieldutil_p.h>
#include <QtQuick/private/qsgdefaultinternalrectanglenode_p.h>
#include <QtQuick/private/qsgdefaultinternalimagenode_p.h>
#include <QtQuick/private/qsgdefaultpainternode_p.h>
#include <QtQuick/private/qsgdefaultglyphnode_p.h>
#include <QtQuick/private/qsgdistancefieldglyphnode_p.h>
#include <QtQuick/private/qsgdistancefieldglyphnode_p_p.h>
#include <QtQuick/private/qsgrenderloop_p.h>
#include <QtQuick/private/qsgdefaultlayer_p.h>
#include <QtQuick/private/qsgdefaultrendercontext_p.h>
#include <QtQuick/private/qsgdefaultrectanglenode_p.h>
#include <QtQuick/private/qsgdefaultimagenode_p.h>
#include <QtQuick/private/qsgdefaultninepatchnode_p.h>
#if QT_CONFIG(quick_sprite)
#include <QtQuick/private/qsgdefaultspritenode_p.h>
#endif

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>

#include <QtQuick/QQuickWindow>

#include <private/qqmlglobal_p.h>

QT_BEGIN_NAMESPACE

namespace QSGMultisampleAntialiasing {
    class ImageNode : public QSGDefaultInternalImageNode {
    public:
        void setAntialiasing(bool) { }
    };


    class RectangleNode : public QSGDefaultInternalRectangleNode {
    public:
        void setAntialiasing(bool) { }
    };
}

DEFINE_BOOL_CONFIG_OPTION(qmlDisableDistanceField, QML_DISABLE_DISTANCEFIELD)

QSGDefaultContext::QSGDefaultContext(QObject *parent)
    : QSGContext (parent)
    , m_antialiasingMethod(QSGContext::UndecidedAntialiasing)
    , m_distanceFieldDisabled(qmlDisableDistanceField())
    , m_distanceFieldAntialiasing(QSGGlyphNode::HighQualitySubPixelAntialiasing)
    , m_distanceFieldAntialiasingDecided(false)
{
    if (Q_UNLIKELY(!qEnvironmentVariableIsEmpty("QSG_DISTANCEFIELD_ANTIALIASING"))) {
        const QByteArray mode = qgetenv("QSG_DISTANCEFIELD_ANTIALIASING");
        m_distanceFieldAntialiasingDecided = true;
        if (mode == "subpixel")
            m_distanceFieldAntialiasing = QSGGlyphNode::HighQualitySubPixelAntialiasing;
        else if (mode == "subpixel-lowq")
            m_distanceFieldAntialiasing = QSGGlyphNode::LowQualitySubPixelAntialiasing;
        else if (mode == "gray")
            m_distanceFieldAntialiasing = QSGGlyphNode::GrayAntialiasing;
    }

    // Adds compatibility with Qt 5.3 and earlier's QSG_RENDER_TIMING
    if (qEnvironmentVariableIsSet("QSG_RENDER_TIMING")) {
        const_cast<QLoggingCategory &>(QSG_LOG_TIME_GLYPH()).setEnabled(QtDebugMsg, true);
        const_cast<QLoggingCategory &>(QSG_LOG_TIME_TEXTURE()).setEnabled(QtDebugMsg, true);
        const_cast<QLoggingCategory &>(QSG_LOG_TIME_RENDERER()).setEnabled(QtDebugMsg, true);
        const_cast<QLoggingCategory &>(QSG_LOG_TIME_RENDERLOOP()).setEnabled(QtDebugMsg, true);
        const_cast<QLoggingCategory &>(QSG_LOG_TIME_COMPILATION()).setEnabled(QtDebugMsg, true);
    }
}

QSGDefaultContext::~QSGDefaultContext()
{

}

void QSGDefaultContext::renderContextInitialized(QSGRenderContext *renderContext)
{
    m_mutex.lock();

    auto openglRenderContext = static_cast<const QSGDefaultRenderContext *>(renderContext);
    if (m_antialiasingMethod == UndecidedAntialiasing) {
        if (Q_UNLIKELY(qEnvironmentVariableIsSet("QSG_ANTIALIASING_METHOD"))) {
            const QByteArray aaType = qgetenv("QSG_ANTIALIASING_METHOD");
            if (aaType == "msaa")
                m_antialiasingMethod = MsaaAntialiasing;
            else if (aaType == "vertex")
                m_antialiasingMethod = VertexAntialiasing;
        }
        if (m_antialiasingMethod == UndecidedAntialiasing) {
            if (openglRenderContext->openglContext()->format().samples() > 0)
                m_antialiasingMethod = MsaaAntialiasing;
            else
                m_antialiasingMethod = VertexAntialiasing;
        }
    }

    // With OpenGL ES, except for Angle on Windows, use GrayAntialiasing, unless
    // some value had been requested explicitly. This could not be decided
    // before without a context. Now the context is ready.
    if (!m_distanceFieldAntialiasingDecided) {
        m_distanceFieldAntialiasingDecided = true;
#ifndef Q_OS_WIN32
        if (openglRenderContext->openglContext()->isOpenGLES())
            m_distanceFieldAntialiasing = QSGGlyphNode::GrayAntialiasing;
#endif
    }

    static bool dumped = false;
    if (!dumped && QSG_LOG_INFO().isDebugEnabled()) {
        dumped = true;
        QSurfaceFormat format = openglRenderContext->openglContext()->format();
        QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
        qCDebug(QSG_LOG_INFO) << "R/G/B/A Buffers:   " << format.redBufferSize() << format.greenBufferSize() << format.blueBufferSize() << format.alphaBufferSize();
        qCDebug(QSG_LOG_INFO) << "Depth Buffer:      " << format.depthBufferSize();
        qCDebug(QSG_LOG_INFO) << "Stencil Buffer:    " << format.stencilBufferSize();
        qCDebug(QSG_LOG_INFO) << "Samples:           " << format.samples();
        qCDebug(QSG_LOG_INFO) << "GL_VENDOR:         " << (const char *) funcs->glGetString(GL_VENDOR);
        qCDebug(QSG_LOG_INFO) << "GL_RENDERER:       " << (const char *) funcs->glGetString(GL_RENDERER);
        qCDebug(QSG_LOG_INFO) << "GL_VERSION:        " << (const char *) funcs->glGetString(GL_VERSION);
        QSet<QByteArray> exts = openglRenderContext->openglContext()->extensions();
        QByteArray all;
        for (const QByteArray &e : qAsConst(exts))
            all += ' ' + e;
        qCDebug(QSG_LOG_INFO) << "GL_EXTENSIONS:    " << all.constData();
        qCDebug(QSG_LOG_INFO) << "Max Texture Size: " << openglRenderContext->maxTextureSize();
        qCDebug(QSG_LOG_INFO) << "Debug context:    " << format.testOption(QSurfaceFormat::DebugContext);
    }

    m_mutex.unlock();
}

void QSGDefaultContext::renderContextInvalidated(QSGRenderContext *)
{
}

QSGRenderContext *QSGDefaultContext::createRenderContext()
{
    return new QSGDefaultRenderContext(this);
}

QSGInternalRectangleNode *QSGDefaultContext::createInternalRectangleNode()
{
    return m_antialiasingMethod == MsaaAntialiasing
            ? new QSGMultisampleAntialiasing::RectangleNode
            : new QSGDefaultInternalRectangleNode;
}

QSGInternalImageNode *QSGDefaultContext::createInternalImageNode()
{
    return m_antialiasingMethod == MsaaAntialiasing
            ? new QSGMultisampleAntialiasing::ImageNode
            : new QSGDefaultInternalImageNode;
}

QSGPainterNode *QSGDefaultContext::createPainterNode(QQuickPaintedItem *item)
{
    return new QSGDefaultPainterNode(item);
}

QSGGlyphNode *QSGDefaultContext::createGlyphNode(QSGRenderContext *rc, bool preferNativeGlyphNode)
{
    if (m_distanceFieldDisabled || preferNativeGlyphNode) {
        return new QSGDefaultGlyphNode;
    } else {
        QSGDistanceFieldGlyphNode *node = new QSGDistanceFieldGlyphNode(rc);
        node->setPreferredAntialiasingMode(m_distanceFieldAntialiasing);
        return node;
    }
}

QSGLayer *QSGDefaultContext::createLayer(QSGRenderContext *renderContext)
{
    return new QSGDefaultLayer(renderContext);
}

QSurfaceFormat QSGDefaultContext::defaultSurfaceFormat() const
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    static bool useDepth = qEnvironmentVariableIsEmpty("QSG_NO_DEPTH_BUFFER");
    static bool useStencil = qEnvironmentVariableIsEmpty("QSG_NO_STENCIL_BUFFER");
    static bool enableDebug = qEnvironmentVariableIsSet("QSG_OPENGL_DEBUG");
    format.setDepthBufferSize(useDepth ? 24 : 0);
    format.setStencilBufferSize(useStencil ? 8 : 0);
    if (enableDebug)
        format.setOption(QSurfaceFormat::DebugContext);
    if (QQuickWindow::hasDefaultAlphaBuffer())
        format.setAlphaBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    return format;
}

void QSGDefaultContext::setDistanceFieldEnabled(bool enabled)
{
    m_distanceFieldDisabled = !enabled;
}

bool QSGDefaultContext::isDistanceFieldEnabled() const
{
    return !m_distanceFieldDisabled;
}

QSGRendererInterface *QSGDefaultContext::rendererInterface(QSGRenderContext *renderContext)
{
    Q_UNUSED(renderContext);
    return this;
}

QSGRectangleNode *QSGDefaultContext::createRectangleNode()
{
    return new QSGDefaultRectangleNode;
}

QSGImageNode *QSGDefaultContext::createImageNode()
{
    return new QSGDefaultImageNode;
}

QSGNinePatchNode *QSGDefaultContext::createNinePatchNode()
{
    return new QSGDefaultNinePatchNode;
}

#if QT_CONFIG(quick_sprite)
QSGSpriteNode *QSGDefaultContext::createSpriteNode()
{
    return new QSGDefaultSpriteNode;
}
#endif

QSGRendererInterface::GraphicsApi QSGDefaultContext::graphicsApi() const
{
    return OpenGL;
}

QSGRendererInterface::ShaderType QSGDefaultContext::shaderType() const
{
    return GLSL;
}

QSGRendererInterface::ShaderCompilationTypes QSGDefaultContext::shaderCompilationType() const
{
    return RuntimeCompilation;
}

QSGRendererInterface::ShaderSourceTypes QSGDefaultContext::shaderSourceType() const
{
    return ShaderSourceString | ShaderSourceFile;
}

QT_END_NAMESPACE
