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

#include "platform/graphics/Canvas2DLayerBridge.h"

#include "base/memory/ptr_util.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/SharedContextRateLimiter.h"
#include "platform/tracing/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTraceLocation.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace {
enum {
  InvalidMailboxIndex = -1,
  MaxCanvasAnimationBacklog = 2,  // Make sure the the GPU is never more than
                                  // two animation frames behind.
};
}  // namespace

namespace blink {

#if USE_IOSURFACE_FOR_2D_CANVAS
struct Canvas2DLayerBridge::ImageInfo : public RefCounted<ImageInfo> {
  ImageInfo(std::unique_ptr<gfx::GpuMemoryBuffer>,
            GLuint imageId,
            GLuint textureId);
  ~ImageInfo();

  // The backing buffer.
  std::unique_ptr<gfx::GpuMemoryBuffer> m_gpuMemoryBuffer;

  // The id of the CHROMIUM image.
  const GLuint m_imageId;

  // The id of the texture bound to the CHROMIUM image.
  const GLuint m_textureId;
};
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

static sk_sp<SkSurface> createSkSurface(GrContext* gr,
                                        const IntSize& size,
                                        int msaaSampleCount,
                                        OpacityMode opacityMode,
                                        sk_sp<SkColorSpace> colorSpace,
                                        SkColorType colorType,
                                        bool* surfaceIsAccelerated) {
  if (gr)
    gr->resetContext();

  SkAlphaType alphaType =
      (Opaque == opacityMode) ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), colorType,
                                       alphaType, colorSpace);
  SkSurfaceProps disableLCDProps(0, kUnknown_SkPixelGeometry);
  sk_sp<SkSurface> surface;

  if (gr) {
    *surfaceIsAccelerated = true;
    surface = SkSurface::MakeRenderTarget(
        gr, SkBudgeted::kNo, info, msaaSampleCount,
        Opaque == opacityMode ? 0 : &disableLCDProps);
  }

  if (!surface) {
    *surfaceIsAccelerated = false;
    surface = SkSurface::MakeRaster(
        info, Opaque == opacityMode ? 0 : &disableLCDProps);
  }

  if (surface) {
    if (opacityMode == Opaque) {
      surface->getCanvas()->clear(SK_ColorBLACK);
    } else {
      surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    }
  }
  return surface;
}

Canvas2DLayerBridge::Canvas2DLayerBridge(
    std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
    const IntSize& size,
    int msaaSampleCount,
    OpacityMode opacityMode,
    AccelerationMode accelerationMode,
    sk_sp<SkColorSpace> colorSpace,
    SkColorType colorType)
    : m_contextProvider(std::move(contextProvider)),
      m_logger(wrapUnique(new Logger)),
      m_weakPtrFactory(this),
      m_imageBuffer(0),
      m_msaaSampleCount(msaaSampleCount),
      m_bytesAllocated(0),
      m_haveRecordedDrawCommands(false),
      m_destructionInProgress(false),
      m_filterQuality(kLow_SkFilterQuality),
      m_isHidden(false),
      m_isDeferralEnabled(true),
      m_isRegisteredTaskObserver(false),
      m_renderingTaskCompletedForCurrentFrame(false),
      m_softwareRenderingWhileHidden(false),
      m_lastImageId(0),
      m_lastFilter(GL_LINEAR),
      m_accelerationMode(accelerationMode),
      m_opacityMode(opacityMode),
      m_size(size),
      m_colorSpace(colorSpace),
      m_colorType(colorType) {
  DCHECK(m_contextProvider);
  DCHECK(!m_contextProvider->isSoftwareRendering());
  // Used by browser tests to detect the use of a Canvas2DLayerBridge.
  TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
  startRecording();
}

Canvas2DLayerBridge::~Canvas2DLayerBridge() {
  DCHECK(m_destructionInProgress);
#if USE_IOSURFACE_FOR_2D_CANVAS
  clearCHROMIUMImageCache();
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

  m_layer.reset();
  DCHECK_EQ(0u, m_mailboxes.size());
}

void Canvas2DLayerBridge::startRecording() {
  DCHECK(m_isDeferralEnabled);
  m_recorder = wrapUnique(new SkPictureRecorder);
  SkCanvas* canvas =
      m_recorder->beginRecording(m_size.width(), m_size.height(), nullptr);
  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas->save();

  if (m_imageBuffer) {
    m_imageBuffer->resetCanvas(canvas);
  }
  m_recordingPixelCount = 0;
}

void Canvas2DLayerBridge::setLoggerForTesting(std::unique_ptr<Logger> logger) {
  m_logger = std::move(logger);
}

bool Canvas2DLayerBridge::shouldAccelerate(AccelerationHint hint) const {
  bool accelerate;
  if (m_softwareRenderingWhileHidden)
    accelerate = false;
  else if (m_accelerationMode == ForceAccelerationForTesting)
    accelerate = true;
  else if (m_accelerationMode == DisableAcceleration)
    accelerate = false;
  else
    accelerate = hint == PreferAcceleration ||
                 hint == PreferAccelerationAfterVisibilityChange;

  if (accelerate &&
      (!m_contextProvider ||
       m_contextProvider->contextGL()->GetGraphicsResetStatusKHR() !=
           GL_NO_ERROR))
    accelerate = false;
  return accelerate;
}

