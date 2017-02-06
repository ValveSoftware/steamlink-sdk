/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "graphicscontext_p.h"

#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/shader_p.h>
#include <Qt3DRender/private/material_p.h>
#include <Qt3DRender/private/gltexture_p.h>
#include <Qt3DRender/private/buffer_p.h>
#include <Qt3DRender/private/attribute_p.h>
#include <Qt3DRender/private/rendercommand_p.h>
#include <Qt3DRender/private/renderstateset_p.h>
#include <Qt3DRender/private/rendertarget_p.h>
#include <Qt3DRender/private/graphicshelperinterface_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/buffermanager_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/gltexturemanager_p.h>
#include <Qt3DRender/private/attachmentpack_p.h>
#include <Qt3DRender/private/qbuffer_p.h>
#include <QOpenGLShaderProgram>

#if !defined(QT_OPENGL_ES_2)
#include <QOpenGLFunctions_2_0>
#include <QOpenGLFunctions_3_2_Core>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_3_Core>
#include <Qt3DRender/private/graphicshelpergl2_p.h>
#include <Qt3DRender/private/graphicshelpergl3_2_p.h>
#include <Qt3DRender/private/graphicshelpergl3_3_p.h>
#include <Qt3DRender/private/graphicshelpergl4_p.h>
#endif
#include <Qt3DRender/private/graphicshelperes2_p.h>
#include <Qt3DRender/private/graphicshelperes3_p.h>

#include <QSurface>
#include <QWindow>
#include <QOpenGLTexture>
#include <QOpenGLDebugLogger>

QT_BEGIN_NAMESPACE

namespace {

QOpenGLShader::ShaderType shaderType(Qt3DRender::QShaderProgram::ShaderType type)
{
    switch (type) {
    case Qt3DRender::QShaderProgram::Vertex: return QOpenGLShader::Vertex;
    case Qt3DRender::QShaderProgram::TessellationControl: return QOpenGLShader::TessellationControl;
    case Qt3DRender::QShaderProgram::TessellationEvaluation: return QOpenGLShader::TessellationEvaluation;
    case Qt3DRender::QShaderProgram::Geometry: return QOpenGLShader::Geometry;
    case Qt3DRender::QShaderProgram::Fragment: return QOpenGLShader::Fragment;
    case Qt3DRender::QShaderProgram::Compute: return QOpenGLShader::Compute;
    default: Q_UNREACHABLE();
    }
}

} // anonymous namespace

namespace Qt3DRender {
namespace Render {

static QHash<unsigned int, GraphicsContext*> static_contexts;

static void logOpenGLDebugMessage(const QOpenGLDebugMessage &debugMessage)
{
    qDebug() << "OpenGL debug message:" << debugMessage;
}

namespace {

GLBuffer::Type bufferTypeToGLBufferType(QBuffer::BufferType type)
{
    switch (type) {
    case QBuffer::VertexBuffer:
        return GLBuffer::ArrayBuffer;
    case QBuffer::IndexBuffer:
        return GLBuffer::IndexBuffer;
    case QBuffer::PixelPackBuffer:
        return GLBuffer::PixelPackBuffer;
    case QBuffer::PixelUnpackBuffer:
        return GLBuffer::PixelUnpackBuffer;
    case QBuffer::UniformBuffer:
        return GLBuffer::UniformBuffer;
    case QBuffer::ShaderStorageBuffer:
        return GLBuffer::ShaderStorageBuffer;
    default:
        Q_UNREACHABLE();
    }
}

} // anonymous

unsigned int nextFreeContextId()
{
    for (unsigned int i=0; i < 0xffff; ++i) {
        if (!static_contexts.contains(i))
            return i;
    }

    qFatal("Couldn't find free context ID");
    return 0;
}

GraphicsContext::GraphicsContext()
    : m_initialized(false)
    , m_id(nextFreeContextId())
    , m_gl(nullptr)
    , m_surface(nullptr)
    , m_glHelper(nullptr)
    , m_ownCurrent(true)
    , m_activeShader(nullptr)
    , m_activeShaderDNA(0)
    , m_currClearStencilValue(0)
    , m_currClearDepthValue(1.f)
    , m_currClearColorValue(0,0,0,0)
    , m_material(nullptr)
    , m_activeFBO(0)
    , m_defaultFBO(0)
    , m_boundArrayBuffer(nullptr)
    , m_stateSet(nullptr)
    , m_renderer(nullptr)
    , m_uboTempArray(QByteArray(1024, 0))
    , m_supportsVAO(true)
    , m_debugLogger(nullptr)
    , m_currentVAO(nullptr)
{
    static_contexts[m_id] = this;
}

GraphicsContext::~GraphicsContext()
{
    releaseOpenGL();

    Q_ASSERT(static_contexts[m_id] == this);
    static_contexts.remove(m_id);
}

void GraphicsContext::initialize()
{
    m_initialized = true;

    Q_ASSERT(m_gl);

    GLint numTexUnits;
    m_gl->functions()->glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &numTexUnits);
    qCDebug(Backend) << "context supports" << numTexUnits << "texture units";

    m_activeTextures.resize(numTexUnits);

    if (m_gl->format().majorVersion() >= 3) {
        m_supportsVAO = true;
    } else {
        QSet<QByteArray> extensions = m_gl->extensions();
        m_supportsVAO = extensions.contains(QByteArrayLiteral("GL_OES_vertex_array_object"))
                || extensions.contains(QByteArrayLiteral("GL_ARB_vertex_array_object"))
                || extensions.contains(QByteArrayLiteral("GL_APPLE_vertex_array_object"));
    }

