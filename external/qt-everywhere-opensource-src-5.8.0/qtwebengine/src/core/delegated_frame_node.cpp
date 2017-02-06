/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

// On Mac we need to reset this define in order to prevent definition
// of "check" macros etc. The "check" macro collides with a member function name in QtQuick.
// See AssertMacros.h in the Mac SDK.
#include <QtGlobal> // We need this for the Q_OS_MAC define.
#if defined(Q_OS_MAC)
#undef __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#endif

#include "delegated_frame_node.h"

#include "chromium_gpu_helper.h"
#include "gl_surface_qt.h"
#include "stream_video_node.h"
#include "type_conversion.h"
#include "yuv_video_node.h"

#include "base/message_loop/message_loop.h"
#include "base/bind.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"

#ifndef QT_NO_OPENGL
# include <QOpenGLContext>
# include <QOpenGLFunctions>
# include <QSGFlatColorMaterial>
#endif
#include <QSGTexture>
#include <private/qsgadaptationlayer_p.h>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
#include <QSGImageNode>
#include <QSGRectangleNode>
#else
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#endif

#if !defined(QT_NO_EGL)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#ifndef GL_TIMEOUT_IGNORED
#define GL_TIMEOUT_IGNORED                0xFFFFFFFFFFFFFFFFull
#endif

#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE              0x84F5
#endif

#ifndef GL_LINEAR
#define GL_LINEAR                         0x2601
#endif

#ifndef GL_RGBA
#define GL_RGBA                           0x1908
#endif

#ifndef GL_RGB
#define GL_RGB                            0x1907
#endif

#ifndef GL_LINE_LOOP
#define GL_LINE_LOOP                      0x0002
#endif

