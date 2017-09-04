/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef Canvas2DImageBufferSurface_h
#define Canvas2DImageBufferSurface_h

#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// This shim is necessary because ImageBufferSurfaces are not allowed to be
// RefCounted.
class Canvas2DImageBufferSurface final : public ImageBufferSurface {
 public:
  Canvas2DImageBufferSurface(
      std::unique_ptr<WebGraphicsContext3DProvider> contextProvider,
      const IntSize& size,
      int msaaSampleCount,
      OpacityMode opacityMode,
      Canvas2DLayerBridge::AccelerationMode accelerationMode,
      sk_sp<SkColorSpace> colorSpace,
      SkColorType colorType)
      : ImageBufferSurface(size, opacityMode, colorSpace, colorType),
        m_layerBridge(
            adoptRef(new Canvas2DLayerBridge(std::move(contextProvider),
                                             size,
                                             msaaSampleCount,
                                             opacityMode,
                                             accelerationMode,
                                             std::move(colorSpace),
                                             colorType))) {
    init();
  }

  Canvas2DImageBufferSurface(PassRefPtr<Canvas2DLayerBridge> bridge,
                             const IntSize& size)
      : ImageBufferSurface(size,
                           bridge->opacityMode(),
                           bridge->colorSpace(),
                           bridge->colorType()),
        m_layerBridge(std::move(bridge)) {
    init();
  }

  ~Canvas2DImageBufferSurface() override { m_layerBridge->beginDestruction(); }

  // ImageBufferSurface implementation
  void finalizeFrame(const FloatRect& dirtyRect) override {
    m_layerBridge->finalizeFrame(dirtyRect);
  }
  void willOverwriteCanvas() override { m_layerBridge->willOverwriteCanvas(); }
  SkCanvas* canvas() override { return m_layerBridge->canvas(); }
  void disableDeferral(DisableDeferralReason reason) override {
    m_layerBridge->disableDeferral(reason);
  }
  bool isValid() const override { return m_layerBridge->checkSurfaceValid(); }
  bool restore() override { return m_layerBridge->restoreSurface(); }
  WebLayer* layer() const override { return m_layerBridge->layer(); }
  bool isAccelerated() const override { return m_layerBridge->isAccelerated(); }
  void setFilterQuality(SkFilterQuality filterQuality) override {
    m_layerBridge->setFilterQuality(filterQuality);
  }
  void setIsHidden(bool hidden) override { m_layerBridge->setIsHidden(hidden); }
  void setImageBuffer(ImageBuffer* imageBuffer) override {
    m_layerBridge->setImageBuffer(imageBuffer);
  }
  void didDraw(const FloatRect& rect) override { m_layerBridge->didDraw(rect); }
  void flush(FlushReason) override { m_layerBridge->flush(); }
  void flushGpu(FlushReason) override { m_layerBridge->flushGpu(); }
  void prepareSurfaceForPaintingIfNeeded() override {
    m_layerBridge->prepareSurfaceForPaintingIfNeeded();
  }
  bool writePixels(const SkImageInfo& origInfo,
                   const void* pixels,
                   size_t rowBytes,
                   int x,
                   int y) override {
    return m_layerBridge->writePixels(origInfo, pixels, rowBytes, x, y);
  }

  sk_sp<SkImage> newImageSnapshot(AccelerationHint hint,
                                  SnapshotReason reason) override {
    return m_layerBridge->newImageSnapshot(hint, reason);
  }

 private:
  void init() {
    clear();
    if (isValid())
      m_layerBridge->flush();
  }

  RefPtr<Canvas2DLayerBridge> m_layerBridge;
};

}  // namespace blink

#endif