    m_defaultFBO = m_gl->defaultFramebufferObject();
    qCDebug(Backend) << "VAO support = " << m_supportsVAO;
}

void GraphicsContext::resolveRenderTargetFormat()
{
    const QSurfaceFormat format = m_gl->format();
    const uint a = format.alphaBufferSize();
    const uint r = format.redBufferSize();
    const uint g = format.greenBufferSize();
    const uint b = format.blueBufferSize();

#define RGBA_BITS(r,g,b,a) (r | (g << 6) | (b << 12) | (a << 18))

    const uint bits = RGBA_BITS(r,g,b,a);
    switch (bits) {
    case RGBA_BITS(8,8,8,8):
        m_renderTargetFormat = QAbstractTexture::RGBA8_UNorm;
        break;
    case RGBA_BITS(8,8,8,0):
        m_renderTargetFormat = QAbstractTexture::RGB8_UNorm;
        break;
    case RGBA_BITS(5,6,5,0):
        m_renderTargetFormat = QAbstractTexture::R5G6B5;
        break;
    }
#undef RGBA_BITS
}

bool GraphicsContext::beginDrawing(QSurface *surface)
{
    Q_ASSERT(surface);
    Q_ASSERT(m_gl);

    m_surface = surface;

    // TO DO: Find a way to make to pause work if the window is not exposed
    //    if (m_surface && m_surface->surfaceClass() == QSurface::Window) {
    //        qDebug() << Q_FUNC_INFO << 1;
    //        if (!static_cast<QWindow *>(m_surface)->isExposed())
    //            return false;
    //        qDebug() << Q_FUNC_INFO << 2;
    //    }

    // Makes the surface current on the OpenGLContext
    // and sets the right glHelper
    m_ownCurrent = !(m_gl->surface() == m_surface);
    if (m_ownCurrent && !makeCurrent(m_surface))
        return false;

    // TODO: cache surface format somewhere rather than doing this every time render surface changes
    resolveRenderTargetFormat();

    // Sets or Create the correct m_glHelper
    // for the current surface
    activateGLHelper();

#if defined(QT3D_RENDER_ASPECT_OPENGL_DEBUG)
    GLint err = m_gl->functions()->glGetError();
    if (err != 0) {
        qCWarning(Backend) << Q_FUNC_INFO << "glGetError:" << err;
    }
#endif

    if (!m_initialized) {
        initialize();
    }

    // need to reset these values every frame, may get overwritten elsewhere
    m_gl->functions()->glClearColor(m_currClearColorValue.redF(), m_currClearColorValue.greenF(), m_currClearColorValue.blueF(), m_currClearColorValue.alphaF());
    m_gl->functions()->glClearDepthf(m_currClearDepthValue);
    m_gl->functions()->glClearStencil(m_currClearStencilValue);


    if (m_activeShader) {
        m_activeShader = nullptr;
        m_activeShaderDNA = 0;
    }

    // reset active textures
    for (int u = 0; u < m_activeTextures.size(); ++u)
        m_activeTextures[u].texture = nullptr;

    m_boundArrayBuffer = nullptr;

    static int callCount = 0;
    ++callCount;
    const int shaderPurgePeriod = 600;
    if (callCount % shaderPurgePeriod == 0)
        m_shaderCache.purge();

    return true;
}

void GraphicsContext::clearBackBuffer(QClearBuffers::BufferTypeFlags buffers)
{
    if (buffers != QClearBuffers::None) {
        GLbitfield mask = 0;

        if (buffers & QClearBuffers::ColorBuffer)
            mask |= GL_COLOR_BUFFER_BIT;
        if (buffers & QClearBuffers::DepthBuffer)
            mask |= GL_DEPTH_BUFFER_BIT;
        if (buffers & QClearBuffers::StencilBuffer)
            mask |= GL_STENCIL_BUFFER_BIT;

        m_gl->functions()->glClear(mask);
    }
}

void GraphicsContext::endDrawing(bool swapBuffers)
{
    if (swapBuffers)
        m_gl->swapBuffers(m_surface);
    if (m_ownCurrent)
        m_gl->doneCurrent();
    decayTextureScores();
}

QSize GraphicsContext::renderTargetSize(const QSize &surfaceSize) const
{
    QSize renderTargetSize;
    if (m_activeFBO != m_defaultFBO) {
        // For external FBOs we may not have a m_renderTargets entry.
        if (m_renderTargetsSize.contains(m_activeFBO)) {
            renderTargetSize = m_renderTargetsSize[m_activeFBO];
        } else if (surfaceSize.isValid()) {
            renderTargetSize = surfaceSize;
        } else {
            // External FBO (when used with QtQuick2 Scene3D)

            // Query FBO color attachment 0 size
            GLint attachmentObjectType = GL_NONE;
            GLint attachment0Name = 0;
            m_gl->functions()->glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                                                     GL_COLOR_ATTACHMENT0,
                                                                     GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                                     &attachmentObjectType);
            m_gl->functions()->glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                                                     GL_COLOR_ATTACHMENT0,
                                                                     GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                                     &attachment0Name);

            if (attachmentObjectType == GL_RENDERBUFFER && m_glHelper->supportsFeature(GraphicsHelperInterface::RenderBufferDimensionRetrieval))
                renderTargetSize = m_glHelper->getRenderBufferDimensions(attachment0Name);
            else if (attachmentObjectType == GL_TEXTURE && m_glHelper->supportsFeature(GraphicsHelperInterface::TextureDimensionRetrieval))
                // Assumes texture level 0 and GL_TEXTURE_2D target
                renderTargetSize = m_glHelper->getTextureDimensions(attachment0Name, GL_TEXTURE_2D);
            else
                return renderTargetSize;
        }
    } else {
        renderTargetSize = m_surface->size();
        if (m_surface->surfaceClass() == QSurface::Window) {
            int dpr = static_cast<QWindow *>(m_surface)->devicePixelRatio();
            renderTargetSize *= dpr;
        }
    }
    return renderTargetSize;
}

void GraphicsContext::setViewport(const QRectF &viewport, const QSize &surfaceSize)
{
    m_viewport = viewport;
    QSize size = renderTargetSize(surfaceSize);

    // Check that the returned size is before calling glViewport
    if (size.isEmpty())
        return;

    // Qt3D 0------------------> 1  OpenGL  1^
    //      |                                |
    //      |                                |
    //      |                                |
    //      V                                |
    //      1                                0---------------------> 1
    // The Viewport is defined between 0 and 1 which allows us to automatically
    // scale to the size of the provided window surface
    m_gl->functions()->glViewport(m_viewport.x() * size.width(),
                                  (1.0 - m_viewport.y() - m_viewport.height()) * size.height(),
                                  m_viewport.width() * size.width(),
                                  m_viewport.height() * size.height());
}

void GraphicsContext::releaseOpenGL()
{
    m_shaderCache.clear();
    m_renderBufferHash.clear();

    // Stop and destroy the OpenGL logger
    if (m_debugLogger) {
        m_debugLogger->stopLogging();
        m_debugLogger.reset(nullptr);
    }
}

// The OpenGLContext is not current on any surface at this point
void GraphicsContext::setOpenGLContext(QOpenGLContext* ctx)
{
    Q_ASSERT(ctx);

    releaseOpenGL();
    m_gl = ctx;
}

void GraphicsContext::activateGLHelper()
{
    // Sets the correct GL Helper depending on the surface
    // If no helper exists, create one
    m_glHelper = m_glHelpers.value(m_surface);
    if (!m_glHelper) {
        m_glHelper = resolveHighestOpenGLFunctions();
        m_glHelpers.insert(m_surface, m_glHelper);
        // Note: OpenGLContext is current at this point
        m_gl->functions()->glDisable(GL_DITHER);
    }
}

bool GraphicsContext::hasValidGLHelper() const
{
    return m_glHelper != nullptr;
}

bool GraphicsContext::isInitialized() const
{
    return m_initialized;
}

bool GraphicsContext::makeCurrent(QSurface *surface)
{
    Q_ASSERT(m_gl);
    if (!m_gl->makeCurrent(surface)) {
        qCWarning(Backend) << Q_FUNC_INFO << "makeCurrent failed";
        return false;
    }

    // Set the correct GL Helper depending on the surface
    // If no helper exists, create one

    m_glHelper = m_glHelpers.value(surface);
    if (!m_glHelper) {
        m_glHelper = resolveHighestOpenGLFunctions();
        m_glHelpers.insert(surface, m_glHelper);
    }
    return true;
}

void GraphicsContext::doneCurrent()
{
    Q_ASSERT(m_gl);
    m_gl->doneCurrent();
}

QOpenGLShaderProgram *GraphicsContext::createShaderProgram(Shader *shaderNode)
{
    QScopedPointer<QOpenGLShaderProgram> shaderProgram(new QOpenGLShaderProgram);

    // Compile shaders
    const auto shaderCode = shaderNode->shaderCode();
    for (int i = QShaderProgram::Vertex; i <= QShaderProgram::Compute; ++i) {
        QShaderProgram::ShaderType type = static_cast<const QShaderProgram::ShaderType>(i);
        if (!shaderCode.at(i).isEmpty() &&
                !shaderProgram->addShaderFromSourceCode(shaderType(type), shaderCode.at(i))) {
            qWarning().noquote() << "Failed to compile shader:" << shaderProgram->log();
        }
    }

    // Call glBindFragDataLocation and link the program
    // Since we are sharing shaders in the backend, we assume that if using custom
    // fragOutputs, they should all be the same for a given shader
    bindFragOutputs(shaderProgram->programId(), shaderNode->fragOutputs());
    if (!shaderProgram->link()) {
        qWarning().noquote() << "Failed to link shader program:" << shaderProgram->log();
        return nullptr;
    }

    // take from scoped-pointer so it doesn't get deleted
    return shaderProgram.take();
}

// That assumes that the shaderProgram in Shader stays the same
void GraphicsContext::loadShader(Shader *shader)
{
    QOpenGLShaderProgram *shaderProgram = m_shaderCache.getShaderProgramAndAddRef(shader->dna(), shader->peerId());
    if (!shaderProgram) {
        // No matching QOpenGLShader in the cache so create one
        shaderProgram = createShaderProgram(shader);

        // Store in cache
        m_shaderCache.insert(shader->dna(), shader->peerId(), shaderProgram);
    }

    // Ensure the Shader node knows about the program interface
    // TODO: Improve this so we only introspect once per actual OpenGL shader program
    //       rather than once per ShaderNode. Could cache the interface description along
    //       with the QOpenGLShaderProgram in the ShaderCache.
    if (!shader->isLoaded()) {
        // Introspect and set up interface description on Shader backend node
        shader->initializeUniforms(m_glHelper->programUniformsAndLocations(shaderProgram->programId()));
        shader->initializeAttributes(m_glHelper->programAttributesAndLocations(shaderProgram->programId()));
        if (m_glHelper->supportsFeature(GraphicsHelperInterface::UniformBufferObject))
            shader->initializeUniformBlocks(m_glHelper->programUniformBlocks(shaderProgram->programId()));
        if (m_glHelper->supportsFeature(GraphicsHelperInterface::ShaderStorageObject))
            shader->initializeShaderStorageBlocks(m_glHelper->programShaderStorageBlocks(shaderProgram->programId()));

        shader->setGraphicsContext(this);
        shader->setLoaded(true);
    }
}