namespace QtWebEngineCore {
#ifndef QT_NO_OPENGL
class MailboxTexture : public QSGTexture, protected QOpenGLFunctions {
public:
    MailboxTexture(const gpu::MailboxHolder &mailboxHolder, const QSize textureSize);
    ~MailboxTexture();
    virtual int textureId() const Q_DECL_OVERRIDE { return m_textureId; }
    virtual QSize textureSize() const Q_DECL_OVERRIDE { return m_textureSize; }
    virtual bool hasAlphaChannel() const Q_DECL_OVERRIDE { return m_hasAlpha; }
    void setHasAlphaChannel(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    virtual bool hasMipmaps() const Q_DECL_OVERRIDE { return false; }
    virtual void bind() Q_DECL_OVERRIDE;

    gpu::MailboxHolder &mailboxHolder() { return m_mailboxHolder; }
    void fetchTexture(gpu::gles2::MailboxManager *mailboxManager);
    void setTarget(GLenum target);

private:
    gpu::MailboxHolder m_mailboxHolder;
    int m_textureId;
    QSize m_textureSize;
    bool m_hasAlpha;
    GLenum m_target;
#if defined(USE_X11)
    bool m_ownsTexture;
#endif
#ifdef Q_OS_QNX
    EGLStreamData m_eglStreamData;
#endif
    friend class DelegatedFrameNode;
};
#endif // QT_NO_OPENGL
class ResourceHolder {
public:
    ResourceHolder(const cc::TransferableResource &resource);
    QSharedPointer<QSGTexture> initTexture(bool quadIsAllOpaque, RenderWidgetHostViewQtDelegate *apiDelegate = 0);
    QSGTexture *texture() const { return m_texture.data(); }
    cc::TransferableResource &transferableResource() { return m_resource; }
    cc::ReturnedResource returnResource();
    void incImportCount() { ++m_importCount; }
    bool needsToFetch() const { return !m_resource.is_software && m_texture && !m_texture.data()->textureId(); }

private:
    QWeakPointer<QSGTexture> m_texture;
    cc::TransferableResource m_resource;
    int m_importCount;
};

class RectClipNode : public QSGClipNode
{
public:
    RectClipNode(const QRectF &);
private:
    QSGGeometry m_geometry;
};

static inline QSharedPointer<QSGLayer> findRenderPassLayer(const cc::RenderPassId &id, const QVector<QPair<cc::RenderPassId, QSharedPointer<QSGLayer> > > &list)
{
    typedef QPair<cc::RenderPassId, QSharedPointer<QSGLayer> > Pair;
    Q_FOREACH (const Pair &pair, list)
        if (pair.first == id)
            return pair.second;
    return QSharedPointer<QSGLayer>();
}

static QSGNode *buildRenderPassChain(QSGNode *chainParent)
{
    // Chromium already ordered the quads from back to front for us, however the
    // Qt scene graph layers individual geometries in their own z-range and uses
    // the depth buffer to visually stack nodes according to their item tree order.

    // This gets rid of the z component of all quads, once any x and y perspective
    // transformation has been applied to vertices not on the z=0 plane. Qt will
    // use an orthographic projection to render them.
    QSGTransformNode *zCompressNode = new QSGTransformNode;
    QMatrix4x4 zCompressMatrix;
    zCompressMatrix.scale(1, 1, 0);
    zCompressNode->setMatrix(zCompressMatrix);
    chainParent->appendChildNode(zCompressNode);
    return zCompressNode;
}

static QSGNode *buildLayerChain(QSGNode *chainParent, const cc::SharedQuadState *layerState)
{
    QSGNode *layerChain = chainParent;
    if (layerState->is_clipped) {
        RectClipNode *clipNode = new RectClipNode(toQt(layerState->clip_rect));
        layerChain->appendChildNode(clipNode);
        layerChain = clipNode;
    }
    if (!layerState->quad_to_target_transform.IsIdentity()) {
        QSGTransformNode *transformNode = new QSGTransformNode;
        transformNode->setMatrix(toQt(layerState->quad_to_target_transform.matrix()));
        layerChain->appendChildNode(transformNode);
        layerChain = transformNode;
    }
    if (layerState->opacity < 1.0) {
        QSGOpacityNode *opacityNode = new QSGOpacityNode;
        opacityNode->setOpacity(layerState->opacity);
        layerChain->appendChildNode(opacityNode);
        layerChain = opacityNode;
    }
    return layerChain;
}

#ifndef QT_NO_OPENGL
static void waitChromiumSync(gl::TransferableFence *sync)
{
    // Chromium uses its own GL bindings and stores in in thread local storage.
    // For that reason, let chromium_gpu_helper.cpp contain the producing code that will run in the Chromium
    // GPU thread, and put the sync consuming code here that will run in the QtQuick SG or GUI thread.
    switch (sync->type) {
    case gl::TransferableFence::NoSync:
        break;
    case gl::TransferableFence::EglSync:
#ifdef EGL_KHR_reusable_sync
    {
        static bool resolved = false;
        static PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR = 0;

        if (!resolved) {
            if (gl::GLSurfaceQt::HasEGLExtension("EGL_KHR_fence_sync")) {
                QOpenGLContext *context = QOpenGLContext::currentContext();
                eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)context->getProcAddress("eglClientWaitSyncKHR");
            }
            resolved = true;
        }

        if (eglClientWaitSyncKHR)
            // FIXME: Use the less wasteful eglWaitSyncKHR once we have a device that supports EGL_KHR_wait_sync.
            eglClientWaitSyncKHR(sync->egl.display, sync->egl.sync, 0, EGL_FOREVER_KHR);
    }
#endif
        break;
    case gl::TransferableFence::ArbSync:
        typedef void (QOPENGLF_APIENTRYP WaitSyncPtr)(GLsync sync, GLbitfield flags, GLuint64 timeout);
        static WaitSyncPtr glWaitSync_ = 0;
        if (!glWaitSync_) {
            QOpenGLContext *context = QOpenGLContext::currentContext();
            glWaitSync_ = (WaitSyncPtr)context->getProcAddress("glWaitSync");
            Q_ASSERT(glWaitSync_);
        }
        glWaitSync_(sync->arb.sync, 0, GL_TIMEOUT_IGNORED);
        break;
    }
}

static void deleteChromiumSync(gl::TransferableFence *sync)
{
    // Chromium uses its own GL bindings and stores in in thread local storage.
    // For that reason, let chromium_gpu_helper.cpp contain the producing code that will run in the Chromium
    // GPU thread, and put the sync consuming code here that will run in the QtQuick SG or GUI thread.
    switch (sync->type) {
    case gl::TransferableFence::NoSync:
        break;
    case gl::TransferableFence::EglSync:
#ifdef EGL_KHR_reusable_sync
    {
        static bool resolved = false;
        static PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR = 0;

        if (!resolved) {
            if (gl::GLSurfaceQt::HasEGLExtension("EGL_KHR_fence_sync")) {
                QOpenGLContext *context = QOpenGLContext::currentContext();
                eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)context->getProcAddress("eglDestroySyncKHR");
            }
            resolved = true;
        }

        if (eglDestroySyncKHR) {
            // FIXME: Use the less wasteful eglWaitSyncKHR once we have a device that supports EGL_KHR_wait_sync.
            eglDestroySyncKHR(sync->egl.display, sync->egl.sync);
            sync->reset();
        }
    }
#endif
        break;
    case gl::TransferableFence::ArbSync:
        typedef void (QOPENGLF_APIENTRYP DeleteSyncPtr)(GLsync sync);
        static DeleteSyncPtr glDeleteSync_ = 0;
        if (!glDeleteSync_) {
            QOpenGLContext *context = QOpenGLContext::currentContext();
            glDeleteSync_ = (DeleteSyncPtr)context->getProcAddress("glDeleteSync");
            Q_ASSERT(glDeleteSync_);
        }
        glDeleteSync_(sync->arb.sync);
        sync->reset();
        break;
    }
    // If Chromium was able to create a sync, we should have been able to handle its type here too.
    Q_ASSERT(!*sync);
}

