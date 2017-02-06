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
#include "qsgdefaultlayer_p.h"

#include <private/qqmlglobal_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qsgdefaultrendercontext_p.h>

#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLFunctions>

#include <QtQuick/private/qsgdepthstencilbuffer_p.h>

#ifdef QSG_DEBUG_FBO_OVERLAY
DEFINE_BOOL_CONFIG_OPTION(qmlFboOverlay, QML_FBO_OVERLAY)
#endif
DEFINE_BOOL_CONFIG_OPTION(qmlFboFlushBeforeDetach, QML_FBO_FLUSH_BEFORE_DETACH)

namespace
{
    class BindableFbo : public QSGBindable
    {
    public:
        BindableFbo(QOpenGLFramebufferObject *fbo, QSGDepthStencilBuffer *depthStencil);
        virtual ~BindableFbo();
        void bind() const Q_DECL_OVERRIDE;
    private:
        QOpenGLFramebufferObject *m_fbo;
        QSGDepthStencilBuffer *m_depthStencil;
    };

    BindableFbo::BindableFbo(QOpenGLFramebufferObject *fbo, QSGDepthStencilBuffer *depthStencil)
        : m_fbo(fbo)
        , m_depthStencil(depthStencil)
    {
    }

    BindableFbo::~BindableFbo()
    {
        if (qmlFboFlushBeforeDetach())
            QOpenGLContext::currentContext()->functions()->glFlush();
        if (m_depthStencil)
            m_depthStencil->detach();
    }

    void BindableFbo::bind() const
    {
        m_fbo->bind();
        if (m_depthStencil)
            m_depthStencil->attach();
    }
}

QSGDefaultLayer::QSGDefaultLayer(QSGRenderContext *context)
    : QSGLayer()
    , m_item(0)
    , m_device_pixel_ratio(1)
    , m_format(GL_RGBA)
    , m_renderer(0)
    , m_fbo(0)
    , m_secondaryFbo(0)
    , m_transparentTexture(0)
#ifdef QSG_DEBUG_FBO_OVERLAY
    , m_debugOverlay(0)
#endif
    , m_mipmap(false)
    , m_live(true)
    , m_recursive(false)
    , m_dirtyTexture(true)
    , m_multisamplingChecked(false)
    , m_multisampling(false)
    , m_grab(false)
    , m_mirrorHorizontal(false)
    , m_mirrorVertical(true)
{
    m_context = static_cast<QSGDefaultRenderContext *>(context);
}

QSGDefaultLayer::~QSGDefaultLayer()
{
    invalidated();
}

void QSGDefaultLayer::invalidated()
{
    delete m_renderer;
    m_renderer = 0;
    delete m_fbo;
    delete m_secondaryFbo;
    m_fbo = m_secondaryFbo = 0;
#ifdef QSG_DEBUG_FBO_OVERLAY
    delete m_debugOverlay;
    m_debugOverlay = 0;
#endif
    if (m_transparentTexture) {
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1, &m_transparentTexture);
        m_transparentTexture = 0;
    }
}

int QSGDefaultLayer::textureId() const
{
    return m_fbo ? m_fbo->texture() : 0;
}

bool QSGDefaultLayer::hasAlphaChannel() const
{
    return m_format != GL_RGB;
}

bool QSGDefaultLayer::hasMipmaps() const
{
    return m_mipmap;
}


void QSGDefaultLayer::bind()
{
#ifndef QT_NO_DEBUG
    if (!m_recursive && m_fbo && ((m_multisampling && m_secondaryFbo->isBound()) || m_fbo->isBound()))
        qWarning("ShaderEffectSource: \'recursive\' must be set to true when rendering recursively.");
#endif
    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    if (!m_fbo && m_format == GL_RGBA) {
        if (m_transparentTexture == 0) {
            funcs->glGenTextures(1, &m_transparentTexture);
            funcs->glBindTexture(GL_TEXTURE_2D, m_transparentTexture);
            const uint zero = 0;
            funcs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &zero);
        } else {
            funcs->glBindTexture(GL_TEXTURE_2D, m_transparentTexture);
        }
    } else {
        funcs->glBindTexture(GL_TEXTURE_2D, m_fbo ? m_fbo->texture() : 0);
        updateBindOptions();
    }
}

bool QSGDefaultLayer::updateTexture()
{
    bool doGrab = (m_live || m_grab) && m_dirtyTexture;
    if (doGrab)
        grab();
    if (m_grab)
        emit scheduledUpdateCompleted();
    m_grab = false;
    return doGrab;
}

void QSGDefaultLayer::setHasMipmaps(bool mipmap)
{
    if (mipmap == m_mipmap)
        return;
    m_mipmap = mipmap;
    if (m_mipmap && m_fbo && !m_fbo->format().mipmap())
        markDirtyTexture();
}