// Called only from RenderThread
void GraphicsContext::activateShader(ProgramDNA shaderDNA)
{
    if (shaderDNA != m_activeShaderDNA) {
        m_activeShader = m_shaderCache.getShaderProgramForDNA(shaderDNA);
        if (Q_LIKELY(m_activeShader != nullptr)) {
            m_activeShader->bind();
            m_activeShaderDNA = shaderDNA;
        } else {
            m_glHelper->useProgram(0);
            qWarning() << "No shader program found for DNA";
            m_activeShaderDNA = 0;
        }
        // Ensure material uniforms are re-applied
        m_material = nullptr;
    }
}

void GraphicsContext::removeShaderProgramReference(Shader *shaderNode)
{
    m_shaderCache.removeRef(shaderNode->dna(), shaderNode->peerId());
}

void GraphicsContext::activateRenderTarget(Qt3DCore::QNodeId renderTargetNodeId, const AttachmentPack &attachments, GLuint defaultFboId)
{
    GLuint fboId = defaultFboId; // Default FBO
    if (renderTargetNodeId) {
        // New RenderTarget
        if (!m_renderTargets.contains(renderTargetNodeId)) {
            if (m_defaultFBO && fboId == m_defaultFBO) {
                // this is the default fbo that some platforms create (iOS), we just register it
                // Insert FBO into hash
                m_renderTargets.insert(renderTargetNodeId, fboId);
            } else if ((fboId = m_glHelper->createFrameBufferObject()) != 0) {
                // The FBO is created and its attachments are set once
                // Insert FBO into hash
                m_renderTargets.insert(renderTargetNodeId, fboId);
                // Bind FBO
                m_glHelper->bindFrameBufferObject(fboId);
                bindFrameBufferAttachmentHelper(fboId, attachments);
            } else {
                qCritical() << "Failed to create FBO";
            }
        } else {
            fboId = m_renderTargets.value(renderTargetNodeId);

            // We need to check if  one of the attachment was resized
            bool needsResize = !m_renderTargetsSize.contains(fboId);    // not even initialized yet?
            if (!needsResize) {
                // render target exists, has attachment been resized?
                GLTextureManager *glTextureManager = m_renderer->nodeManagers()->glTextureManager();
                const QSize s = m_renderTargetsSize[fboId];
                const auto attachments_ = attachments.attachments();
                for (const Attachment &attachment : attachments_) {
                    GLTexture *rTex = glTextureManager->lookupResource(attachment.m_textureUuid);
                    needsResize |= (rTex != nullptr && rTex->size() != s);
                    if (attachment.m_point == QRenderTargetOutput::Color0)
                        m_renderTargetFormat = rTex->properties().format;
                }
            }

            if (needsResize) {
                m_glHelper->bindFrameBufferObject(fboId);
                bindFrameBufferAttachmentHelper(fboId, attachments);
            }
        }
    }
    m_activeFBO = fboId;
    m_glHelper->bindFrameBufferObject(m_activeFBO);
    // Set active drawBuffers
    activateDrawBuffers(attachments);
}

void GraphicsContext::bindFrameBufferAttachmentHelper(GLuint fboId, const AttachmentPack &attachments)
{
    // Set FBO attachments

    QSize fboSize;
    GLTextureManager *glTextureManager = m_renderer->nodeManagers()->glTextureManager();
    const auto attachments_ = attachments.attachments();
    for (const Attachment &attachment : attachments_) {
        GLTexture *rTex = glTextureManager->lookupResource(attachment.m_textureUuid);
        QOpenGLTexture *glTex = rTex ? rTex->getOrCreateGLTexture() : nullptr;
        if (glTex != nullptr) {
            if (fboSize.isEmpty())
                fboSize = QSize(glTex->width(), glTex->height());
            else
                fboSize = QSize(qMin(fboSize.width(), glTex->width()), qMin(fboSize.height(), glTex->height()));
            m_glHelper->bindFrameBufferAttachment(glTex, attachment);
        }
    }
    m_renderTargetsSize.insert(fboId, fboSize);
}

void GraphicsContext::activateDrawBuffers(const AttachmentPack &attachments)
{
    const QVector<int> activeDrawBuffers = attachments.getGlDrawBuffers();

    if (m_glHelper->checkFrameBufferComplete()) {
        if (activeDrawBuffers.size() > 1) {// We need MRT
            if (m_glHelper->supportsFeature(GraphicsHelperInterface::MRT)) {
                // Set up MRT, glDrawBuffers...
                m_glHelper->drawBuffers(activeDrawBuffers.size(), activeDrawBuffers.data());
            }
        }
    } else {
        qWarning() << "FBO incomplete";
    }
}


void GraphicsContext::setActiveMaterial(Material *rmat)
{
    if (m_material == rmat)
        return;

    deactivateTexturesWithScope(TextureScopeMaterial);
    m_material = rmat;
}

int GraphicsContext::activateTexture(TextureScope scope, GLTexture *tex, int onUnit)
{
    // Returns the texture unit to use for the texture
    // This always return a valid unit, unless there are more textures than
    // texture unit available for the current material
    onUnit = assignUnitForTexture(tex);

    // check we didn't overflow the available units
    if (onUnit == -1)
        return -1;

    // actually re-bind if required, the tex->dna on the unit not being the same
    // Note: tex->dna() could be 0 if the texture has not been created yet
    if (m_activeTextures[onUnit].texture != tex) {
        QOpenGLTexture *glTex = tex->getOrCreateGLTexture();
        glTex->bind(onUnit);
        m_activeTextures[onUnit].texture = tex;
    }

#if defined(QT3D_RENDER_ASPECT_OPENGL_DEBUG)
    int err = m_gl->functions()->glGetError();
    if (err)
        qCWarning(Backend) << "GL error after activating texture" << QString::number(err, 16)
                           << tex->textureId() << "on unit" << onUnit;
#endif

    m_activeTextures[onUnit].score = 200;
    m_activeTextures[onUnit].pinned = true;
    m_activeTextures[onUnit].scope = scope;

    return onUnit;
}

void GraphicsContext::deactivateTexturesWithScope(TextureScope ts)
{
    for (int u=0; u<m_activeTextures.size(); ++u) {
        if (!m_activeTextures[u].pinned)
            continue; // inactive, ignore

        if (m_activeTextures[u].scope == ts) {
            m_activeTextures[u].pinned = false;
            m_activeTextures[u].score = qMax(m_activeTextures[u].score, 1) - 1;
        }
    } // of units iteration
}

/*!
 * Finds the highest supported opengl version and internally use the most optimized
 * helper for a given version.
 */