MailboxTexture::MailboxTexture(const gpu::MailboxHolder &mailboxHolder, const QSize textureSize)
    : m_mailboxHolder(mailboxHolder)
    , m_textureId(0)
    , m_textureSize(textureSize)
    , m_hasAlpha(false)
    , m_target(GL_TEXTURE_2D)
#if defined(USE_X11)
    , m_ownsTexture(false)
#endif
{
    initializeOpenGLFunctions();

    // Assume that resources without a size will be used with a full source rect.
    // Setting a size of 1x1 will let any texture node compute a normalized source
    // rect of (0, 0) to (1, 1) while an empty texture size would set (0, 0) on all corners.
    if (m_textureSize.isEmpty())
        m_textureSize = QSize(1, 1);
}

MailboxTexture::~MailboxTexture()
{
#if defined(USE_X11)
   // This is rare case, where context is not shared
   // we created extra texture in current context, so
   // delete it now
   if (m_ownsTexture) {
       QOpenGLContext *currentContext = QOpenGLContext::currentContext() ;
       QOpenGLFunctions *funcs = currentContext->functions();
       GLuint id(m_textureId);
       funcs->glDeleteTextures(1, &id);
   }
#endif
}

void MailboxTexture::bind()
{
    glBindTexture(m_target, m_textureId);
#ifdef Q_OS_QNX
    if (m_target == GL_TEXTURE_EXTERNAL_OES) {
        static bool resolved = false;
        static PFNEGLSTREAMCONSUMERACQUIREKHRPROC eglStreamConsumerAcquire = 0;

        if (!resolved) {
            QOpenGLContext *context = QOpenGLContext::currentContext();
            eglStreamConsumerAcquire = (PFNEGLSTREAMCONSUMERACQUIREKHRPROC)context->getProcAddress("eglStreamConsumerAcquireKHR");
            resolved = true;
        }
        if (eglStreamConsumerAcquire)
            eglStreamConsumerAcquire(m_eglStreamData.egl_display, m_eglStreamData.egl_str_handle);
    }
#endif
}

void MailboxTexture::setTarget(GLenum target)
{
    m_target = target;
}

void MailboxTexture::fetchTexture(gpu::gles2::MailboxManager *mailboxManager)
{
    gpu::gles2::Texture *tex = ConsumeTexture(mailboxManager, m_target, m_mailboxHolder.mailbox);

    // The texture might already have been deleted (e.g. when navigating away from a page).
    if (tex) {
        m_textureId = service_id(tex);
#ifdef Q_OS_QNX
        if (m_target == GL_TEXTURE_EXTERNAL_OES) {
            m_eglStreamData = eglstream_connect_consumer(tex);
        }
#endif
    }
}
#endif //QT_NO_OPENGL

ResourceHolder::ResourceHolder(const cc::TransferableResource &resource)
    : m_resource(resource)
    , m_importCount(1)
{
}

QSharedPointer<QSGTexture> ResourceHolder::initTexture(bool quadNeedsBlending, RenderWidgetHostViewQtDelegate *apiDelegate)
{
    QSharedPointer<QSGTexture> texture = m_texture.toStrongRef();
    if (!texture) {
        if (m_resource.is_software) {
            Q_ASSERT(apiDelegate);
            std::unique_ptr<cc::SharedBitmap> sharedBitmap = content::HostSharedBitmapManager::current()->GetSharedBitmapFromId(m_resource.size, m_resource.mailbox_holder.mailbox);
            // QSG interprets QImage::hasAlphaChannel meaning that a node should enable blending
            // to draw it but Chromium keeps this information in the quads.
            // The input format is currently always Format_ARGB32_Premultiplied, so assume that all
            // alpha bytes are 0xff if quads aren't requesting blending and avoid the conversion
            // from Format_ARGB32_Premultiplied to Format_RGB32 just to get hasAlphaChannel to
            // return false.
            QImage::Format format = quadNeedsBlending ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;
            QImage image(sharedBitmap->pixels(), m_resource.size.width(), m_resource.size.height(), format);
            texture.reset(apiDelegate->createTextureFromImage(image.copy()));
        } else {
#ifndef QT_NO_OPENGL
            texture.reset(new MailboxTexture(m_resource.mailbox_holder, toQt(m_resource.size)));
            static_cast<MailboxTexture *>(texture.data())->setHasAlphaChannel(quadNeedsBlending);
#else
            Q_UNREACHABLE();
#endif
        }
        m_texture = texture;
    }
    // All quads using a resource should request the same blending state.
    Q_ASSERT(texture->hasAlphaChannel() || !quadNeedsBlending);
    return texture;
}

