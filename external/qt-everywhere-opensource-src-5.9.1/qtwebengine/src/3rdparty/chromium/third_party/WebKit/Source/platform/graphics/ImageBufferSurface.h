/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageBufferSurface_h
#define ImageBufferSurface_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

class SkCanvas;
class SkColorSpace;
class SkImage;
struct SkImageInfo;
class SkPicture;

namespace blink {

class ImageBuffer;
class WebLayer;
class FloatRect;
class GraphicsContext;

class PLATFORM_EXPORT ImageBufferSurface {
  WTF_MAKE_NONCOPYABLE(ImageBufferSurface);
  USING_FAST_MALLOC(ImageBufferSurface);

 public:
  virtual ~ImageBufferSurface();

  virtual SkCanvas* canvas() = 0;
  virtual void disableDeferral(DisableDeferralReason) {}
  virtual void willOverwriteCanvas() {}
  virtual void didDraw(const FloatRect& rect) {}
  virtual bool isValid() const = 0;
  virtual bool restore() { return false; }
  virtual WebLayer* layer() const { return 0; }
  virtual bool isAccelerated() const { return false; }
  virtual bool isRecording() const { return false; }
  virtual bool isExpensiveToPaint() { return false; }
  virtual void setFilterQuality(SkFilterQuality) {}
  virtual void setIsHidden(bool) {}
  virtual void setImageBuffer(ImageBuffer*) {}
  virtual sk_sp<SkPicture> getPicture();
  virtual void finalizeFrame(const FloatRect& dirtyRect) {}
  virtual void draw(GraphicsContext&,
                    const FloatRect& destRect,
                    const FloatRect& srcRect,
                    SkBlendMode);
  virtual void setHasExpensiveOp() {}
  virtual GLuint getBackingTextureHandleForOverwrite() { return 0; }

  // Executes all deferred rendering immediately.
  virtual void flush(FlushReason);

  // Like flush, but flushes all the way down to the GPU context if the surface
  // uses the GPU.
  virtual void flushGpu(FlushReason reason) { flush(reason); }

  virtual void prepareSurfaceForPaintingIfNeeded() {}
  virtual bool writePixels(const SkImageInfo& origInfo,
                           const void* pixels,
                           size_t rowBytes,
                           int x,
                           int y);

  // May return nullptr if the surface is GPU-backed and the GPU context was
  // lost.
  virtual sk_sp<SkImage> newImageSnapshot(AccelerationHint, SnapshotReason) = 0;

  OpacityMode getOpacityMode() const { return m_opacityMode; }
  const IntSize& size() const { return m_size; }
  const sk_sp<SkColorSpace> colorSpace() const { return m_colorSpace; }
  SkColorType colorType() const { return m_colorType; }
  void notifyIsValidChanged(bool isValid) const;

 protected:
  ImageBufferSurface(const IntSize&,
                     OpacityMode,
                     sk_sp<SkColorSpace>,
                     SkColorType);
  void clear();

 private:
  OpacityMode m_opacityMode;
  IntSize m_size;
  sk_sp<SkColorSpace> m_colorSpace;
  SkColorType m_colorType;
};

}  // namespace blink

#endif