GraphicsHelperInterface *GraphicsContext::resolveHighestOpenGLFunctions()
{
    Q_ASSERT(m_gl);
    GraphicsHelperInterface *glHelper = nullptr;

    if (m_gl->isOpenGLES()) {
        if (m_gl->format().majorVersion() >= 3)
            glHelper = new GraphicsHelperES3();
        else
            glHelper = new GraphicsHelperES2();
        glHelper->initializeHelper(m_gl, nullptr);
        qCDebug(Backend) << Q_FUNC_INFO << " Building OpenGL 2/ES2 Helper";
    }
#ifndef QT_OPENGL_ES_2
    else {
        QAbstractOpenGLFunctions *glFunctions = nullptr;
        if ((glFunctions = m_gl->versionFunctions<QOpenGLFunctions_4_3_Core>()) != nullptr) {
            qCDebug(Backend) << Q_FUNC_INFO << " Building OpenGL 4.3";
            glHelper = new GraphicsHelperGL4();
        } else if ((glFunctions = m_gl->versionFunctions<QOpenGLFunctions_3_3_Core>()) != nullptr) {
            qCDebug(Backend) << Q_FUNC_INFO << " Building OpenGL 3.3";
            glHelper = new GraphicsHelperGL3_3();
        } else if ((glFunctions = m_gl->versionFunctions<QOpenGLFunctions_3_2_Core>()) != nullptr) {
            qCDebug(Backend) << Q_FUNC_INFO << " Building OpenGL 3.2";
            glHelper = new GraphicsHelperGL3_2();
        } else if ((glFunctions = m_gl->versionFunctions<QOpenGLFunctions_2_0>()) != nullptr) {
            qCDebug(Backend) << Q_FUNC_INFO << " Building OpenGL 2 Helper";
            glHelper = new GraphicsHelperGL2();
        }
        Q_ASSERT_X(glHelper, "GraphicsContext::resolveHighestOpenGLFunctions", "unable to create valid helper for available OpenGL version");
        glHelper->initializeHelper(m_gl, glFunctions);
    }
#endif

    // Note: at this point we are certain the context (m_gl) is current with a surface
    const QByteArray debugLoggingMode = qgetenv("QT3DRENDER_DEBUG_LOGGING");
    const bool enableDebugLogging = !debugLoggingMode.isEmpty();

    if (enableDebugLogging && !m_debugLogger) {
        if (m_gl->hasExtension("GL_KHR_debug")) {
            qCDebug(Backend) << "Qt3D: Enabling OpenGL debug logging";
            m_debugLogger.reset(new QOpenGLDebugLogger);
            if (m_debugLogger->initialize()) {
                QObject::connect(m_debugLogger.data(), &QOpenGLDebugLogger::messageLogged, &logOpenGLDebugMessage);
                const QString mode = QString::fromLocal8Bit(debugLoggingMode);
                m_debugLogger->startLogging(mode.toLower().startsWith(QLatin1String("sync"))
                                            ? QOpenGLDebugLogger::SynchronousLogging
                                            : QOpenGLDebugLogger::AsynchronousLogging);

                const auto msgs = m_debugLogger->loggedMessages();
                for (const QOpenGLDebugMessage &msg : msgs)
                    logOpenGLDebugMessage(msg);
            }
        } else {
            qCDebug(Backend) << "Qt3D: OpenGL debug logging requested but GL_KHR_debug not supported";
        }
    }


    // Set Vendor and Extensions of reference GraphicsApiFilter
    // TO DO: would that vary like the glHelper ?

    QStringList extensions;
    const auto exts = m_gl->extensions();
    for (const QByteArray &ext : exts)
        extensions << QString::fromUtf8(ext);
    m_contextInfo.m_major = m_gl->format().version().first;
    m_contextInfo.m_minor = m_gl->format().version().second;
    m_contextInfo.m_api = m_gl->isOpenGLES() ? QGraphicsApiFilter::OpenGLES : QGraphicsApiFilter::OpenGL;
    m_contextInfo.m_profile = static_cast<QGraphicsApiFilter::OpenGLProfile>(m_gl->format().profile());
    m_contextInfo.m_extensions = extensions;
    m_contextInfo.m_vendor = QString::fromUtf8(reinterpret_cast<const char *>(m_gl->functions()->glGetString(GL_VENDOR)));

    return glHelper;
}

void GraphicsContext::deactivateTexture(GLTexture* tex)
{
    for (int u=0; u<m_activeTextures.size(); ++u) {
        if (m_activeTextures[u].texture == tex) {
            Q_ASSERT(m_activeTextures[u].pinned);
            m_activeTextures[u].pinned = false;
            return;
        }
    } // of units iteration

    qCWarning(Backend) << Q_FUNC_INFO << "texture not active:" << tex;
}

void GraphicsContext::setCurrentStateSet(RenderStateSet *ss)
{
    if (ss == m_stateSet)
        return;

    if (ss)
        ss->apply(this);
    m_stateSet = ss;
}

RenderStateSet *GraphicsContext::currentStateSet() const
{
    return m_stateSet;
}

const GraphicsApiFilterData *GraphicsContext::contextInfo() const
{
    return &m_contextInfo;
}

bool GraphicsContext::supportsDrawBuffersBlend() const
{
    return m_glHelper->supportsFeature(GraphicsHelperInterface::DrawBuffersBlend);
}

/*!
 * Wraps an OpenGL call to glDrawElementsInstanced.
 * If the call is not supported by the system's OpenGL version,
 * it is simulated with a loop.
 */
void GraphicsContext::drawElementsInstancedBaseVertexBaseInstance(GLenum primitiveType,
                                                                  GLsizei primitiveCount,
                                                                  GLint indexType,
                                                                  void *indices,
                                                                  GLsizei instances,
                                                                  GLint baseVertex,
                                                                  GLint baseInstance)
{
    m_glHelper->drawElementsInstancedBaseVertexBaseInstance(primitiveType,
                                                            primitiveCount,
                                                            indexType,
                                                            indices,
                                                            instances,
                                                            baseVertex,
                                                            baseInstance);
}

/*!
 * Wraps an OpenGL call to glDrawArraysInstanced.
 */
void GraphicsContext::drawArraysInstanced(GLenum primitiveType,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei instances)
{
    m_glHelper->drawArraysInstanced(primitiveType,
                                    first,
                                    count,
                                    instances);
}

void GraphicsContext::drawArraysInstancedBaseInstance(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances, GLsizei baseinstance)
{
    m_glHelper->drawArraysInstancedBaseInstance(primitiveType,
                                                first,
                                                count,
                                                instances,
                                                baseinstance);
}

/*!
 * Wraps an OpenGL call to glDrawElements.
 */
void GraphicsContext::drawElements(GLenum primitiveType,
                                   GLsizei primitiveCount,
                                   GLint indexType,
                                   void *indices,
                                   GLint baseVertex)
{
    m_glHelper->drawElements(primitiveType,
                             primitiveCount,
                             indexType,
                             indices,
                             baseVertex);
}

/*!
 * Wraps an OpenGL call to glDrawArrays.
 */
void GraphicsContext::drawArrays(GLenum primitiveType,
                                 GLint first,
                                 GLsizei count)
{
    m_glHelper->drawArrays(primitiveType,
                           first,
                           count);
}

void GraphicsContext::setVerticesPerPatch(GLint verticesPerPatch)
{
    m_glHelper->setVerticesPerPatch(verticesPerPatch);
}

void GraphicsContext::blendEquation(GLenum mode)
{
    m_glHelper->blendEquation(mode);
}

void GraphicsContext::blendFunci(GLuint buf, GLenum sfactor, GLenum dfactor)
{
    m_glHelper->blendFunci(buf, sfactor, dfactor);
}

void GraphicsContext::blendFuncSeparatei(GLuint buf, GLenum sRGB, GLenum dRGB, GLenum sAlpha, GLenum dAlpha)
{
    m_glHelper->blendFuncSeparatei(buf, sRGB, dRGB, sAlpha, dAlpha);
}

void GraphicsContext::alphaTest(GLenum mode1, GLenum mode2)
{
    m_glHelper->alphaTest(mode1, mode2);
}

void GraphicsContext::bindFramebuffer(GLuint fbo)
{
    m_glHelper->bindFrameBufferObject(fbo);
}

void GraphicsContext::depthTest(GLenum mode)
{
    m_glHelper->depthTest(mode);
}

void GraphicsContext::depthMask(GLenum mode)
{
    m_glHelper->depthMask(mode);
}

void GraphicsContext::frontFace(GLenum mode)
{
    m_glHelper->frontFace(mode);
}

void GraphicsContext::bindFragOutputs(GLuint shader, const QHash<QString, int> &outputs)
{
    if (m_glHelper->supportsFeature(GraphicsHelperInterface::MRT) &&
            m_glHelper->supportsFeature(GraphicsHelperInterface::BindableFragmentOutputs))
        m_glHelper->bindFragDataLocation(shader, outputs);
}

void GraphicsContext::bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    m_glHelper->bindUniformBlock(programId, uniformBlockIndex, uniformBlockBinding);
}

void GraphicsContext::bindShaderStorageBlock(GLuint programId, GLuint shaderStorageBlockIndex, GLuint shaderStorageBlockBinding)
{
    m_glHelper->bindShaderStorageBlock(programId, shaderStorageBlockIndex, shaderStorageBlockBinding);
}