cc::ReturnedResource ResourceHolder::returnResource()
{
    cc::ReturnedResource returned;
    // The ResourceProvider ensures that the resource isn't used by the parent compositor's GL
    // context in the GPU process by inserting a sync point to be waited for by the child
    // compositor's GL context. We don't need this since we are triggering the delegated frame
    // ack directly from our rendering thread. At this point (in updatePaintNode) we know that
    // a frame that was compositing any of those resources has already been swapped and we thus
    // don't need to use this mechanism.
    returned.id = m_resource.id;
    returned.count = m_importCount;
    m_importCount = 0;
    return returned;
}


RectClipNode::RectClipNode(const QRectF &rect)
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
{
    QSGGeometry::updateRectGeometry(&m_geometry, rect);
    setGeometry(&m_geometry);
    setClipRect(rect);
    setIsRectangular(true);
}

DelegatedFrameNode::DelegatedFrameNode()
    : m_numPendingSyncPoints(0)
#if defined(USE_X11) && !defined(QT_NO_OPENGL)
    , m_contextShared(true)
#endif
{
    setFlag(UsePreprocess);
#if defined(USE_X11) && !defined(QT_NO_OPENGL)
    QOpenGLContext *currentContext = QOpenGLContext::currentContext() ;
    QOpenGLContext *sharedContext = qt_gl_global_share_context();
    if (!QOpenGLContext::areSharing(currentContext, sharedContext)) {
        static bool allowNotSharedContextWarningShown = true;
        if (allowNotSharedContextWarningShown) {
            allowNotSharedContextWarningShown = false;
            qWarning("Context is not shared, textures will be copied between contexts.");
        }
        m_offsurface.reset(new QOffscreenSurface);
        m_offsurface->create();
        m_contextShared = false;
    }
#endif
}

DelegatedFrameNode::~DelegatedFrameNode()
{
}

void DelegatedFrameNode::preprocess()
{
#ifndef QT_NO_OPENGL
    // With the threaded render loop the GUI thread has been unlocked at this point.
    // We can now wait for the Chromium GPU thread to produce textures that will be
    // rendered on our quads and fetch the IDs from the mailboxes we were given.
    QList<MailboxTexture *> mailboxesToFetch;
    typedef QHash<unsigned, QSharedPointer<ResourceHolder> >::const_iterator ResourceHolderIterator;
    ResourceHolderIterator end = m_chromiumCompositorData->resourceHolders.constEnd();
    for (ResourceHolderIterator it = m_chromiumCompositorData->resourceHolders.constBegin(); it != end ; ++it) {
        if ((*it)->needsToFetch())
            mailboxesToFetch.append(static_cast<MailboxTexture *>((*it)->texture()));
    }

    if (!mailboxesToFetch.isEmpty())
        fetchAndSyncMailboxes(mailboxesToFetch);

    // Then render any intermediate RenderPass in order.
    typedef QPair<cc::RenderPassId, QSharedPointer<QSGLayer> > Pair;
    Q_FOREACH (const Pair &pair, m_sgObjects.renderPassLayers) {
        // The layer is non-live, request a one-time update here.
        pair.second->scheduleUpdate();
        // Proceed with the actual update.
        pair.second->updateTexture();
    }
#endif
}

static YUVVideoMaterial::ColorSpace toQt(cc::YUVVideoDrawQuad::ColorSpace color_space)
{
    switch (color_space) {
    case cc::YUVVideoDrawQuad::REC_601:
        return YUVVideoMaterial::REC_601;
    case cc::YUVVideoDrawQuad::REC_709:
        return YUVVideoMaterial::REC_709;
    case cc::YUVVideoDrawQuad::JPEG:
        return YUVVideoMaterial::JPEG;
    }
    Q_UNREACHABLE();
    return YUVVideoMaterial::REC_601;
}

