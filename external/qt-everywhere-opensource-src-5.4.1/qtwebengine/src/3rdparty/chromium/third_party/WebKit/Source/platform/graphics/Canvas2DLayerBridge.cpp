/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/graphics/Canvas2DLayerBridge.h"

#include "GrContext.h"
#include "SkDevice.h"
#include "SkSurface.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/Canvas2DLayerManager.h"
#include "platform/graphics/GraphicsLayer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "wtf/RefCountedLeakCounter.h"

using blink::WebExternalTextureLayer;
using blink::WebGraphicsContext3D;

namespace {
enum {
    InvalidMailboxIndex = -1,
};

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, canvas2DLayerBridgeInstanceCounter, ("Canvas2DLayerBridge"));
}

namespace WebCore {

static PassRefPtr<SkSurface> createSkSurface(GrContext* gr, const IntSize& size, int msaaSampleCount = 0)
{
    if (!gr)
        return nullptr;
    gr->resetContext();
    SkImageInfo info;
    info.fWidth = size.width();
    info.fHeight = size.height();
    info.fColorType = kPMColor_SkColorType;
    info.fAlphaType = kPremul_SkAlphaType;
    return adoptRef(SkSurface::NewRenderTarget(gr, info,  msaaSampleCount));
}

PassRefPtr<Canvas2DLayerBridge> Canvas2DLayerBridge::create(const IntSize& size, OpacityMode opacityMode, int msaaSampleCount)
{
    TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation");
    OwnPtr<blink::WebGraphicsContext3DProvider> contextProvider = adoptPtr(blink::Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!contextProvider)
        return nullptr;
    RefPtr<SkSurface> surface(createSkSurface(contextProvider->grContext(), size, msaaSampleCount));
    if (!surface)
        return nullptr;
    RefPtr<Canvas2DLayerBridge> layerBridge;
    OwnPtr<SkDeferredCanvas> canvas = adoptPtr(SkDeferredCanvas::Create(surface.get()));
    layerBridge = adoptRef(new Canvas2DLayerBridge(contextProvider.release(), canvas.release(), msaaSampleCount, opacityMode));
    return layerBridge.release();
}

Canvas2DLayerBridge::Canvas2DLayerBridge(PassOwnPtr<blink::WebGraphicsContext3DProvider> contextProvider, PassOwnPtr<SkDeferredCanvas> canvas, int msaaSampleCount, OpacityMode opacityMode)
    : m_canvas(canvas)
    , m_contextProvider(contextProvider)
    , m_imageBuffer(0)
    , m_msaaSampleCount(msaaSampleCount)
    , m_bytesAllocated(0)
    , m_didRecordDrawCommand(false)
    , m_isSurfaceValid(true)
    , m_framesPending(0)
    , m_framesSinceMailboxRelease(0)
    , m_destructionInProgress(false)
    , m_rateLimitingEnabled(false)
    , m_isHidden(false)
    , m_next(0)
    , m_prev(0)
    , m_lastImageId(0)
    , m_releasedMailboxInfoIndex(InvalidMailboxIndex)
{
    ASSERT(m_canvas);
    ASSERT(m_contextProvider);
    // Used by browser tests to detect the use of a Canvas2DLayerBridge.
    TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation");
    m_layer = adoptPtr(blink::Platform::current()->compositorSupport()->createExternalTextureLayer(this));
    m_layer->setOpaque(opacityMode == Opaque);
    m_layer->setBlendBackgroundColor(opacityMode != Opaque);
    GraphicsLayer::registerContentsLayer(m_layer->layer());
    m_layer->setRateLimitContext(m_rateLimitingEnabled);
    m_canvas->setNotificationClient(this);
#ifndef NDEBUG
    canvas2DLayerBridgeInstanceCounter.increment();
#endif
}

Canvas2DLayerBridge::~Canvas2DLayerBridge()
{
    ASSERT(m_destructionInProgress);
    ASSERT(!Canvas2DLayerManager::get().isInList(this));
    m_layer.clear();
    freeReleasedMailbox();
#if ASSERT_ENABLED
    Vector<MailboxInfo>::iterator mailboxInfo;
    for (mailboxInfo = m_mailboxes.begin(); mailboxInfo < m_mailboxes.end(); ++mailboxInfo) {
        ASSERT(mailboxInfo->m_status != MailboxInUse);
        ASSERT(mailboxInfo->m_status != MailboxReleased || m_contextProvider->context3d()->isContextLost() || !m_isSurfaceValid);
    }
#endif
    m_mailboxes.clear();
#ifndef NDEBUG
    canvas2DLayerBridgeInstanceCounter.decrement();
#endif
}

void Canvas2DLayerBridge::beginDestruction()
{
    ASSERT(!m_destructionInProgress);
    setRateLimitingEnabled(false);
    m_canvas->silentFlush();
    m_imageBuffer = 0;
    freeTransientResources();
    setIsHidden(true);
    m_destructionInProgress = true;
    GraphicsLayer::unregisterContentsLayer(m_layer->layer());
    m_canvas->setNotificationClient(0);
    m_canvas.clear();
    m_layer->clearTexture();
    // Orphaning the layer is required to trigger the recration of a new layer
    // in the case where destruction is caused by a canvas resize. Test:
    // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
    m_layer->layer()->removeFromParent();
    // To anyone who ever hits this assert: Please update crbug.com/344666
    // with repro steps.
    ASSERT(!m_bytesAllocated);
}

void Canvas2DLayerBridge::setIsHidden(bool hidden)
{
    ASSERT(!m_destructionInProgress);
    bool newHiddenValue = hidden || m_destructionInProgress;
    if (m_isHidden == newHiddenValue)
        return;

    m_isHidden = newHiddenValue;
    if (isHidden()) {
        freeTransientResources();
    }
}

void Canvas2DLayerBridge::freeTransientResources()
{
    ASSERT(!m_destructionInProgress);
    freeReleasedMailbox();
    flush();
    freeMemoryIfPossible(bytesAllocated());
    ASSERT(!hasTransientResources());
}

bool Canvas2DLayerBridge::hasTransientResources() const
{
    return !m_destructionInProgress && (hasReleasedMailbox() || bytesAllocated());
}

void Canvas2DLayerBridge::limitPendingFrames()
{
    ASSERT(!m_destructionInProgress);
    if (isHidden()) {
        freeTransientResources();
        return;
    }
    if (m_didRecordDrawCommand) {
        m_framesPending++;
        m_didRecordDrawCommand = false;
        if (m_framesPending > 1) {
            // Turn on the rate limiter if this layer tends to accumulate a
            // non-discardable multi-frame backlog of draw commands.
            setRateLimitingEnabled(true);
        }
        if (m_rateLimitingEnabled) {
            flush();
        }
    }
    ++m_framesSinceMailboxRelease;
    if (releasedMailboxHasExpired()) {
        freeReleasedMailbox();
    }
}

void Canvas2DLayerBridge::prepareForDraw()
{
    ASSERT(!m_destructionInProgress);
    ASSERT(m_layer);
    if (!checkSurfaceValid()) {
        if (m_canvas) {
            // drop pending commands because there is no surface to draw to
            m_canvas->silentFlush();
        }
        return;
    }
    context()->makeContextCurrent();
}

void Canvas2DLayerBridge::storageAllocatedForRecordingChanged(size_t bytesAllocated)
{
    ASSERT(!m_destructionInProgress);
    intptr_t delta = (intptr_t)bytesAllocated - (intptr_t)m_bytesAllocated;
    m_bytesAllocated = bytesAllocated;
    Canvas2DLayerManager::get().layerTransientResourceAllocationChanged(this, delta);
}

size_t Canvas2DLayerBridge::storageAllocatedForRecording()
{
    return m_canvas->storageAllocatedForRecording();
}

void Canvas2DLayerBridge::flushedDrawCommands()
{
    ASSERT(!m_destructionInProgress);
    storageAllocatedForRecordingChanged(storageAllocatedForRecording());
    m_framesPending = 0;
}

void Canvas2DLayerBridge::skippedPendingDrawCommands()
{
    ASSERT(!m_destructionInProgress);
    // Stop triggering the rate limiter if SkDeferredCanvas is detecting
    // and optimizing overdraw.
    setRateLimitingEnabled(false);
    flushedDrawCommands();
}

void Canvas2DLayerBridge::setRateLimitingEnabled(bool enabled)
{
    ASSERT(!m_destructionInProgress);
    if (m_rateLimitingEnabled != enabled) {
        m_rateLimitingEnabled = enabled;
        m_layer->setRateLimitContext(m_rateLimitingEnabled);
    }
}

size_t Canvas2DLayerBridge::freeMemoryIfPossible(size_t bytesToFree)
{
    ASSERT(!m_destructionInProgress);
    size_t bytesFreed = m_canvas->freeMemoryIfPossible(bytesToFree);
    m_bytesAllocated -= bytesFreed;
    if (bytesFreed)
        Canvas2DLayerManager::get().layerTransientResourceAllocationChanged(this, -((intptr_t)bytesFreed));
    return bytesFreed;
}

void Canvas2DLayerBridge::flush()
{
    ASSERT(!m_destructionInProgress);
    if (m_canvas->hasPendingCommands()) {
        TRACE_EVENT0("cc", "Canvas2DLayerBridge::flush");
        freeReleasedMailbox(); // To avoid unnecessary triple-buffering
        m_canvas->flush();
    }
}

bool Canvas2DLayerBridge::releasedMailboxHasExpired()
{
    // This heuristic indicates that the canvas is not being
    // actively presented by the compositor (3 frames rendered since
    // last mailbox release), suggesting that double buffering is not required.
    return hasReleasedMailbox() && m_framesSinceMailboxRelease > 2;
}

Canvas2DLayerBridge::MailboxInfo* Canvas2DLayerBridge::releasedMailboxInfo()
{
    return hasReleasedMailbox() ? &m_mailboxes[m_releasedMailboxInfoIndex] : 0;
}

bool Canvas2DLayerBridge::hasReleasedMailbox() const
{
    return m_releasedMailboxInfoIndex != InvalidMailboxIndex;
}

void Canvas2DLayerBridge::freeReleasedMailbox()
{
    if (m_contextProvider->context3d()->isContextLost() || !m_isSurfaceValid)
        return;
    MailboxInfo* mailboxInfo = releasedMailboxInfo();
    if (!mailboxInfo)
        return;

    ASSERT(mailboxInfo->m_status == MailboxReleased);
    if (mailboxInfo->m_mailbox.syncPoint) {
        context()->waitSyncPoint(mailboxInfo->m_mailbox.syncPoint);
        mailboxInfo->m_mailbox.syncPoint = 0;
    }
    // Invalidate texture state in case the compositor altered it since the copy-on-write.
    if (mailboxInfo->m_image) {
        if (isHidden() || releasedMailboxHasExpired())
            mailboxInfo->m_image->getTexture()->resetFlag(static_cast<GrTextureFlags>(GrTexture::kReturnToCache_FlagBit));
        mailboxInfo->m_image->getTexture()->textureParamsModified();
        mailboxInfo->m_image.clear();
    }
    mailboxInfo->m_status = MailboxAvailable;
    m_releasedMailboxInfoIndex = InvalidMailboxIndex;
    Canvas2DLayerManager::get().layerTransientResourceAllocationChanged(this);
}

blink::WebGraphicsContext3D* Canvas2DLayerBridge::context()
{
    // Check on m_layer is necessary because context() may be called during
    // the destruction of m_layer
    if (m_layer && !m_destructionInProgress)
        checkSurfaceValid(); // To ensure rate limiter is disabled if context is lost.
    return m_contextProvider->context3d();
}

bool Canvas2DLayerBridge::checkSurfaceValid()
{
    ASSERT(!m_destructionInProgress);
    if (m_destructionInProgress || !m_isSurfaceValid)
        return false;
    if (m_contextProvider->context3d()->isContextLost()) {
        m_isSurfaceValid = false;
        if (m_imageBuffer)
            m_imageBuffer->notifySurfaceInvalid();
        setRateLimitingEnabled(false);
    }
    return m_isSurfaceValid;
}

bool Canvas2DLayerBridge::restoreSurface()
{
    ASSERT(!m_destructionInProgress);
    if (m_destructionInProgress)
        return false;
    ASSERT(m_layer && !m_isSurfaceValid);

    blink::WebGraphicsContext3D* sharedContext = 0;
    // We must clear the mailboxes before calling m_layer->clearTexture() to prevent
    // re-entry via mailboxReleased from operating on defunct GrContext objects.
    m_mailboxes.clear();
    m_releasedMailboxInfoIndex = InvalidMailboxIndex;
    m_layer->clearTexture();
    m_contextProvider = adoptPtr(blink::Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (m_contextProvider)
        sharedContext = m_contextProvider->context3d();

    if (sharedContext && !sharedContext->isContextLost()) {
        IntSize size(m_canvas->getTopDevice()->width(), m_canvas->getTopDevice()->height());
        RefPtr<SkSurface> surface(createSkSurface(m_contextProvider->grContext(), size, m_msaaSampleCount));
        if (surface.get()) {
            m_canvas->setSurface(surface.get());
            m_isSurfaceValid = true;
            // FIXME: draw sad canvas picture into new buffer crbug.com/243842
        }
    }

    return m_isSurfaceValid;
}

bool Canvas2DLayerBridge::prepareMailbox(blink::WebExternalTextureMailbox* outMailbox, blink::WebExternalBitmap* bitmap)
{
    if (m_destructionInProgress) {
        // It can be hit in the following sequence.
        // 1. Canvas draws something.
        // 2. The compositor begins the frame.
        // 3. Javascript makes a context be lost.
        // 4. Here.
        return false;
    }
    if (bitmap) {
        // Using accelerated 2d canvas with software renderer, which
        // should only happen in tests that use fake graphics contexts
        // or in Android WebView in software mode. In this case, we do
        // not care about producing any results for this canvas.
        m_canvas->silentFlush();
        m_lastImageId = 0;
        return false;
    }
    if (!checkSurfaceValid())
        return false;

    blink::WebGraphicsContext3D* webContext = context();

    // Release to skia textures that were previouosly released by the
    // compositor. We do this before acquiring the next snapshot in
    // order to cap maximum gpu memory consumption.
    webContext->makeContextCurrent();
    flush();

    RefPtr<SkImage> image = adoptRef(m_canvas->newImageSnapshot());

    // Early exit if canvas was not drawn to since last prepareMailbox
    if (image->uniqueID() == m_lastImageId)
        return false;
    m_lastImageId = image->uniqueID();

    MailboxInfo* mailboxInfo = createMailboxInfo();
    mailboxInfo->m_status = MailboxInUse;
    mailboxInfo->m_image = image;

    ASSERT(mailboxInfo->m_mailbox.syncPoint == 0);
    ASSERT(mailboxInfo->m_image.get());
    ASSERT(mailboxInfo->m_image->getTexture());

    // Because of texture sharing with the compositor, we must invalidate
    // the state cached in skia so that the deferred copy on write
    // in SkSurface_Gpu does not make any false assumptions.
    mailboxInfo->m_image->getTexture()->textureParamsModified();

    webContext->bindTexture(GL_TEXTURE_2D, mailboxInfo->m_image->getTexture()->getTextureHandle());
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    webContext->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    webContext->produceTextureCHROMIUM(GL_TEXTURE_2D, mailboxInfo->m_mailbox.name);
    if (isHidden()) {
        // With hidden canvases, we release the SkImage immediately because
        // there is no need for animations to be double buffered.
        mailboxInfo->m_image.clear();
    } else {
        webContext->flush();
        mailboxInfo->m_mailbox.syncPoint = webContext->insertSyncPoint();
    }
    webContext->bindTexture(GL_TEXTURE_2D, 0);
    // Because we are changing the texture binding without going through skia,
    // we must dirty the context.
    m_contextProvider->grContext()->resetContext(kTextureBinding_GrGLBackendState);

    // set m_parentLayerBridge to make sure 'this' stays alive as long as it has
    // live mailboxes
    ASSERT(!mailboxInfo->m_parentLayerBridge);
    mailboxInfo->m_parentLayerBridge = this;
    *outMailbox = mailboxInfo->m_mailbox;
    return true;
}

Canvas2DLayerBridge::MailboxInfo* Canvas2DLayerBridge::createMailboxInfo() {
    ASSERT(!m_destructionInProgress);
    MailboxInfo* mailboxInfo;
    for (mailboxInfo = m_mailboxes.begin(); mailboxInfo < m_mailboxes.end(); mailboxInfo++) {
        if (mailboxInfo->m_status == MailboxAvailable) {
            return mailboxInfo;
        }
    }

    // No available mailbox: create one.
    m_mailboxes.grow(m_mailboxes.size() + 1);
    mailboxInfo = &m_mailboxes.last();
    context()->genMailboxCHROMIUM(mailboxInfo->m_mailbox.name);
    // Worst case, canvas is triple buffered.  More than 3 active mailboxes
    // means there is a problem.
    // For the single-threaded case, this value needs to be at least
    // kMaxSwapBuffersPending+1 (in render_widget.h).
    // Because of crbug.com/247874, it needs to be kMaxSwapBuffersPending+2.
    // TODO(piman): fix this.
    ASSERT(m_mailboxes.size() <= 4);
    ASSERT(mailboxInfo < m_mailboxes.end());
    return mailboxInfo;
}

void Canvas2DLayerBridge::mailboxReleased(const blink::WebExternalTextureMailbox& mailbox)
{
    freeReleasedMailbox(); // Never have more than one mailbox in the released state.
    Vector<MailboxInfo>::iterator mailboxInfo;
    for (mailboxInfo = m_mailboxes.begin(); mailboxInfo < m_mailboxes.end(); ++mailboxInfo) {
        if (nameEquals(mailboxInfo->m_mailbox, mailbox)) {
            mailboxInfo->m_mailbox.syncPoint = mailbox.syncPoint;
            ASSERT(mailboxInfo->m_status == MailboxInUse);
            mailboxInfo->m_status = MailboxReleased;
            // Trigger Canvas2DLayerBridge self-destruction if this is the
            // last live mailbox and the layer bridge is not externally
            // referenced.
            m_releasedMailboxInfoIndex = mailboxInfo - m_mailboxes.begin();
            m_framesSinceMailboxRelease = 0;
            if (isHidden()) {
                freeReleasedMailbox();
            } else {
                ASSERT(!m_destructionInProgress);
                Canvas2DLayerManager::get().layerTransientResourceAllocationChanged(this);
            }
            ASSERT(mailboxInfo->m_parentLayerBridge.get() == this);
            mailboxInfo->m_parentLayerBridge.clear();
            return;
        }
    }
}

blink::WebLayer* Canvas2DLayerBridge::layer() const
{
    ASSERT(!m_destructionInProgress);
    ASSERT(m_layer);
    return m_layer->layer();
}

void Canvas2DLayerBridge::willUse()
{
    ASSERT(!m_destructionInProgress);
    Canvas2DLayerManager::get().layerDidDraw(this);
    m_didRecordDrawCommand = true;
}

Platform3DObject Canvas2DLayerBridge::getBackingTexture()
{
    ASSERT(!m_destructionInProgress);
    if (!checkSurfaceValid())
        return 0;
    willUse();
    m_canvas->flush();
    context()->flush();
    GrRenderTarget* renderTarget = m_canvas->getTopDevice()->accessRenderTarget();
    if (renderTarget) {
        return renderTarget->asTexture()->getTextureHandle();
    }
    return 0;
}

Canvas2DLayerBridge::MailboxInfo::MailboxInfo(const MailboxInfo& other) {
    // This copy constructor should only be used for Vector reallocation
    // Assuming 'other' is to be destroyed, we transfer m_image ownership
    // rather than do a refcount dance.
    memcpy(&m_mailbox, &other.m_mailbox, sizeof(m_mailbox));
    m_image = const_cast<MailboxInfo*>(&other)->m_image.release();
    m_status = other.m_status;
}

}