void GraphicsContext::bindBufferBase(GLenum target, GLuint bindingIndex, GLuint buffer)
{
    m_glHelper->bindBufferBase(target, bindingIndex, buffer);
}

void GraphicsContext::buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer)
{
    m_glHelper->buildUniformBuffer(v, description, buffer);
}

void GraphicsContext::setMSAAEnabled(bool enabled)
{
    m_glHelper->setMSAAEnabled(enabled);
}

void GraphicsContext::setAlphaCoverageEnabled(bool enabled)
{
    m_glHelper->setAlphaCoverageEnabled(enabled);
}

void GraphicsContext::clearBufferf(GLint drawbuffer, const QVector4D &values)
{
    m_glHelper->clearBufferf(drawbuffer, values);
}

GLuint GraphicsContext::boundFrameBufferObject()
{
    return m_glHelper->boundFrameBufferObject();
}

void GraphicsContext::clearColor(const QColor &color)
{
    if (m_currClearColorValue != color) {
        m_currClearColorValue = color;
        m_gl->functions()->glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }
}

void GraphicsContext::clearDepthValue(float depth)
{
    if (m_currClearDepthValue != depth) {
        m_currClearDepthValue = depth;
        m_gl->functions()->glClearDepthf(depth);
    }
}

void GraphicsContext::clearStencilValue(int stencil)
{
    if (m_currClearStencilValue != stencil) {
        m_currClearStencilValue = stencil;
        m_gl->functions()->glClearStencil(stencil);
    }
}

void GraphicsContext::enableClipPlane(int clipPlane)
{
    m_glHelper->enableClipPlane(clipPlane);
}

void GraphicsContext::disableClipPlane(int clipPlane)
{
    m_glHelper->disableClipPlane(clipPlane);
}

void GraphicsContext::setClipPlane(int clipPlane, const QVector3D &normal, float distance)
{
    m_glHelper->setClipPlane(clipPlane, normal, distance);
}

GLint GraphicsContext::maxClipPlaneCount()
{
    return m_glHelper->maxClipPlaneCount();
}

void GraphicsContext::enablePrimitiveRestart(int restartIndex)
{
    if (m_glHelper->supportsFeature(GraphicsHelperInterface::PrimitiveRestart))
        m_glHelper->enablePrimitiveRestart(restartIndex);
}

void GraphicsContext::disablePrimitiveRestart()
{
    if (m_glHelper->supportsFeature(GraphicsHelperInterface::PrimitiveRestart))
        m_glHelper->disablePrimitiveRestart();
}

void GraphicsContext::pointSize(bool programmable, GLfloat value)
{
    m_glHelper->pointSize(programmable, value);
}

void GraphicsContext::dispatchCompute(int x, int y, int z)
{
    if (m_glHelper->supportsFeature(GraphicsHelperInterface::Compute))
        m_glHelper->dispatchCompute(x, y, z);
}

void GraphicsContext::enablei(GLenum cap, GLuint index)
{
    m_glHelper->enablei(cap, index);
}

void GraphicsContext::disablei(GLenum cap, GLuint index)
{
    m_glHelper->disablei(cap, index);
}

void GraphicsContext::setSeamlessCubemap(bool enable)
{
    m_glHelper->setSeamlessCubemap(enable);
}

/*!
    \internal
    Returns a texture unit for a texture, -1 if all texture units are assigned.
    Tries to use the texture unit with the texture that hasn't been used for the longest time
    if the texture happens not to be already pinned on a texture unit.
 */
GLint GraphicsContext::assignUnitForTexture(GLTexture *tex)
{
    int lowestScoredUnit = -1;
    int lowestScore = 0xfffffff;

    for (int u=0; u<m_activeTextures.size(); ++u) {
        if (m_activeTextures[u].texture == tex)
            return u;

        // No texture is currently active on the texture unit
        // we save the texture unit with the texture that has been on there
        // the longest time while not being used
        if (!m_activeTextures[u].pinned) {
            int score = m_activeTextures[u].score;
            if (score < lowestScore) {
                lowestScore = score;
                lowestScoredUnit = u;
            }
        }
    } // of units iteration

    if (lowestScoredUnit == -1)
        qCWarning(Backend) << Q_FUNC_INFO << "No free texture units!";

    return lowestScoredUnit;
}

void GraphicsContext::decayTextureScores()
{
    for (int u = 0; u < m_activeTextures.size(); u++)
        m_activeTextures[u].score = qMax(m_activeTextures[u].score - 1, 0);
}

QOpenGLShaderProgram* GraphicsContext::activeShader() const
{
    Q_ASSERT(m_activeShader);
    return m_activeShader;
}

void GraphicsContext::setRenderer(Renderer *renderer)
{
    m_renderer = renderer;
}

// It will be easier if the QGraphicContext applies the QUniformPack
// than the other way around
void GraphicsContext::setParameters(ShaderParameterPack &parameterPack)
{
    // Activate textures and update TextureUniform in the pack
    // with the correct textureUnit

    // Set the pinned texture of the previous material texture
    // to pinable so that we should easily find an available texture unit
    NodeManagers *manager = m_renderer->nodeManagers();

    deactivateTexturesWithScope(TextureScopeMaterial);
    // Update the uniforms with the correct texture unit id's
    PackUniformHash &uniformValues = parameterPack.uniforms();

    for (int i = 0; i < parameterPack.textures().size(); ++i) {
        const ShaderParameterPack::NamedTexture &namedTex = parameterPack.textures().at(i);
        // Given a Texture QNodeId, we retrieve the associated shared GLTexture
        if (uniformValues.contains(namedTex.glslNameId)) {
            GLTexture *t = manager->glTextureManager()->lookupResource(namedTex.texId);
            if (t != nullptr) {
                UniformValue &texUniform = uniformValues[namedTex.glslNameId];
                Q_ASSERT(texUniform.valueType() == UniformValue::TextureValue);
                const int texUnit = activateTexture(TextureScopeMaterial, t);
                texUniform.data<UniformValue::Texture>()->textureId = texUnit;
            }
        }
    }

    QOpenGLShaderProgram *shader = activeShader();

    // TO DO: We could cache the binding points somehow and only do the binding when necessary
    // for SSBO and UBO

    // Bind Shader Storage block to SSBO and update SSBO
    const QVector<BlockToSSBO> blockToSSBOs = parameterPack.shaderStorageBuffers();
    int ssboIndex = 0;
    for (const BlockToSSBO b : blockToSSBOs) {
        Buffer *cpuBuffer = m_renderer->nodeManagers()->bufferManager()->lookupResource(b.m_bufferID);
        GLBuffer *ssbo = glBufferForRenderBuffer(cpuBuffer);
        bindShaderStorageBlock(shader->programId(), b.m_blockIndex, ssboIndex);
        // Needed to avoid conflict where the buffer would already
        // be bound as a VertexArray
        bindGLBuffer(ssbo, GLBuffer::ShaderStorageBuffer);
        ssbo->bindBufferBase(this, ssboIndex++, GLBuffer::ShaderStorageBuffer);
        // Perform update if required
        if (cpuBuffer->isDirty()) {
            uploadDataToGLBuffer(cpuBuffer, ssbo);
            cpuBuffer->unsetDirty();
        }
        // TO DO: Make sure that there's enough binding points
    }

    // Bind UniformBlocks to UBO and update UBO from Buffer
    // TO DO: Convert ShaderData to Buffer so that we can use that generic process
    const QVector<BlockToUBO> blockToUBOs = parameterPack.uniformBuffers();
    int uboIndex = 0;
    for (const BlockToUBO &b : blockToUBOs) {
        Buffer *cpuBuffer = m_renderer->nodeManagers()->bufferManager()->lookupResource(b.m_bufferID);
        GLBuffer *ubo = glBufferForRenderBuffer(cpuBuffer);
        bindUniformBlock(shader->programId(), b.m_blockIndex, uboIndex);
        // Needed to avoid conflict where the buffer would already
        // be bound as a VertexArray
        bindGLBuffer(ubo, GLBuffer::UniformBuffer);
        ubo->bindBufferBase(this, uboIndex++, GLBuffer::UniformBuffer);
        if (cpuBuffer->isDirty()) {
            // Perform update if required
            uploadDataToGLBuffer(cpuBuffer, ubo);
            cpuBuffer->unsetDirty();
        }
        // TO DO: Make sure that there's enough binding points
    }

    // Update uniforms in the Default Uniform Block
    const PackUniformHash values = parameterPack.uniforms();
    const QVector<ShaderUniform> activeUniforms = parameterPack.submissionUniforms();

    for (const ShaderUniform &uniform : activeUniforms) {
        // We can use [] as we are sure the the uniform wouldn't
        // be un activeUniforms if there wasn't a matching value
        applyUniform(uniform, values[uniform.m_nameId]);
    }
}