void DelegatedFrameNode::commit(ChromiumCompositorData *chromiumCompositorData, cc::ReturnedResourceArray *resourcesToRelease, RenderWidgetHostViewQtDelegate *apiDelegate)
{
    m_chromiumCompositorData = chromiumCompositorData;
    cc::DelegatedFrameData* frameData = m_chromiumCompositorData->frameData.get();
    if (!frameData)
        return;

    // DelegatedFrameNode is a transform node only for the purpose of
    // countering the scale of devicePixel-scaled tiles when rendering them
    // to the final surface.
    QMatrix4x4 matrix;
    matrix.scale(1 / m_chromiumCompositorData->frameDevicePixelRatio, 1 / m_chromiumCompositorData->frameDevicePixelRatio);
    setMatrix(matrix);

    // Keep the old objects in scope to hold a ref on layers, resources and textures
    // that we can re-use. Destroy the remaining objects before returning.
    SGObjects previousSGObjects;
    qSwap(m_sgObjects, previousSGObjects);
    QHash<unsigned, QSharedPointer<ResourceHolder> > resourceCandidates;
    qSwap(m_chromiumCompositorData->resourceHolders, resourceCandidates);

    // A frame's resource_list only contains the new resources to be added to the scene. Quads can
    // still reference resources that were added in previous frames. Add them to the list of
    // candidates to be picked up by quads, it's then our responsibility to return unused resources
    // to the producing child compositor.
    for (unsigned i = 0; i < frameData->resource_list.size(); ++i) {
        const cc::TransferableResource &res = frameData->resource_list.at(i);
        if (QSharedPointer<ResourceHolder> resource = resourceCandidates.value(res.id))
            resource->incImportCount();
        else
            resourceCandidates[res.id] = QSharedPointer<ResourceHolder>(new ResourceHolder(res));
    }

    frameData->resource_list.clear();

    // There is currently no way to know which and how quads changed since the last frame.
    // We have to reconstruct the node chain with their geometries on every update.
    // Intermediate render pass node chains are going to be destroyed when previousSGObjects
    // goes out of scope together with any QSGLayer that could reference them.
    while (QSGNode *oldChain = firstChild())
        delete oldChain;

    // The RenderPasses list is actually a tree where a parent RenderPass is connected
    // to its dependencies through a RenderPassId reference in one or more RenderPassQuads.
    // The list is already ordered with intermediate RenderPasses placed before their
    // parent, with the last one in the list being the root RenderPass, the one
    // that we displayed to the user.
    // All RenderPasses except the last one are rendered to an FBO.
    cc::RenderPass *rootRenderPass = frameData->render_pass_list.back().get();

    for (unsigned i = 0; i < frameData->render_pass_list.size(); ++i) {
        cc::RenderPass *pass = frameData->render_pass_list.at(i).get();

        QSGNode *renderPassParent = 0;
        if (pass != rootRenderPass) {
            QSharedPointer<QSGLayer> rpLayer = findRenderPassLayer(pass->id, previousSGObjects.renderPassLayers);
            if (!rpLayer) {
                rpLayer = QSharedPointer<QSGLayer>(apiDelegate->createLayer());
                // Avoid any premature texture update since we need to wait
                // for the GPU thread to produce the dependent resources first.
                rpLayer->setLive(false);
            }
            QSharedPointer<QSGRootNode> rootNode(new QSGRootNode);
            rpLayer->setRect(toQt(pass->output_rect));
            rpLayer->setSize(toQt(pass->output_rect.size()));
            rpLayer->setFormat(pass->has_transparent_background ? GL_RGBA : GL_RGB);
            rpLayer->setItem(rootNode.data());
            m_sgObjects.renderPassLayers.append(QPair<cc::RenderPassId, QSharedPointer<QSGLayer> >(pass->id, rpLayer));
            m_sgObjects.renderPassRootNodes.append(rootNode);
            renderPassParent = rootNode.data();
        } else
            renderPassParent = this;

        QSGNode *renderPassChain = buildRenderPassChain(renderPassParent);
        const cc::SharedQuadState *currentLayerState = 0;
        QSGNode *currentLayerChain = 0;

        cc::QuadList::ConstBackToFrontIterator it = pass->quad_list.BackToFrontBegin();
        cc::QuadList::ConstBackToFrontIterator end = pass->quad_list.BackToFrontEnd();
        for (; it != end; ++it) {
            const cc::DrawQuad *quad = *it;

            if (currentLayerState != quad->shared_quad_state) {
                currentLayerState = quad->shared_quad_state;
                currentLayerChain = buildLayerChain(renderPassChain, currentLayerState);
            }

            switch (quad->material) {
            case cc::DrawQuad::RENDER_PASS: {
                const cc::RenderPassDrawQuad *renderPassQuad = cc::RenderPassDrawQuad::MaterialCast(quad);
                QSGTexture *layer = findRenderPassLayer(renderPassQuad->render_pass_id, m_sgObjects.renderPassLayers).data();
                // cc::GLRenderer::DrawRenderPassQuad silently ignores missing render passes.
                if (!layer)
                    continue;

                // Only QSGInternalImageNode currently supports QSGLayer textures.
                QSGInternalImageNode *imageNode = apiDelegate->createImageNode();
                imageNode->setTargetRect(toQt(quad->rect));
                imageNode->setInnerTargetRect(toQt(quad->rect));
                imageNode->setTexture(layer);
                imageNode->update();
                currentLayerChain->appendChildNode(imageNode);
                break;
            } case cc::DrawQuad::TEXTURE_CONTENT: {
                const cc::TextureDrawQuad *tquad = cc::TextureDrawQuad::MaterialCast(quad);
                ResourceHolder *resource = findAndHoldResource(tquad->resource_id(), resourceCandidates);

                QSGTextureNode *textureNode = apiDelegate->createTextureNode();
                textureNode->setTextureCoordinatesTransform(tquad->y_flipped ? QSGTextureNode::MirrorVertically : QSGTextureNode::NoTransform);
                textureNode->setRect(toQt(quad->rect));
                textureNode->setFiltering(resource->transferableResource().filter == GL_LINEAR ? QSGTexture::Linear : QSGTexture::Nearest);
                textureNode->setTexture(initAndHoldTexture(resource, quad->ShouldDrawWithBlending(), apiDelegate));
                currentLayerChain->appendChildNode(textureNode);
                break;
            } case cc::DrawQuad::SOLID_COLOR: {
                const cc::SolidColorDrawQuad *scquad = cc::SolidColorDrawQuad::MaterialCast(quad);
                QSGRectangleNode *rectangleNode = apiDelegate->createRectangleNode();

                // Qt only supports MSAA and this flag shouldn't be needed.
                // If we ever want to use QSGRectangleNode::setAntialiasing for this we should
                // try to see if we can do something similar for tile quads first.
                Q_UNUSED(scquad->force_anti_aliasing_off);

                rectangleNode->setRect(toQt(quad->rect));
                rectangleNode->setColor(toQt(scquad->color));
                currentLayerChain->appendChildNode(rectangleNode);
                break;
#ifndef QT_NO_OPENGL
            } case cc::DrawQuad::DEBUG_BORDER: {
                const cc::DebugBorderDrawQuad *dbquad = cc::DebugBorderDrawQuad::MaterialCast(quad);
                QSGGeometryNode *geometryNode = new QSGGeometryNode;

                QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
                geometry->setDrawingMode(GL_LINE_LOOP);
                geometry->setLineWidth(dbquad->width);
                // QSGGeometry::updateRectGeometry would actually set the corners in the following order:
                // top-left, bottom-left, top-right, bottom-right, leading to a nice criss cross, instead
                // of having a closed loop.
                const gfx::Rect &r(dbquad->rect);
                geometry->vertexDataAsPoint2D()[0].set(r.x(), r.y());
                geometry->vertexDataAsPoint2D()[1].set(r.x() + r.width(), r.y());
                geometry->vertexDataAsPoint2D()[2].set(r.x() + r.width(), r.y() + r.height());
                geometry->vertexDataAsPoint2D()[3].set(r.x(), r.y() + r.height());
                geometryNode->setGeometry(geometry);

                QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
                material->setColor(toQt(dbquad->color));
                geometryNode->setMaterial(material);

                geometryNode->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
                currentLayerChain->appendChildNode(geometryNode);
                break;
#endif
            } case cc::DrawQuad::TILED_CONTENT: {
                const cc::TileDrawQuad *tquad = cc::TileDrawQuad::MaterialCast(quad);
                ResourceHolder *resource = findAndHoldResource(tquad->resource_id(), resourceCandidates);

                QSGTextureNode *textureNode = apiDelegate->createTextureNode();
                textureNode->setRect(toQt(quad->rect));
                textureNode->setSourceRect(toQt(tquad->tex_coord_rect));
                textureNode->setFiltering(resource->transferableResource().filter == GL_LINEAR ? QSGTexture::Linear : QSGTexture::Nearest);
                textureNode->setTexture(initAndHoldTexture(resource, quad->ShouldDrawWithBlending(), apiDelegate));
                currentLayerChain->appendChildNode(textureNode);
                break;
#ifndef QT_NO_OPENGL
            } case cc::DrawQuad::YUV_VIDEO_CONTENT: {
                const cc::YUVVideoDrawQuad *vquad = cc::YUVVideoDrawQuad::MaterialCast(quad);
                ResourceHolder *yResource = findAndHoldResource(vquad->y_plane_resource_id(), resourceCandidates);
                ResourceHolder *uResource = findAndHoldResource(vquad->u_plane_resource_id(), resourceCandidates);
                ResourceHolder *vResource = findAndHoldResource(vquad->v_plane_resource_id(), resourceCandidates);
                ResourceHolder *aResource = 0;
                // This currently requires --enable-vp8-alpha-playback and needs a video with alpha data to be triggered.
                if (vquad->a_plane_resource_id())
                    aResource = findAndHoldResource(vquad->a_plane_resource_id(), resourceCandidates);

                YUVVideoNode *videoNode = new YUVVideoNode(
                    initAndHoldTexture(yResource, quad->ShouldDrawWithBlending()),
                    initAndHoldTexture(uResource, quad->ShouldDrawWithBlending()),
                    initAndHoldTexture(vResource, quad->ShouldDrawWithBlending()),
                    aResource ? initAndHoldTexture(aResource, quad->ShouldDrawWithBlending()) : 0,
                    toQt(vquad->ya_tex_coord_rect), toQt(vquad->uv_tex_coord_rect),
                    toQt(vquad->ya_tex_size), toQt(vquad->uv_tex_size),
                    toQt(vquad->color_space),
                    vquad->resource_multiplier, vquad->resource_offset);
                videoNode->setRect(toQt(quad->rect));
                currentLayerChain->appendChildNode(videoNode);
                break;
#ifdef GL_OES_EGL_image_external
            } case cc::DrawQuad::STREAM_VIDEO_CONTENT: {
                const cc::StreamVideoDrawQuad *squad = cc::StreamVideoDrawQuad::MaterialCast(quad);
                ResourceHolder *resource = findAndHoldResource(squad->resource_id(), resourceCandidates);
                MailboxTexture *texture = static_cast<MailboxTexture *>(initAndHoldTexture(resource, quad->ShouldDrawWithBlending()));
                texture->setTarget(GL_TEXTURE_EXTERNAL_OES); // since this is not default TEXTURE_2D type

                StreamVideoNode *svideoNode = new StreamVideoNode(texture, false, ExternalTarget);
                svideoNode->setRect(toQt(squad->rect));
                svideoNode->setTextureMatrix(toQt(squad->matrix.matrix()));
                currentLayerChain->appendChildNode(svideoNode);
                break;
#endif // GL_OES_EGL_image_external
#endif // QT_NO_OPENGL
            } case cc::DrawQuad::SURFACE_CONTENT:
                Q_UNREACHABLE();
            default:
                qWarning("Unimplemented quad material: %d", quad->material);
            }
        }
    }

    // Send resources of remaining candidates back to the child compositors so that they can be freed or reused.
    typedef QHash<unsigned, QSharedPointer<ResourceHolder> >::const_iterator ResourceHolderIterator;
    ResourceHolderIterator end = resourceCandidates.constEnd();
    for (ResourceHolderIterator it = resourceCandidates.constBegin(); it != end ; ++it)
        resourcesToRelease->push_back((*it)->returnResource());
}