void QSGDefaultLayer::setItem(QSGNode *item)
{
    if (item == m_item)
        return;
    m_item = item;

    if (m_live && !m_item) {
        delete m_fbo;
        delete m_secondaryFbo;
        m_fbo = m_secondaryFbo = 0;
        m_depthStencilBuffer.clear();
    }

    markDirtyTexture();
}

void QSGDefaultLayer::setRect(const QRectF &rect)
{
    if (rect == m_rect)
        return;
    m_rect = rect;
    markDirtyTexture();
}

void QSGDefaultLayer::setSize(const QSize &size)
{
    if (size == m_size)
        return;
    m_size = size;

    if (m_live && m_size.isNull()) {
        delete m_fbo;
        delete m_secondaryFbo;
        m_fbo = m_secondaryFbo = 0;
        m_depthStencilBuffer.clear();
    }

    markDirtyTexture();
}

void QSGDefaultLayer::setFormat(GLenum format)
{
    if (format == m_format)
        return;
    m_format = format;
    markDirtyTexture();
}

void QSGDefaultLayer::setLive(bool live)
{
    if (live == m_live)
        return;
    m_live = live;

    if (m_live && (!m_item || m_size.isNull())) {
        delete m_fbo;
        delete m_secondaryFbo;
        m_fbo = m_secondaryFbo = 0;
        m_depthStencilBuffer.clear();
    }

    markDirtyTexture();
}

void QSGDefaultLayer::scheduleUpdate()
{
    if (m_grab)
        return;
    m_grab = true;
    if (m_dirtyTexture)
        emit updateRequested();
}

void QSGDefaultLayer::setRecursive(bool recursive)
{
    m_recursive = recursive;
}

void QSGDefaultLayer::setMirrorHorizontal(bool mirror)
{
    m_mirrorHorizontal = mirror;
}

void QSGDefaultLayer::setMirrorVertical(bool mirror)
{
    m_mirrorVertical = mirror;
}

void QSGDefaultLayer::markDirtyTexture()
{
    m_dirtyTexture = true;
    if (m_live || m_grab)
        emit updateRequested();
}