void GraphicsContext::enableAttribute(const VAOVertexAttribute &attr)
{
    // Bind buffer within the current VAO
    GLBuffer *buf = m_renderer->nodeManagers()->glBufferManager()->data(attr.bufferHandle);
    Q_ASSERT(buf);
    bindGLBuffer(buf, attr.bufferType);

    QOpenGLShaderProgram *prog = activeShader();
    prog->enableAttributeArray(attr.location);
    prog->setAttributeBuffer(attr.location,
                             attr.dataType,
                             attr.byteOffset,
                             attr.vertexSize,
                             attr.byteStride);

    // Done by the helper if it supports it
    if (attr.divisor != 0)
        m_glHelper->vertexAttribDivisor(attr.location, attr.divisor);
}

void GraphicsContext::disableAttribute(const GraphicsContext::VAOVertexAttribute &attr)
{
    QOpenGLShaderProgram *prog = activeShader();
    prog->disableAttributeArray(attr.location);
}

void GraphicsContext::applyUniform(const ShaderUniform &description, const UniformValue &v)
{
    const UniformType type = m_glHelper->uniformTypeFromGLType(description.m_type);

    switch (type) {
    case UniformType::Float:
        // See QTBUG-57510 and uniform_p.h
        if (v.storedType() == Int) {
            float value = float(*v.constData<int>());
            UniformValue floatV(value);
            applyUniformHelper<UniformType::Float>(description.m_location, description.m_size, floatV);
        } else {
            applyUniformHelper<UniformType::Float>(description.m_location, description.m_size, v);
        }
        break;
    case UniformType::Vec2:
        applyUniformHelper<UniformType::Vec2>(description.m_location, description.m_size, v);
        break;
    case UniformType::Vec3:
        applyUniformHelper<UniformType::Vec3>(description.m_location, description.m_size, v);
        break;
    case UniformType::Vec4:
        applyUniformHelper<UniformType::Vec4>(description.m_location, description.m_size, v);
        break;

    case UniformType::Double:
        applyUniformHelper<UniformType::Double>(description.m_location, description.m_size, v);
        break;
    case UniformType::DVec2:
        applyUniformHelper<UniformType::DVec2>(description.m_location, description.m_size, v);
        break;
    case UniformType::DVec3:
        applyUniformHelper<UniformType::DVec3>(description.m_location, description.m_size, v);
        break;
    case UniformType::DVec4:
        applyUniformHelper<UniformType::DVec4>(description.m_location, description.m_size, v);
        break;

    case UniformType::Sampler:
    case UniformType::Int:
        applyUniformHelper<UniformType::Int>(description.m_location, description.m_size, v);
        break;
    case UniformType::IVec2:
        applyUniformHelper<UniformType::IVec2>(description.m_location, description.m_size, v);
        break;
    case UniformType::IVec3:
        applyUniformHelper<UniformType::IVec3>(description.m_location, description.m_size, v);
        break;
    case UniformType::IVec4:
        applyUniformHelper<UniformType::IVec4>(description.m_location, description.m_size, v);
        break;

    case UniformType::UInt:
        applyUniformHelper<UniformType::Int>(description.m_location, description.m_size, v);
        break;
    case UniformType::UIVec2:
        applyUniformHelper<UniformType::IVec2>(description.m_location, description.m_size, v);
        break;
    case UniformType::UIVec3:
        applyUniformHelper<UniformType::IVec3>(description.m_location, description.m_size, v);
        break;
    case UniformType::UIVec4:
        applyUniformHelper<UniformType::IVec4>(description.m_location, description.m_size, v);
        break;

    case UniformType::Bool:
        applyUniformHelper<UniformType::Bool>(description.m_location, description.m_size, v);
        break;
    case UniformType::BVec2:
        applyUniformHelper<UniformType::BVec2>(description.m_location, description.m_size, v);
        break;
    case UniformType::BVec3:
        applyUniformHelper<UniformType::BVec3>(description.m_location, description.m_size, v);
        break;
    case UniformType::BVec4:
        applyUniformHelper<UniformType::BVec4>(description.m_location, description.m_size, v);
        break;

    case UniformType::Mat2:
        applyUniformHelper<UniformType::Mat2>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat3:
        applyUniformHelper<UniformType::Mat3>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat4:
        applyUniformHelper<UniformType::Mat4>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat2x3:
        applyUniformHelper<UniformType::Mat2x3>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat3x2:
        applyUniformHelper<UniformType::Mat3x2>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat2x4:
        applyUniformHelper<UniformType::Mat2x4>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat4x2:
        applyUniformHelper<UniformType::Mat4x2>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat3x4:
        applyUniformHelper<UniformType::Mat3x4>(description.m_location, description.m_size, v);
        break;
    case UniformType::Mat4x3:
        applyUniformHelper<UniformType::Mat4x3>(description.m_location, description.m_size, v);
        break;

    default:
        break;
    }
}

// Note: needs to be called while VAO is bound
void GraphicsContext::specifyAttribute(const Attribute *attribute, Buffer *buffer, int location)
{
    if (location < 0) {
        qCWarning(Backend) << "failed to resolve location for attribute:" << attribute->name();
        return;
    }

    const GLint attributeDataType = glDataTypeFromAttributeDataType(attribute->vertexBaseType());
    const HGLBuffer glBufferHandle = m_renderer->nodeManagers()->glBufferManager()->lookupHandle(buffer->peerId());
    const GLBuffer::Type bufferType = bufferTypeToGLBufferType(buffer->type());

    int typeSize = 0;
    int attrCount = 0;

    if (attribute->vertexSize() >= 1 && attribute->vertexSize() <= 4) {
        attrCount = 1;
    } else if (attribute->vertexSize() == 9) {
        typeSize = byteSizeFromType(attributeDataType);
        attrCount = 3;
    } else if (attribute->vertexSize() == 16) {
        typeSize = byteSizeFromType(attributeDataType);
        attrCount = 4;
    } else {
        Q_UNREACHABLE();
    }

    for (int i = 0; i < attrCount; i++) {
        VAOVertexAttribute attr;
        attr.bufferHandle = glBufferHandle;
        attr.bufferType = bufferType;
        attr.location = location + i;
        attr.dataType = attributeDataType;
        attr.byteOffset = attribute->byteOffset() + (i * attrCount * typeSize);
        attr.vertexSize = attribute->vertexSize() / attrCount;
        attr.byteStride = attribute->byteStride() + (attrCount * attrCount * typeSize);
        attr.divisor = attribute->divisor();

        enableAttribute(attr);

        // Save this in the current emulated VAO
        if (m_currentVAO)
            m_currentVAO->saveVertexAttribute(attr);
    }
}

void GraphicsContext::specifyIndices(Buffer *buffer)
{
    Q_ASSERT(buffer->type() == QBuffer::IndexBuffer);

    GLBuffer *buf = glBufferForRenderBuffer(buffer);
    if (!bindGLBuffer(buf, GLBuffer::IndexBuffer))
        qCWarning(Backend) << Q_FUNC_INFO << "binding index buffer failed";

    // bound within the current VAO
    // Save this in the current emulated VAO
    if (m_currentVAO)
        m_currentVAO->saveIndexAttribute(m_renderer->nodeManagers()->glBufferManager()->lookupHandle(buffer->peerId()));
}

void GraphicsContext::updateBuffer(Buffer *buffer)
{
    const QHash<Qt3DCore::QNodeId, HGLBuffer>::iterator it = m_renderBufferHash.find(buffer->peerId());
    if (it != m_renderBufferHash.end())
        uploadDataToGLBuffer(buffer, m_renderer->nodeManagers()->glBufferManager()->data(it.value()));
}