ResourceHolder *DelegatedFrameNode::findAndHoldResource(unsigned resourceId, QHash<unsigned, QSharedPointer<ResourceHolder> > &candidates)
{
    // ResourceHolders must survive when the scene graph destroys our node branch
    QSharedPointer<ResourceHolder> &resource = m_chromiumCompositorData->resourceHolders[resourceId];
    if (!resource)
        resource = candidates.take(resourceId);
    Q_ASSERT(resource);
    return resource.data();
}

QSGTexture *DelegatedFrameNode::initAndHoldTexture(ResourceHolder *resource, bool quadIsAllOpaque, RenderWidgetHostViewQtDelegate *apiDelegate)
{
    // QSGTextures must be destroyed in the scene graph thread as part of the QSGNode tree,
    // so we can't store them with the ResourceHolder in m_chromiumCompositorData.
    // Hold them through a QSharedPointer solely on the root DelegatedFrameNode of the web view
    // and access them through a QWeakPointer from the resource holder to find them later.
    m_sgObjects.textureStrongRefs.append(resource->initTexture(quadIsAllOpaque, apiDelegate));
    return m_sgObjects.textureStrongRefs.last().data();
}

void DelegatedFrameNode::fetchAndSyncMailboxes(QList<MailboxTexture *> &mailboxesToFetch)
{
#ifndef QT_NO_OPENGL
    QList<gl::TransferableFence> transferredFences;
    {
        QMutexLocker lock(&m_mutex);

        gpu::SyncPointManager *syncPointManager = sync_point_manager();
        if (!m_syncPointClient)
            m_syncPointClient = syncPointManager->CreateSyncPointClientWaiter();
        base::MessageLoop *gpuMessageLoop = gpu_message_loop();
        Q_ASSERT(m_numPendingSyncPoints == 0);
        m_numPendingSyncPoints = mailboxesToFetch.count();
        auto it = mailboxesToFetch.constBegin();
        auto end = mailboxesToFetch.constEnd();
        for (; it != end; ++it) {
            MailboxTexture *mailboxTexture = *it;
            gpu::SyncToken &syncToken = mailboxTexture->mailboxHolder().sync_token;
            if (syncToken.HasData()) {
                scoped_refptr<gpu::SyncPointClientState> release_state =
                    syncPointManager->GetSyncPointClientState(syncToken.namespace_id(), syncToken.command_buffer_id());
                if (release_state && !release_state->IsFenceSyncReleased(syncToken.release_count())) {
                    m_syncPointClient->WaitOutOfOrderNonThreadSafe(
                                release_state.get(), syncToken.release_count(),
                                gpuMessageLoop->task_runner(), base::Bind(&DelegatedFrameNode::pullTexture, this, mailboxTexture));
                    continue;
                }
            }
            gpuMessageLoop->PostTask(FROM_HERE, base::Bind(&DelegatedFrameNode::pullTexture, this, mailboxTexture));
        }

        m_mailboxesFetchedWaitCond.wait(&m_mutex);
        m_textureFences.swap(transferredFences);
    }

    Q_FOREACH (gl::TransferableFence sync, transferredFences) {
        // We need to wait on the fences on the Qt current context, and
        // can therefore not use GLFence routines that uses a different
        // concept of current context.
        waitChromiumSync(&sync);
        deleteChromiumSync(&sync);
    }

#if defined(USE_X11)
    // Workaround when context is not shared QTBUG-48969
    // Make slow copy between two contexts.
    if (!m_contextShared) {
        QOpenGLContext *currentContext = QOpenGLContext::currentContext() ;
        QOpenGLContext *sharedContext = qt_gl_global_share_context();

        QSurface *surface = currentContext->surface();
        Q_ASSERT(m_offsurface);
        sharedContext->makeCurrent(m_offsurface.data());
        QOpenGLFunctions *funcs = sharedContext->functions();

        GLuint fbo = 0;
        funcs->glGenFramebuffers(1, &fbo);

        Q_FOREACH (MailboxTexture *mailboxTexture, mailboxesToFetch) {
            // Read texture into QImage from shared context.
            // Switch to shared context.
            sharedContext->makeCurrent(m_offsurface.data());
            funcs = sharedContext->functions();
            QImage img(mailboxTexture->textureSize(), QImage::Format_RGBA8888_Premultiplied);
            funcs->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            funcs->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mailboxTexture->m_textureId, 0);
            GLenum status = funcs->glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                qWarning("fbo error, skipping slow copy...");
                continue;
            }
            funcs->glReadPixels(0, 0, mailboxTexture->textureSize().width(), mailboxTexture->textureSize().height(),
                                GL_RGBA, GL_UNSIGNED_BYTE, img.bits());

            // Restore current context.
            // Create texture from QImage in current context.
            currentContext->makeCurrent(surface);
            GLuint texture = 0;
            funcs = currentContext->functions();
            funcs->glGenTextures(1, &texture);
            funcs->glBindTexture(GL_TEXTURE_2D, texture);
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            funcs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            funcs->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mailboxTexture->textureSize().width(), mailboxTexture->textureSize().height(), 0,
                                GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
            mailboxTexture->m_textureId = texture;
            mailboxTexture->m_ownsTexture = true;
        }
        // Cleanup allocated resources
        sharedContext->makeCurrent(m_offsurface.data());
        funcs = sharedContext->functions();
        funcs->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        funcs->glDeleteFramebuffers(1, &fbo);
        currentContext->makeCurrent(surface);
    }