bool Canvas2DLayerBridge::isAccelerated() const {
  if (m_accelerationMode == DisableAcceleration)
    return false;
  if (isHibernating())
    return false;
  if (m_softwareRenderingWhileHidden)
    return false;
  if (m_layer) {
    // We don't check |m_surface|, so this returns true if context was lost
    // (|m_surface| is null) with restoration pending.
    return true;
  }
  if (m_surface)  // && !m_layer is implied
    return false;

  // Whether or not to accelerate is not yet resolved. Determine whether
  // immediate presentation of the canvas would result in the canvas being
  // accelerated. Presentation is assumed to be a 'PreferAcceleration'
  // operation.
  return shouldAccelerate(PreferAcceleration);
}

GLenum Canvas2DLayerBridge::getGLFilter() {
  return m_filterQuality == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
}

#if USE_IOSURFACE_FOR_2D_CANVAS
bool Canvas2DLayerBridge::prepareIOSurfaceMailboxFromImage(
    SkImage* image,
    cc::TextureMailbox* outMailbox) {
  // Need to flush skia's internal queue, because the texture is about to be
  // accessed directly.
  GrContext* grContext = m_contextProvider->grContext();
  grContext->flush();

  RefPtr<ImageInfo> imageInfo = createIOSurfaceBackedTexture();
  if (!imageInfo)
    return false;

  gpu::gles2::GLES2Interface* gl = contextGL();
  if (!gl)
    return false;

  GLuint imageTexture =
      skia::GrBackendObjectToGrGLTextureInfo(image->getTextureHandle(true))
          ->fID;
  gl->CopySubTextureCHROMIUM(imageTexture, imageInfo->m_textureId, 0, 0, 0, 0,
                             m_size.width(), m_size.height(), GL_FALSE,
                             GL_FALSE, GL_FALSE);

  MailboxInfo& info = m_mailboxes.first();
  uint32_t textureTarget = GC3D_TEXTURE_RECTANGLE_ARB;
  gpu::Mailbox mailbox;
  gl->GenMailboxCHROMIUM(mailbox.name);
  gl->ProduceTextureDirectCHROMIUM(imageInfo->m_textureId, textureTarget,
                                   mailbox.name);

  const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();
  gl->Flush();
  gpu::SyncToken syncToken;
  gl->GenSyncTokenCHROMIUM(fenceSync, syncToken.GetData());

  info.m_imageInfo = imageInfo;
  bool isOverlayCandidate = true;
  bool secureOutputOnly = false;
  info.m_mailbox = mailbox;

  *outMailbox =
      cc::TextureMailbox(mailbox, syncToken, textureTarget, gfx::Size(m_size),
                         isOverlayCandidate, secureOutputOnly);
  if (RuntimeEnabledFeatures::colorCorrectRenderingEnabled()) {
    gfx::ColorSpace colorSpace =
        gfx::ColorSpace::FromSkColorSpace(m_colorSpace);
    outMailbox->set_color_space(colorSpace);
    imageInfo->m_gpuMemoryBuffer->SetColorSpaceForScanout(colorSpace);
  }

  gl->BindTexture(GC3D_TEXTURE_RECTANGLE_ARB, 0);

  // Because we are changing the texture binding without going through skia,
  // we must dirty the context.
  grContext->resetContext(kTextureBinding_GrGLBackendState);

  return true;
}

RefPtr<Canvas2DLayerBridge::ImageInfo>
Canvas2DLayerBridge::createIOSurfaceBackedTexture() {
  if (!m_imageInfoCache.isEmpty()) {
    RefPtr<Canvas2DLayerBridge::ImageInfo> info = m_imageInfoCache.last();
    m_imageInfoCache.pop_back();
    return info;
  }

  gpu::gles2::GLES2Interface* gl = contextGL();
  if (!gl)
    return nullptr;

  gpu::GpuMemoryBufferManager* gpuMemoryBufferManager =
      Platform::current()->getGpuMemoryBufferManager();
  if (!gpuMemoryBufferManager)
    return nullptr;

  std::unique_ptr<gfx::GpuMemoryBuffer> gpuMemoryBuffer =
      gpuMemoryBufferManager->AllocateGpuMemoryBuffer(
          gfx::Size(m_size), gfx::BufferFormat::RGBA_8888,
          gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle);
  if (!gpuMemoryBuffer)
    return nullptr;

  GLuint imageId =
      gl->CreateImageCHROMIUM(gpuMemoryBuffer->AsClientBuffer(), m_size.width(),
                              m_size.height(), GL_RGBA);
  if (!imageId)
    return nullptr;

  GLuint textureId;
  gl->GenTextures(1, &textureId);
  GLenum target = GC3D_TEXTURE_RECTANGLE_ARB;
  gl->BindTexture(target, textureId);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, getGLFilter());
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, getGLFilter());
  gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->BindTexImage2DCHROMIUM(target, imageId);

  return adoptRef(new Canvas2DLayerBridge::ImageInfo(std::move(gpuMemoryBuffer),
                                                     imageId, textureId));
}