void QSGDefaultLayer::grab()
{
    if (!m_item || m_size.isNull()) {
        delete m_fbo;
        delete m_secondaryFbo;
        m_fbo = m_secondaryFbo = 0;
        m_depthStencilBuffer.clear();
        m_dirtyTexture = false;
        return;
    }
    QSGNode *root = m_item;
    while (root->firstChild() && root->type() != QSGNode::RootNodeType)
        root = root->firstChild();
    if (root->type() != QSGNode::RootNodeType)
        return;

    if (!m_renderer) {
        m_renderer = m_context->createRenderer();
        connect(m_renderer, SIGNAL(sceneGraphChanged()), this, SLOT(markDirtyTexture()));
    }
    m_renderer->setDevicePixelRatio(m_device_pixel_ratio);
    m_renderer->setRootNode(static_cast<QSGRootNode *>(root));

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();
    bool deleteFboLater = false;
    if (!m_fbo || m_fbo->size() != m_size || m_fbo->format().internalTextureFormat() != m_format
        || (!m_fbo->format().mipmap() && m_mipmap))
    {
        if (!m_multisamplingChecked) {
            if (m_context->openglContext()->format().samples() <= 1) {
                m_multisampling = false;
            } else {
                const QSet<QByteArray> extensions = m_context->openglContext()->extensions();
                m_multisampling = extensions.contains(QByteArrayLiteral("GL_EXT_framebuffer_multisample"))
                    && extensions.contains(QByteArrayLiteral("GL_EXT_framebuffer_blit"));
            }
            m_multisamplingChecked = true;
        }
        if (m_multisampling) {
            // Don't delete the FBO right away in case it is used recursively.
            deleteFboLater = true;
            delete m_secondaryFbo;
            QOpenGLFramebufferObjectFormat format;

            format.setInternalTextureFormat(m_format);
            format.setSamples(m_context->openglContext()->format().samples());
            m_secondaryFbo = new QOpenGLFramebufferObject(m_size, format);
            m_depthStencilBuffer = m_context->depthStencilBufferForFbo(m_secondaryFbo);
        } else {
            QOpenGLFramebufferObjectFormat format;
            format.setInternalTextureFormat(m_format);
            format.setMipmap(m_mipmap);
            if (m_recursive) {
                deleteFboLater = true;
                delete m_secondaryFbo;
                m_secondaryFbo = new QOpenGLFramebufferObject(m_size, format);
                funcs->glBindTexture(GL_TEXTURE_2D, m_secondaryFbo->texture());
                updateBindOptions(true);
                m_depthStencilBuffer = m_context->depthStencilBufferForFbo(m_secondaryFbo);
            } else {
                delete m_fbo;
                delete m_secondaryFbo;
                m_fbo = new QOpenGLFramebufferObject(m_size, format);
                m_secondaryFbo = 0;
                funcs->glBindTexture(GL_TEXTURE_2D, m_fbo->texture());
                updateBindOptions(true);
                m_depthStencilBuffer = m_context->depthStencilBufferForFbo(m_fbo);
            }
        }
    }

    if (m_recursive && !m_secondaryFbo) {
        // m_fbo already created, m_recursive was just set.
        Q_ASSERT(m_fbo);
        Q_ASSERT(!m_multisampling);

        m_secondaryFbo = new QOpenGLFramebufferObject(m_size, m_fbo->format());
        funcs->glBindTexture(GL_TEXTURE_2D, m_secondaryFbo->texture());
        updateBindOptions(true);
    }

    // Render texture.
    root->markDirty(QSGNode::DirtyForceUpdate); // Force matrix, clip and opacity update.
    m_renderer->nodeChanged(root, QSGNode::DirtyForceUpdate); // Force render list update.

#ifdef QSG_DEBUG_FBO_OVERLAY
    if (qmlFboOverlay()) {
        if (!m_debugOverlay)
            m_debugOverlay = new QSGSimpleRectNode();
        m_debugOverlay->setRect(QRectF(0, 0, m_size.width(), m_size.height()));
        m_debugOverlay->setColor(QColor(0xff, 0x00, 0x80, 0x40));
        root->appendChildNode(m_debugOverlay);
    }
#endif

    m_dirtyTexture = false;

    m_renderer->setDeviceRect(m_size);
    m_renderer->setViewportRect(m_size);
    QRectF mirrored(m_mirrorHorizontal ? m_rect.right() : m_rect.left(),
                    m_mirrorVertical ? m_rect.bottom() : m_rect.top(),
                    m_mirrorHorizontal ? -m_rect.width() : m_rect.width(),
                    m_mirrorVertical ? -m_rect.height() : m_rect.height());
    m_renderer->setProjectionMatrixToRect(mirrored);
    m_renderer->setClearColor(Qt::transparent);

    if (m_multisampling) {
        m_renderer->renderScene(BindableFbo(m_secondaryFbo, m_depthStencilBuffer.data()));

        if (deleteFboLater) {
            delete m_fbo;
            QOpenGLFramebufferObjectFormat format;
            format.setInternalTextureFormat(m_format);
            format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
            format.setMipmap(m_mipmap);
            format.setSamples(0);
            m_fbo = new QOpenGLFramebufferObject(m_size, format);
            funcs->glBindTexture(GL_TEXTURE_2D, m_fbo->texture());
            updateBindOptions(true);
        }

        QRect r(QPoint(), m_size);
        QOpenGLFramebufferObject::blitFramebuffer(m_fbo, r, m_secondaryFbo, r);
    } else {
        if (m_recursive) {
            m_renderer->renderScene(BindableFbo(m_secondaryFbo, m_depthStencilBuffer.data()));

            if (deleteFboLater) {
                delete m_fbo;
                QOpenGLFramebufferObjectFormat format;
                format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
                format.setInternalTextureFormat(m_format);
                format.setMipmap(m_mipmap);
                m_fbo = new QOpenGLFramebufferObject(m_size, format);
                funcs->glBindTexture(GL_TEXTURE_2D, m_fbo->texture());
                updateBindOptions(true);
            }
            qSwap(m_fbo, m_secondaryFbo);
        } else {
            m_renderer->renderScene(BindableFbo(m_fbo, m_depthStencilBuffer.data()));
        }
    }

    if (m_mipmap) {
        funcs->glBindTexture(GL_TEXTURE_2D, textureId());
        funcs->glGenerateMipmap(GL_TEXTURE_2D);
    }

    root->markDirty(QSGNode::DirtyForceUpdate); // Force matrix, clip, opacity and render list update.

#ifdef QSG_DEBUG_FBO_OVERLAY
    if (qmlFboOverlay())
        root->removeChildNode(m_debugOverlay);
#endif
    if (m_recursive)
        markDirtyTexture(); // Continuously update if 'live' and 'recursive'.
}

QImage QSGDefaultLayer::toImage() const
{
    if (m_fbo)
        return m_fbo->toImage();

    return QImage();
}

QRectF QSGDefaultLayer::normalizedTextureSubRect() const
{
    return QRectF(m_mirrorHorizontal ? 1 : 0,
                  m_mirrorVertical ? 0 : 1,
                  m_mirrorHorizontal ? -1 : 1,
                  m_mirrorVertical ? 1 : -1);
}