void GraphicsContext::releaseBuffer(Qt3DCore::QNodeId bufferId)
{
    auto it = m_renderBufferHash.find(bufferId);
    if (it != m_renderBufferHash.end()) {
        HGLBuffer glBuffHandle = it.value();
        GLBuffer *glBuff = m_renderer->nodeManagers()->glBufferManager()->data(glBuffHandle);

        Q_ASSERT(glBuff);
        // Destroy the GPU resource
        glBuff->destroy(this);
        // Destroy the GLBuffer instance
        m_renderer->nodeManagers()->glBufferManager()->releaseResource(bufferId);
        // Remove Id - HGLBuffer entry
        m_renderBufferHash.erase(it);
    }
}

bool GraphicsContext::hasGLBufferForBuffer(Buffer *buffer)
{
    const QHash<Qt3DCore::QNodeId, HGLBuffer>::iterator it = m_renderBufferHash.find(buffer->peerId());
    return (it != m_renderBufferHash.end());
}

GLBuffer *GraphicsContext::glBufferForRenderBuffer(Buffer *buf)
{
    if (!m_renderBufferHash.contains(buf->peerId()))
        m_renderBufferHash.insert(buf->peerId(), createGLBufferFor(buf));
    return m_renderer->nodeManagers()->glBufferManager()->data(m_renderBufferHash.value(buf->peerId()));
}

HGLBuffer GraphicsContext::createGLBufferFor(Buffer *buffer)
{
    GLBuffer *b = m_renderer->nodeManagers()->glBufferManager()->getOrCreateResource(buffer->peerId());
    //    b.setUsagePattern(static_cast<QOpenGLBuffer::UsagePattern>(buffer->usage()));
    Q_ASSERT(b);
    if (!b->create(this))
        qCWarning(Render::Io) << Q_FUNC_INFO << "buffer creation failed";

    if (!bindGLBuffer(b, bufferTypeToGLBufferType(buffer->type())))
        qCWarning(Render::Io) << Q_FUNC_INFO << "buffer binding failed";

    // TO DO: Handle usage pattern
    b->allocate(this, buffer->data().constData(), buffer->data().size(), false);
    return m_renderer->nodeManagers()->glBufferManager()->lookupHandle(buffer->peerId());
}

bool GraphicsContext::bindGLBuffer(GLBuffer *buffer, GLBuffer::Type type)
{
    if (type == GLBuffer::ArrayBuffer && buffer == m_boundArrayBuffer)
        return true;

    if (buffer->bind(this, type)) {
        if (type == GLBuffer::ArrayBuffer)
            m_boundArrayBuffer = buffer;
        return true;
    }
    return false;
}

void GraphicsContext::uploadDataToGLBuffer(Buffer *buffer, GLBuffer *b, bool releaseBuffer)
{
    if (!bindGLBuffer(b, bufferTypeToGLBufferType(buffer->type())))
        qCWarning(Render::Io) << Q_FUNC_INFO << "buffer bind failed";
    // If the buffer is dirty (hence being called here)
    // there are two possible cases
    // * setData was called changing the whole data or functor (or the usage pattern)
    // * partial buffer updates where received

    // Note: we assume the case where both setData/functor and updates are called to be a misuse
    // with unexpected behavior
    const QVector<Qt3DRender::QBufferUpdate> updates = std::move(buffer->pendingBufferUpdates());
    if (!updates.empty()) {
        for (const Qt3DRender::QBufferUpdate &update : updates) {
            // TO DO: based on the number of updates .., it might make sense to
            // sometime use glMapBuffer rather than glBufferSubData
            b->update(this, update.data.constData(), update.data.size(), update.offset);
        }
    } else {
        const int bufferSize = buffer->data().size();
        // TO DO: Handle usage pattern
        b->allocate(this, bufferSize, false); // orphan the buffer
        b->allocate(this, buffer->data().constData(), bufferSize, false);
    }
    if (releaseBuffer) {
        b->release(this);
        if (bufferTypeToGLBufferType(buffer->type()) == GLBuffer::ArrayBuffer)
            m_boundArrayBuffer = nullptr;
    }
    qCDebug(Render::Io) << "uploaded buffer size=" << buffer->data().size();
}

GLint GraphicsContext::elementType(GLint type)
{
    switch (type) {
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
        return GL_FLOAT;

#ifndef QT_OPENGL_ES_2 // Otherwise compile error as Qt defines GL_DOUBLE as GL_FLOAT when using ES2
    case GL_DOUBLE:
#ifdef GL_DOUBLE_VEC3 // For compiling on pre GL 4.1 systems
    case GL_DOUBLE_VEC2:
    case GL_DOUBLE_VEC3:
    case GL_DOUBLE_VEC4:
#endif
        return GL_DOUBLE;
#endif
    default:
        qWarning() << Q_FUNC_INFO << "unsupported:" << QString::number(type, 16);
    }

    return GL_INVALID_VALUE;
}

GLint GraphicsContext::tupleSizeFromType(GLint type)
{
    switch (type) {
    case GL_FLOAT:
#ifndef QT_OPENGL_ES_2 // Otherwise compile error as Qt defines GL_DOUBLE as GL_FLOAT when using ES2
    case GL_DOUBLE:
#endif
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_INT:
        break; // fall through

    case GL_FLOAT_VEC2:
#ifdef GL_DOUBLE_VEC2 // For compiling on pre GL 4.1 systems.
    case GL_DOUBLE_VEC2:
#endif
        return 2;

    case GL_FLOAT_VEC3:
#ifdef GL_DOUBLE_VEC3 // For compiling on pre GL 4.1 systems.
    case GL_DOUBLE_VEC3:
#endif
        return 3;

    case GL_FLOAT_VEC4:
#ifdef GL_DOUBLE_VEC4 // For compiling on pre GL 4.1 systems.
    case GL_DOUBLE_VEC4:
#endif
        return 4;

    default:
        qWarning() << Q_FUNC_INFO << "unsupported:" << QString::number(type, 16);
    }

    return 1;
}

GLuint GraphicsContext::byteSizeFromType(GLint type)
{
    switch (type) {
    case GL_FLOAT:          return sizeof(float);
#ifndef QT_OPENGL_ES_2 // Otherwise compile error as Qt defines GL_DOUBLE as GL_FLOAT when using ES2
    case GL_DOUBLE:         return sizeof(double);
#endif
    case GL_UNSIGNED_BYTE:  return sizeof(unsigned char);
    case GL_UNSIGNED_INT:   return sizeof(GLuint);

    case GL_FLOAT_VEC2:     return sizeof(float) * 2;
    case GL_FLOAT_VEC3:     return sizeof(float) * 3;
    case GL_FLOAT_VEC4:     return sizeof(float) * 4;
#ifdef GL_DOUBLE_VEC3 // Required to compile on pre GL 4.1 systems
    case GL_DOUBLE_VEC2:    return sizeof(double) * 2;
    case GL_DOUBLE_VEC3:    return sizeof(double) * 3;
    case GL_DOUBLE_VEC4:    return sizeof(double) * 4;
#endif
    default:
        qWarning() << Q_FUNC_INFO << "unsupported:" << QString::number(type, 16);
    }

    return 0;
}

GLint GraphicsContext::glDataTypeFromAttributeDataType(QAttribute::VertexBaseType dataType)
{
    switch (dataType) {
    case QAttribute::Byte:
        return GL_BYTE;
    case QAttribute::UnsignedByte:
        return GL_UNSIGNED_BYTE;
    case QAttribute::Short:
        return GL_SHORT;
    case QAttribute::UnsignedShort:
        return GL_UNSIGNED_SHORT;
    case QAttribute::Int:
        return GL_INT;
    case QAttribute::UnsignedInt:
        return GL_UNSIGNED_INT;
    case QAttribute::HalfFloat:
#ifdef GL_HALF_FLOAT
        return GL_HALF_FLOAT;
#endif
#ifndef QT_OPENGL_ES_2 // Otherwise compile error as Qt defines GL_DOUBLE as GL_FLOAT when using ES2
    case QAttribute::Double:
        return GL_DOUBLE;
#endif
    case QAttribute::Float:
        break;
    default:
        qWarning() << Q_FUNC_INFO << "unsupported dataType:" << dataType;
    }
    return GL_FLOAT;
}