void Canvas2DLayerBridge::deleteCHROMIUMImage(RefPtr<ImageInfo> info) {
  gpu::gles2::GLES2Interface* gl = contextGL();
  if (!gl)
    return;

  GLenum target = GC3D_TEXTURE_RECTANGLE_ARB;
  gl->BindTexture(target, info->m_textureId);
  gl->ReleaseTexImage2DCHROMIUM(target, info->m_imageId);
  gl->DestroyImageCHROMIUM(info->m_imageId);
  gl->DeleteTextures(1, &info->m_textureId);
  gl->BindTexture(target, 0);
  info->m_gpuMemoryBuffer.reset();

  resetSkiaTextureBinding();
}

void Canvas2DLayerBridge::clearCHROMIUMImageCache() {
  for (const auto& it : m_imageInfoCache) {
    deleteCHROMIUMImage(it);
  }
  m_imageInfoCache.clear();
}
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

void Canvas2DLayerBridge::createMailboxInfo() {
  MailboxInfo tmp;
  tmp.m_parentLayerBridge = this;
  m_mailboxes.prepend(tmp);
}

bool Canvas2DLayerBridge::prepareMailboxFromImage(
    sk_sp<SkImage> image,
    cc::TextureMailbox* outMailbox) {
  createMailboxInfo();
  MailboxInfo& mailboxInfo = m_mailboxes.first();

  GrContext* grContext = m_contextProvider->grContext();
  if (!grContext) {
    mailboxInfo.m_image = std::move(image);
    // For testing, skip GL stuff when using a mock graphics context.
    return true;
  }

#if USE_IOSURFACE_FOR_2D_CANVAS
  if (RuntimeEnabledFeatures::canvas2dImageChromiumEnabled()) {
    if (prepareIOSurfaceMailboxFromImage(image.get(), outMailbox))
      return true;
    // Note: if IOSurface backed texture creation failed we fall back to the
    // non-IOSurface path.
  }
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

  mailboxInfo.m_image = std::move(image);

  if (RuntimeEnabledFeatures::forceDisable2dCanvasCopyOnWriteEnabled())
    m_surface->notifyContentWillChange(SkSurface::kRetain_ContentChangeMode);

  // Need to flush skia's internal queue, because the texture is about to be
  // accessed directly.
  grContext->flush();

  // Because of texture sharing with the compositor, we must invalidate
  // the state cached in skia so that the deferred copy on write
  // in SkSurface_Gpu does not make any false assumptions.
  mailboxInfo.m_image->getTexture()->textureParamsModified();

  gpu::gles2::GLES2Interface* gl = contextGL();
  if (!gl)
    return false;

  GLuint textureID = skia::GrBackendObjectToGrGLTextureInfo(
                         mailboxInfo.m_image->getTextureHandle(true))
                         ->fID;
  gl->BindTexture(GL_TEXTURE_2D, textureID);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, getGLFilter());
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, getGLFilter());
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gpu::Mailbox mailbox;
  gl->GenMailboxCHROMIUM(mailbox.name);
  gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);

  gpu::SyncToken syncToken;
  if (isHidden()) {
    // With hidden canvases, we release the SkImage immediately because
    // there is no need for animations to be double buffered.
    mailboxInfo.m_image.reset();
  } else {
    // FIXME: We'd rather insert a syncpoint than perform a flush here,
    // but currently the canvas will flicker if we don't flush here.
    const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();
    gl->Flush();
    gl->GenSyncTokenCHROMIUM(fenceSync, syncToken.GetData());
  }
  mailboxInfo.m_mailbox = mailbox;
  *outMailbox = cc::TextureMailbox(mailbox, syncToken, GL_TEXTURE_2D);

  gl->BindTexture(GL_TEXTURE_2D, 0);
  // Because we are changing the texture binding without going through skia,
  // we must dirty the context.
  grContext->resetContext(kTextureBinding_GrGLBackendState);
  return true;
}

void Canvas2DLayerBridge::resetSkiaTextureBinding() {
  GrContext* grContext = m_contextProvider->grContext();
  if (grContext)
    grContext->resetContext(kTextureBinding_GrGLBackendState);
}

static void hibernateWrapper(WeakPtr<Canvas2DLayerBridge> bridge,
                             double /*idleDeadline*/) {
  if (bridge) {
    bridge->hibernate();
  } else {
    Canvas2DLayerBridge::Logger localLogger;
    localLogger.reportHibernationEvent(
        Canvas2DLayerBridge::
            HibernationAbortedDueToDestructionWhileHibernatePending);
  }
}

static void hibernateWrapperForTesting(WeakPtr<Canvas2DLayerBridge> bridge) {
  hibernateWrapper(bridge, 0);
}