#endif
#else
    Q_UNUSED(mailboxesToFetch)
#endif //QT_NO_OPENGL
}


void DelegatedFrameNode::pullTexture(DelegatedFrameNode *frameNode, MailboxTexture *texture)
{
#ifndef QT_NO_OPENGL
    gpu::gles2::MailboxManager *mailboxManager = mailbox_manager();
    gpu::SyncToken &syncToken = texture->mailboxHolder().sync_token;
    if (syncToken.HasData())
        mailboxManager->PullTextureUpdates(syncToken);
    texture->fetchTexture(mailboxManager);
    if (!!gl::GLContext::GetCurrent() && gl::GLFence::IsSupported()) {
        // Create a fence on the Chromium GPU-thread and context
        gl::GLFence *fence = gl::GLFence::Create();
        // But transfer it to something generic since we need to read it using Qt's OpenGL.
        frameNode->m_textureFences.append(fence->Transfer());
        delete fence;
    }
    if (--frameNode->m_numPendingSyncPoints == 0)
        base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&DelegatedFrameNode::fenceAndUnlockQt, frameNode));
#else
    Q_UNUSED(frameNode)
    Q_UNUSED(texture)
#endif
}

void DelegatedFrameNode::fenceAndUnlockQt(DelegatedFrameNode *frameNode)
{
    QMutexLocker lock(&frameNode->m_mutex);
    // Signal preprocess() the textures are ready
    frameNode->m_mailboxesFetchedWaitCond.wakeOne();
}

} // namespace QtWebEngineCore