static void copyGLFramebufferDataToImage(QImage &img, const uchar *srcData, uint stride, uint width, uint height, QAbstractTexture::TextureFormat format)
{
    switch (format) {
    case QAbstractTexture::RGBA32F:
        {
            uchar *srcScanline = (uchar *)srcData + stride * (height - 1);
            for (uint i = 0; i < height; ++i) {
                uchar *dstScanline = img.scanLine(i);
                float *pSrc = (float*)srcScanline;
                for (uint j = 0; j < width; j++) {
                    *dstScanline++ = (uchar)(255.0f * (pSrc[4*j+2] / (1.0f + pSrc[4*j+2])));
                    *dstScanline++ = (uchar)(255.0f * (pSrc[4*j+1] / (1.0f + pSrc[4*j+1])));
                    *dstScanline++ = (uchar)(255.0f * (pSrc[4*j+0] / (1.0f + pSrc[4*j+0])));
                    *dstScanline++ = (uchar)(255.0f * qBound(0.0f, pSrc[4*j+3], 1.0f));
                }
                srcScanline -= stride;
            }
        } break;
    default:
        {
            uchar* srcScanline = (uchar *)srcData + stride * (height - 1);
            for (uint i = 0; i < height; ++i) {
                memcpy(img.scanLine(i), srcScanline, stride);
                srcScanline -= stride;
            }
        } break;
    }
}

QImage GraphicsContext::readFramebuffer(QSize size)
{
    QImage img;
    const unsigned int area = size.width() * size.height();
    unsigned int bytes;
    GLenum format, type;
    QImage::Format imageFormat;
    uint stride;

#ifndef QT_OPENGL_ES_2
    /* format value should match GL internalFormat */
    GLenum internalFormat = m_renderTargetFormat;
#endif

    switch (m_renderTargetFormat) {
    case QAbstractTexture::RGBAFormat:
    case QAbstractTexture::RGBA8_SNorm:
    case QAbstractTexture::RGBA8_UNorm:
    case QAbstractTexture::RGBA8U:
    case QAbstractTexture::SRGB8_Alpha8:
#ifdef QT_OPENGL_ES_2
        format = GL_RGBA;
        imageFormat = QImage::Format_RGBA8888_Premultiplied;
#else
        format = GL_BGRA;
        imageFormat = QImage::Format_ARGB32_Premultiplied;
        internalFormat = GL_RGBA8;
#endif
        type = GL_UNSIGNED_BYTE;
        bytes = area * 4;
        stride = size.width() * 4;
        break;
    case QAbstractTexture::SRGB8:
    case QAbstractTexture::RGBFormat:
    case QAbstractTexture::RGB8U:
#ifdef QT_OPENGL_ES_2
        format = GL_RGBA;
        imageFormat = QImage::Format_RGBX8888;
#else
        format = GL_BGRA;
        imageFormat = QImage::Format_RGB32;
        internalFormat = GL_RGB8;
#endif
        type = GL_UNSIGNED_BYTE;
        bytes = area * 4;
        stride = size.width() * 4;
        break;
#ifndef QT_OPENGL_ES_2
    case QAbstractTexture::RG11B10F:
        bytes = area * 4;
        format = GL_RGB;
        type = GL_UNSIGNED_INT_10F_11F_11F_REV;
        imageFormat = QImage::Format_RGB30;
        stride = size.width() * 4;
        break;
    case QAbstractTexture::RGB10A2:
        bytes = area * 4;
        format = GL_RGBA;
        type = GL_UNSIGNED_INT_2_10_10_10_REV;
        imageFormat = QImage::Format_A2BGR30_Premultiplied;
        stride = size.width() * 4;
        break;
    case QAbstractTexture::R5G6B5:
        bytes = area * 2;
        format = GL_RGB;
        type = GL_UNSIGNED_SHORT;
        internalFormat = GL_UNSIGNED_SHORT_5_6_5_REV;
        imageFormat = QImage::Format_RGB16;
        stride = size.width() * 2;
        break;
    case QAbstractTexture::RGBA16F:
    case QAbstractTexture::RGBA16U:
    case QAbstractTexture::RGBA32F:
    case QAbstractTexture::RGBA32U:
        bytes = area * 16;
        format = GL_RGBA;
        type = GL_FLOAT;
        imageFormat = QImage::Format_ARGB32_Premultiplied;
        stride = size.width() * 16;
        break;
#endif
    default:
        // unsupported format
        Q_UNREACHABLE();
        return img;
    }

#ifndef QT_OPENGL_ES_2
    GLint samples = 0;
    m_gl->functions()->glGetIntegerv(GL_SAMPLES, &samples);
    if (samples > 0 && !m_glHelper->supportsFeature(GraphicsHelperInterface::BlitFramebuffer))
        return img;
#endif

    img = QImage(size.width(), size.height(), imageFormat);

    QScopedArrayPointer<uchar> data(new uchar [bytes]);

#ifndef QT_OPENGL_ES_2
    if (samples > 0) {
        // resolve multisample-framebuffer to renderbuffer and read pixels from it
        GLuint fbo, rb;
        QOpenGLFunctions *gl = m_gl->functions();
        gl->glGenFramebuffers(1, &fbo);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        gl->glGenRenderbuffers(1, &rb);
        gl->glBindRenderbuffer(GL_RENDERBUFFER, rb);
        gl->glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, size.width(), size.height());
        gl->glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);

        const GLenum status = gl->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            gl->glDeleteRenderbuffers(1, &rb);
            gl->glDeleteFramebuffers(1, &fbo);
            return img;
        }

        m_glHelper->blitFramebuffer(0, 0, size.width(), size.height(),
                                    0, 0, size.width(), size.height(),
                                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        gl->glReadPixels(0,0,size.width(), size.height(), format, type, data.data());

        copyGLFramebufferDataToImage(img, data.data(), stride, size.width(), size.height(), m_renderTargetFormat);

        gl->glBindRenderbuffer(GL_RENDERBUFFER, rb);
        gl->glDeleteRenderbuffers(1, &rb);
        gl->glBindFramebuffer(GL_FRAMEBUFFER, m_activeFBO);
        gl->glDeleteFramebuffers(1, &fbo);
    } else {
#endif
        // read pixels directly from framebuffer
        m_gl->functions()->glReadPixels(0,0,size.width(), size.height(), format, type, data.data());
        copyGLFramebufferDataToImage(img, data.data(), stride, size.width(), size.height(), m_renderTargetFormat);

#ifndef QT_OPENGL_ES_2
    }
#endif

    return img;
}

QT3D_UNIFORM_TYPE_IMPL(UniformType::Float, float, glUniform1fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Vec2, float, glUniform2fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Vec3, float, glUniform3fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Vec4, float, glUniform4fv)

// OpenGL expects int* as values for booleans
QT3D_UNIFORM_TYPE_IMPL(UniformType::Bool, int, glUniform1iv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::BVec2, int, glUniform2iv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::BVec3, int, glUniform3iv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::BVec4, int, glUniform4iv)

QT3D_UNIFORM_TYPE_IMPL(UniformType::Int, int, glUniform1iv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::IVec2, int, glUniform2iv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::IVec3, int, glUniform3iv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::IVec4, int, glUniform4iv)

QT3D_UNIFORM_TYPE_IMPL(UniformType::UInt, uint, glUniform1uiv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::UIVec2, uint, glUniform2uiv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::UIVec3, uint, glUniform3uiv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::UIVec4, uint, glUniform4uiv)

QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat2, float, glUniformMatrix2fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat3, float, glUniformMatrix3fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat4, float, glUniformMatrix4fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat2x3, float, glUniformMatrix2x3fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat3x2, float, glUniformMatrix3x2fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat2x4, float, glUniformMatrix2x4fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat4x2, float, glUniformMatrix4x2fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat3x4, float, glUniformMatrix3x4fv)
QT3D_UNIFORM_TYPE_IMPL(UniformType::Mat4x3, float, glUniformMatrix4x3fv)

} // namespace Render
} // namespace Qt3DRender of namespace

QT_END_NAMESPACE