void Canvas2DLayerBridge::hibernate() {
  DCHECK(!isHibernating());
  DCHECK(m_hibernationScheduled);

  m_hibernationScheduled = false;

  if (m_destructionInProgress) {
    m_logger->reportHibernationEvent(HibernationAbortedDueToPendingDestruction);
    return;
  }

  if (!m_surface) {
    m_logger->reportHibernationEvent(HibernationAbortedBecauseNoSurface);
    return;
  }

  if (!isHidden()) {
    m_logger->reportHibernationEvent(HibernationAbortedDueToVisibilityChange);
    return;
  }

  if (!checkSurfaceValid()) {
    m_logger->reportHibernationEvent(HibernationAbortedDueGpuContextLoss);
    return;
  }

  if (!isAccelerated()) {
    m_logger->reportHibernationEvent(
        HibernationAbortedDueToSwitchToUnacceleratedRendering);
    return;
  }

  TRACE_EVENT0("cc", "Canvas2DLayerBridge::hibernate");
  sk_sp<SkSurface> tempHibernationSurface =
      SkSurface::MakeRasterN32Premul(m_size.width(), m_size.height());
  if (!tempHibernationSurface) {
    m_logger->reportHibernationEvent(HibernationAbortedDueToAllocationFailure);
    return;
  }
  // No HibernationEvent reported on success. This is on purppose to avoid
  // non-complementary stats. Each HibernationScheduled event is paired with
  // exactly one failure or exit event.
  flushRecordingOnly();
  // The following checks that the flush succeeded, which should always be the
  // case because flushRecordingOnly should only fail it it fails to allocate
  // a surface, and we have an early exit at the top of this function for when
  // 'this' does not already have a surface.
  DCHECK(!m_haveRecordedDrawCommands);
  SkPaint copyPaint;
  copyPaint.setBlendMode(SkBlendMode::kSrc);
  m_surface->draw(tempHibernationSurface->getCanvas(), 0, 0,
                  &copyPaint);  // GPU readback
  m_hibernationImage = tempHibernationSurface->makeImageSnapshot();
  m_surface.reset();  // destroy the GPU-backed buffer
  m_layer->clearTexture();
#if USE_IOSURFACE_FOR_2D_CANVAS
  clearCHROMIUMImageCache();
#endif  // USE_IOSURFACE_FOR_2D_CANVAS
  m_logger->didStartHibernating();
}

void Canvas2DLayerBridge::reportSurfaceCreationFailure() {
  if (!m_surfaceCreationFailedAtLeastOnce) {
    // Only count the failure once per instance so that the histogram may
    // reflect the proportion of Canvas2DLayerBridge instances with surface
    // allocation failures.
    CanvasMetrics::countCanvasContextUsage(
        CanvasMetrics::GPUAccelerated2DCanvasSurfaceCreationFailed);
    m_surfaceCreationFailedAtLeastOnce = true;
  }
}

SkSurface* Canvas2DLayerBridge::getOrCreateSurface(AccelerationHint hint) {
  if (m_surface)
    return m_surface.get();

  if (m_layer && !isHibernating() && hint == PreferAcceleration &&
      m_accelerationMode != DisableAcceleration) {
    return nullptr;  // re-creation will happen through restore()
  }

  bool wantAcceleration = shouldAccelerate(hint);
  if (CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU && isHidden() &&
      wantAcceleration) {
    wantAcceleration = false;
    m_softwareRenderingWhileHidden = true;
  }

  bool surfaceIsAccelerated;
  m_surface = createSkSurface(
      wantAcceleration ? m_contextProvider->grContext() : nullptr, m_size,
      m_msaaSampleCount, m_opacityMode, m_colorSpace, m_colorType,
      &surfaceIsAccelerated);

  if (m_surface) {
    // Always save an initial frame, to support resetting the top level matrix
    // and clip.
    m_surface->getCanvas()->save();
  } else {
    reportSurfaceCreationFailure();
  }

  if (m_surface && surfaceIsAccelerated && !m_layer) {
    m_layer =
        Platform::current()->compositorSupport()->createExternalTextureLayer(
            this);
    m_layer->setOpaque(m_opacityMode == Opaque);
    m_layer->setBlendBackgroundColor(m_opacityMode != Opaque);
    GraphicsLayer::registerContentsLayer(m_layer->layer());
    m_layer->setNearestNeighbor(m_filterQuality == kNone_SkFilterQuality);
  }

  if (m_surface && isHibernating()) {
    if (surfaceIsAccelerated) {
      m_logger->reportHibernationEvent(HibernationEndedNormally);
    } else {
      if (isHidden())
        m_logger->reportHibernationEvent(
            HibernationEndedWithSwitchToBackgroundRendering);
      else
        m_logger->reportHibernationEvent(HibernationEndedWithFallbackToSW);
    }

    SkPaint copyPaint;
    copyPaint.setBlendMode(SkBlendMode::kSrc);
    m_surface->getCanvas()->drawImage(m_hibernationImage.get(), 0, 0,
                                      &copyPaint);
    m_hibernationImage.reset();

    if (m_imageBuffer)
      m_imageBuffer->updateGPUMemoryUsage();

    if (m_imageBuffer && !m_isDeferralEnabled)
      m_imageBuffer->resetCanvas(m_surface->getCanvas());
  }

  return m_surface.get();
}

SkCanvas* Canvas2DLayerBridge::canvas() {
  if (!m_isDeferralEnabled) {
    SkSurface* s = getOrCreateSurface();
    return s ? s->getCanvas() : nullptr;
  }
  return m_recorder->getRecordingCanvas();
}

void Canvas2DLayerBridge::disableDeferral(DisableDeferralReason reason) {
  // Disabling deferral is permanent: once triggered by disableDeferral()
  // we stay in immediate mode indefinitely. This is a performance heuristic
  // that significantly helps a number of use cases. The rationale is that if
  // immediate rendering was needed once, it is likely to be needed at least
  // once per frame, which eliminates the possibility for inter-frame
  // overdraw optimization. Furthermore, in cases where immediate mode is
  // required multiple times per frame, the repeated flushing of deferred
  // commands would cause significant overhead, so it is better to just stop
  // trying to defer altogether.
  if (!m_isDeferralEnabled)
    return;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, gpuDisabledHistogram,
                      ("Canvas.GPUAccelerated2DCanvasDisableDeferralReason",
                       DisableDeferralReasonCount));
  gpuDisabledHistogram.count(reason);
  CanvasMetrics::countCanvasContextUsage(
      CanvasMetrics::GPUAccelerated2DCanvasDeferralDisabled);
  flushRecordingOnly();
  // Because we will be discarding the recorder, if the flush failed
  // content will be lost -> force m_haveRecordedDrawCommands to false
  m_haveRecordedDrawCommands = false;

  m_isDeferralEnabled = false;
  m_recorder.reset();
  // install the current matrix/clip stack onto the immediate canvas
  SkSurface* surface = getOrCreateSurface();
  if (m_imageBuffer && surface)
    m_imageBuffer->resetCanvas(surface->getCanvas());
}

void Canvas2DLayerBridge::setImageBuffer(ImageBuffer* imageBuffer) {
  m_imageBuffer = imageBuffer;
  if (m_imageBuffer && m_isDeferralEnabled) {
    m_imageBuffer->resetCanvas(m_recorder->getRecordingCanvas());
  }
}

void Canvas2DLayerBridge::beginDestruction() {
  if (m_destructionInProgress)
    return;
  if (isHibernating())
    m_logger->reportHibernationEvent(HibernationEndedWithTeardown);
  m_hibernationImage.reset();
  m_recorder.reset();
  m_imageBuffer = nullptr;
  m_destructionInProgress = true;
  setIsHidden(true);
  m_surface.reset();

  unregisterTaskObserver();

  if (m_layer && m_accelerationMode != DisableAcceleration) {
    GraphicsLayer::unregisterContentsLayer(m_layer->layer());
    m_layer->clearTexture();
    // Orphaning the layer is required to trigger the recration of a new layer
    // in the case where destruction is caused by a canvas resize. Test:
    // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
    m_layer->layer()->removeFromParent();
  }

  DCHECK(!m_bytesAllocated);
}

void Canvas2DLayerBridge::unregisterTaskObserver() {
  if (m_isRegisteredTaskObserver) {
    Platform::current()->currentThread()->removeTaskObserver(this);
    m_isRegisteredTaskObserver = false;
  }
}

void Canvas2DLayerBridge::setFilterQuality(SkFilterQuality filterQuality) {
  DCHECK(!m_destructionInProgress);
  m_filterQuality = filterQuality;
  if (m_layer)
    m_layer->setNearestNeighbor(m_filterQuality == kNone_SkFilterQuality);
}

void Canvas2DLayerBridge::setIsHidden(bool hidden) {
  bool newHiddenValue = hidden || m_destructionInProgress;
  if (m_isHidden == newHiddenValue)
    return;

  m_isHidden = newHiddenValue;
  if (CANVAS2D_HIBERNATION_ENABLED && m_surface && isHidden() &&
      !m_destructionInProgress && !m_hibernationScheduled) {
    if (m_layer)
      m_layer->clearTexture();
    m_logger->reportHibernationEvent(HibernationScheduled);
    m_hibernationScheduled = true;
    if (m_dontUseIdleSchedulingForTesting) {
      Platform::current()->currentThread()->getWebTaskRunner()->postTask(
          BLINK_FROM_HERE, WTF::bind(&hibernateWrapperForTesting,
                                     m_weakPtrFactory.createWeakPtr()));
    } else {
      Platform::current()->currentThread()->scheduler()->postIdleTask(
          BLINK_FROM_HERE,
          WTF::bind(&hibernateWrapper, m_weakPtrFactory.createWeakPtr()));
    }
  }
  if (!isHidden() && m_softwareRenderingWhileHidden) {
    flushRecordingOnly();
    SkPaint copyPaint;
    copyPaint.setBlendMode(SkBlendMode::kSrc);

    sk_sp<SkSurface> oldSurface = std::move(m_surface);
    m_surface.reset();

    m_softwareRenderingWhileHidden = false;
    SkSurface* newSurface =
        getOrCreateSurface(PreferAccelerationAfterVisibilityChange);
    if (newSurface) {
      if (oldSurface)
        oldSurface->draw(newSurface->getCanvas(), 0, 0, &copyPaint);
      if (m_imageBuffer && !m_isDeferralEnabled) {
        m_imageBuffer->resetCanvas(m_surface->getCanvas());
      }
    }
  }
  if (!isHidden() && isHibernating()) {
    getOrCreateSurface();  // Rude awakening
  }
}

bool Canvas2DLayerBridge::writePixels(const SkImageInfo& origInfo,
                                      const void* pixels,
                                      size_t rowBytes,
                                      int x,
                                      int y) {
  if (!getOrCreateSurface())
    return false;
  if (x <= 0 && y <= 0 && x + origInfo.width() >= m_size.width() &&
      y + origInfo.height() >= m_size.height()) {
    skipQueuedDrawCommands();
  } else {
    flush();
  }
  DCHECK(!m_haveRecordedDrawCommands);
  // call write pixels on the surface, not the recording canvas.
  // No need to call beginDirectSurfaceAccessModeIfNeeded() because writePixels
  // ignores the matrix and clip state.
  return getOrCreateSurface()->getCanvas()->writePixels(origInfo, pixels,
                                                        rowBytes, x, y);
}

void Canvas2DLayerBridge::skipQueuedDrawCommands() {
  if (m_haveRecordedDrawCommands) {
    m_recorder->finishRecordingAsPicture();
    startRecording();
    m_haveRecordedDrawCommands = false;
  }

  if (m_isDeferralEnabled) {
    unregisterTaskObserver();
    if (m_rateLimiter)
      m_rateLimiter->reset();
  }
}

void Canvas2DLayerBridge::flushRecordingOnly() {
  DCHECK(!m_destructionInProgress);

  if (m_haveRecordedDrawCommands && getOrCreateSurface()) {
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushRecordingOnly");
    m_recorder->finishRecordingAsPicture()->playback(
        getOrCreateSurface()->getCanvas());
    if (m_isDeferralEnabled)
      startRecording();
    m_haveRecordedDrawCommands = false;
  }
}

void Canvas2DLayerBridge::flush() {
  if (!getOrCreateSurface())
    return;
  TRACE_EVENT0("cc", "Canvas2DLayerBridge::flush");
  flushRecordingOnly();
  getOrCreateSurface()->getCanvas()->flush();
}

void Canvas2DLayerBridge::flushGpu() {
  TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushGpu");
  flush();
  gpu::gles2::GLES2Interface* gl = contextGL();
  if (isAccelerated() && gl)
    gl->Flush();
}

gpu::gles2::GLES2Interface* Canvas2DLayerBridge::contextGL() {
  // Check on m_layer is necessary because contextGL() may be called during
  // the destruction of m_layer
  if (m_layer && m_accelerationMode != DisableAcceleration &&
      !m_destructionInProgress) {
    // Call checkSurfaceValid to ensure the rate limiter is disabled if the
    // context is lost.
    if (!checkSurfaceValid())
      return nullptr;
  }
  return m_contextProvider ? m_contextProvider->contextGL() : nullptr;
}

bool Canvas2DLayerBridge::checkSurfaceValid() {
  DCHECK(!m_destructionInProgress);
  if (m_destructionInProgress)
    return false;
  if (isHibernating())
    return true;
  if (!m_layer || m_accelerationMode == DisableAcceleration)
    return true;
  if (!m_surface)
    return false;
  if (m_contextProvider->contextGL()->GetGraphicsResetStatusKHR() !=
      GL_NO_ERROR) {
    m_surface.reset();
    for (auto mailboxInfo = m_mailboxes.begin();
         mailboxInfo != m_mailboxes.end(); ++mailboxInfo) {
      if (mailboxInfo->m_image)
        mailboxInfo->m_image.reset();
    }
    if (m_imageBuffer)
      m_imageBuffer->notifySurfaceInvalid();
    CanvasMetrics::countCanvasContextUsage(
        CanvasMetrics::Accelerated2DCanvasGPUContextLost);
  }
  return m_surface.get();
}

bool Canvas2DLayerBridge::restoreSurface() {
  DCHECK(!m_destructionInProgress);
  if (m_destructionInProgress)
    return false;
  DCHECK(isAccelerated() && !m_surface);

  gpu::gles2::GLES2Interface* sharedGL = nullptr;
  m_layer->clearTexture();
  m_contextProvider = wrapUnique(
      Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
  if (m_contextProvider)
    sharedGL = m_contextProvider->contextGL();

  if (sharedGL && sharedGL->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    GrContext* grCtx = m_contextProvider->grContext();
    bool surfaceIsAccelerated;
    sk_sp<SkSurface> surface(
        createSkSurface(grCtx, m_size, m_msaaSampleCount, m_opacityMode,
                        m_colorSpace, m_colorType, &surfaceIsAccelerated));

    if (!m_surface)
      reportSurfaceCreationFailure();

    // The current paradigm does not support switching from accelerated to
    // non-accelerated, which would be tricky due to changes to the layer tree,
    // which can only happen at specific times during the document lifecycle.
    // Therefore, we can only accept the restored surface if it is accelerated.
    if (surface && surfaceIsAccelerated) {
      m_surface = std::move(surface);
      // FIXME: draw sad canvas picture into new buffer crbug.com/243842
    }
  }
  if (m_imageBuffer)
    m_imageBuffer->updateGPUMemoryUsage();

  return m_surface.get();
}

static gfx::ColorSpace SkColorSpaceToColorSpace(
    const SkColorSpace* skColorSpace) {
  // TODO(crbug.com/634102): Eliminate this clumsy conversion by unifying
  // SkColorSpace and gfx::ColorSpace.
  if (!skColorSpace)
    return gfx::ColorSpace();

  gfx::ColorSpace::TransferID transferID =
      gfx::ColorSpace::TransferID::UNSPECIFIED;
  if (skColorSpace->gammaCloseToSRGB()) {
    transferID = gfx::ColorSpace::TransferID::IEC61966_2_1;
  } else if (skColorSpace->gammaIsLinear()) {
    transferID = gfx::ColorSpace::TransferID::LINEAR;
  } else {
    // TODO(crbug.com/634102): Not all curve type are supported
    DCHECK(false);
  }

  // TODO(crbug.com/634102): No primary conversions are performed.
  // Rec-709 is assumed.
  return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709, transferID,
                         gfx::ColorSpace::MatrixID::RGB,
                         gfx::ColorSpace::RangeID::FULL);
}

bool Canvas2DLayerBridge::PrepareTextureMailbox(
    cc::TextureMailbox* outMailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* outReleaseCallback) {
  if (m_destructionInProgress) {
    // It can be hit in the following sequence.
    // 1. Canvas draws something.
    // 2. The compositor begins the frame.
    // 3. Javascript makes a context be lost.
    // 4. Here.
    return false;
  }
  DCHECK(isAccelerated() || isHibernating() || m_softwareRenderingWhileHidden);

  // if hibernating but not hidden, we want to wake up from
  // hibernation
  if ((isHibernating() || m_softwareRenderingWhileHidden) && isHidden())
    return false;

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (m_contextProvider->contextGL()->GetGraphicsResetStatusKHR() !=
      GL_NO_ERROR)
    return false;

  sk_sp<SkImage> image =
      newImageSnapshot(PreferAcceleration, SnapshotReasonUnknown);
  if (!image || !image->getTexture())
    return false;

  // Early exit if canvas was not drawn to since last prepareMailbox.
  GLenum filter = getGLFilter();
  if (image->uniqueID() == m_lastImageId && filter == m_lastFilter)
    return false;
  m_lastImageId = image->uniqueID();
  m_lastFilter = filter;

  if (!prepareMailboxFromImage(std::move(image), outMailbox))
    return false;
  outMailbox->set_nearest_neighbor(getGLFilter() == GL_NEAREST);
  gfx::ColorSpace colorSpace = SkColorSpaceToColorSpace(m_colorSpace.get());
  outMailbox->set_color_space(colorSpace);

  auto func =
      WTF::bind(&Canvas2DLayerBridge::mailboxReleased,
                m_weakPtrFactory.createWeakPtr(), outMailbox->mailbox());
  *outReleaseCallback =
      cc::SingleReleaseCallback::Create(convertToBaseCallback(std::move(func)));
  return true;
}

void Canvas2DLayerBridge::mailboxReleased(const gpu::Mailbox& mailbox,
                                          const gpu::SyncToken& syncToken,
                                          bool lostResource) {
  DCHECK(isAccelerated() || isHibernating());
  bool contextLost =
      !isHibernating() &&
      (!m_surface ||
       m_contextProvider->contextGL()->GetGraphicsResetStatusKHR() !=
           GL_NO_ERROR);
  DCHECK(m_mailboxes.last().m_parentLayerBridge.get() == this);

  // Mailboxes are typically released in FIFO order, so we iterate
  // from the end of m_mailboxes.
  auto releasedMailboxInfo = m_mailboxes.end();
  auto firstMailbox = m_mailboxes.begin();

  while (true) {
    --releasedMailboxInfo;
    if (releasedMailboxInfo->m_mailbox == mailbox)
      break;
    DCHECK(releasedMailboxInfo != firstMailbox);
  }

#if USE_IOSURFACE_FOR_2D_CANVAS
  if (releasedMailboxInfo->m_imageInfo && !lostResource) {
    if (contextLost) {
      deleteCHROMIUMImage(releasedMailboxInfo->m_imageInfo);
    } else {
      m_imageInfoCache.append(releasedMailboxInfo->m_imageInfo);
    }
  }
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

  if (!contextLost) {
    // Invalidate texture state in case the compositor altered it since the
    // copy-on-write.
    if (releasedMailboxInfo->m_image) {
#if USE_IOSURFACE_FOR_2D_CANVAS
      DCHECK(!releasedMailboxInfo->m_imageInfo);
#endif  // USE_IOSURFACE_FOR_2D_CANVAS
      if (syncToken.HasData()) {
        contextGL()->WaitSyncTokenCHROMIUM(syncToken.GetConstData());
      }
      GrTexture* texture = releasedMailboxInfo->m_image->getTexture();
      if (texture) {
        if (lostResource) {
          texture->abandon();
        } else {
          texture->textureParamsModified();
          // Break the mailbox association to avoid leaking mailboxes every time
          // skia recycles a texture.
          gpu::gles2::GLES2Interface* gl = contextGL();
          if (gl)
            gl->ProduceTextureDirectCHROMIUM(
                0, GL_TEXTURE_2D, releasedMailboxInfo->m_mailbox.name);
        }
      }
    }
  }

  RefPtr<Canvas2DLayerBridge> selfRef;
  if (m_destructionInProgress) {
    // To avoid memory use after free, take a scoped self-reference
    // to postpone destruction until the end of this function.
    selfRef = this;
  }

  // The destruction of 'releasedMailboxInfo' will:
  // 1) Release the self reference held by the mailboxInfo, which may trigger
  //    the self-destruction of this Canvas2DLayerBridge
  // 2) Release the SkImage, which will return the texture to skia's scratch
  //    texture pool.
  m_mailboxes.remove(releasedMailboxInfo);

  if (m_mailboxes.isEmpty() && m_accelerationMode == DisableAcceleration)
    m_layer.reset();
}

WebLayer* Canvas2DLayerBridge::layer() const {
  DCHECK(!m_destructionInProgress);
  DCHECK(m_layer);
  return m_layer->layer();
}

void Canvas2DLayerBridge::didDraw(const FloatRect& rect) {
  if (m_isDeferralEnabled) {
    m_haveRecordedDrawCommands = true;
    IntRect pixelBounds = enclosingIntRect(rect);
    m_recordingPixelCount += pixelBounds.width() * pixelBounds.height();
    if (m_recordingPixelCount >=
        (m_size.width() * m_size.height() *
         ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold)) {
      disableDeferral(DisableDeferralReasonExpensiveOverdrawHeuristic);
    }
  }
  if (!m_isRegisteredTaskObserver) {
    Platform::current()->currentThread()->addTaskObserver(this);
    m_isRegisteredTaskObserver = true;
  }
}

void Canvas2DLayerBridge::prepareSurfaceForPaintingIfNeeded() {
  getOrCreateSurface(PreferAcceleration);
}

void Canvas2DLayerBridge::finalizeFrame(const FloatRect& dirtyRect) {
  DCHECK(!m_destructionInProgress);
  if (m_layer && m_accelerationMode != DisableAcceleration)
    m_layer->layer()->invalidateRect(enclosingIntRect(dirtyRect));
  if (m_rateLimiter)
    m_rateLimiter->reset();
  m_renderingTaskCompletedForCurrentFrame = false;
}

void Canvas2DLayerBridge::didProcessTask() {
  TRACE_EVENT0("cc", "Canvas2DLayerBridge::didProcessTask");
  DCHECK(m_isRegisteredTaskObserver);
  // If m_renderTaskProcessedForCurrentFrame is already set to true,
  // it means that rendering tasks are not synchronized with the compositor
  // (i.e. not using requestAnimationFrame), so we are at risk of posting
  // a multi-frame backlog to the GPU
  if (m_renderingTaskCompletedForCurrentFrame) {
    if (isAccelerated()) {
      flushGpu();
      if (!m_rateLimiter) {
        m_rateLimiter =
            SharedContextRateLimiter::create(MaxCanvasAnimationBacklog);
      }
    } else {
      flush();
    }
  }

  if (m_rateLimiter) {
    m_rateLimiter->tick();
  }

  m_renderingTaskCompletedForCurrentFrame = true;
  unregisterTaskObserver();
}

void Canvas2DLayerBridge::willProcessTask() {
  NOTREACHED();
}

sk_sp<SkImage> Canvas2DLayerBridge::newImageSnapshot(AccelerationHint hint,
                                                     SnapshotReason) {
  if (isHibernating())
    return m_hibernationImage;
  if (!checkSurfaceValid())
    return nullptr;
  if (!getOrCreateSurface(hint))
    return nullptr;
  flush();
  // A readback operation may alter the texture parameters, which may affect
  // the compositor's behavior. Therefore, we must trigger copy-on-write
  // even though we are not technically writing to the texture, only to its
  // parameters.
  getOrCreateSurface()->notifyContentWillChange(
      SkSurface::kRetain_ContentChangeMode);
  return m_surface->makeImageSnapshot();
}

void Canvas2DLayerBridge::willOverwriteCanvas() {
  skipQueuedDrawCommands();
}

#if USE_IOSURFACE_FOR_2D_CANVAS
Canvas2DLayerBridge::ImageInfo::ImageInfo(
    std::unique_ptr<gfx::GpuMemoryBuffer> gpuMemoryBuffer,
    GLuint imageId,
    GLuint textureId)
    : m_gpuMemoryBuffer(std::move(gpuMemoryBuffer)),
      m_imageId(imageId),
      m_textureId(textureId) {
  DCHECK(m_gpuMemoryBuffer);
  DCHECK(m_imageId);
  DCHECK(m_textureId);
}

Canvas2DLayerBridge::ImageInfo::~ImageInfo() {}
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

Canvas2DLayerBridge::MailboxInfo::MailboxInfo() = default;
Canvas2DLayerBridge::MailboxInfo::MailboxInfo(const MailboxInfo& other) =
    default;

void Canvas2DLayerBridge::Logger::reportHibernationEvent(
    HibernationEvent event) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, hibernationHistogram,
                      ("Canvas.HibernationEvents", HibernationEventCount));
  hibernationHistogram.count(event);
}

}  // namespace blink
