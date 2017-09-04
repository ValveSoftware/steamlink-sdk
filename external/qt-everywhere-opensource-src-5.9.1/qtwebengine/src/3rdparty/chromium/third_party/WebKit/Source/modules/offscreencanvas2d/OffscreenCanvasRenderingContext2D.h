// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasRenderingContext2D_h
#define OffscreenCanvasRenderingContext2D_h

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/canvas2d/BaseRenderingContext2D.h"
#include <memory>

namespace blink {

class MODULES_EXPORT OffscreenCanvasRenderingContext2D final
    : public CanvasRenderingContext,
      public BaseRenderingContext2D {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(OffscreenCanvasRenderingContext2D);

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() {}
    ~Factory() override {}

    CanvasRenderingContext* create(
        ScriptState* scriptState,
        OffscreenCanvas* canvas,
        const CanvasContextCreationAttributes& attrs) override {
      return new OffscreenCanvasRenderingContext2D(scriptState, canvas, attrs);
    }

    CanvasRenderingContext::ContextType getContextType() const override {
      return CanvasRenderingContext::Context2d;
    }
  };

  void commit(ScriptState*, ExceptionState&);

  // CanvasRenderingContext implementation
  ~OffscreenCanvasRenderingContext2D() override;
  ContextType getContextType() const override { return Context2d; }
  bool is2d() const override { return true; }
  void setOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;
  void setIsHidden(bool) final { ASSERT_NOT_REACHED(); }
  void stop() final { ASSERT_NOT_REACHED(); }
  void setCanvasGetContextResult(RenderingContext&) final {}
  void clearRect(double x, double y, double width, double height) override {
    BaseRenderingContext2D::clearRect(x, y, width, height);
  }
  PassRefPtr<Image> getImage(AccelerationHint, SnapshotReason) const final;
  ImageData* toImageData(SnapshotReason) const override;
  void reset() override;

  // BaseRenderingContext2D implementation
  bool originClean() const final;
  void setOriginTainted() final;
  bool wouldTaintOrigin(CanvasImageSource*, ExecutionContext*) final;

  int width() const final;
  int height() const final;

  bool hasImageBuffer() const final;
  ImageBuffer* imageBuffer() const final;

  bool parseColorOrCurrentColor(Color&, const String& colorString) const final;

  SkCanvas* drawingCanvas() const final;
  SkCanvas* existingDrawingCanvas() const final;
  void disableDeferral(DisableDeferralReason) final;

  AffineTransform baseTransform() const final;
  void didDraw(const SkIRect& dirtyRect) final;

  bool stateHasFilter() final;
  sk_sp<SkImageFilter> stateGetFilter() final;
  void snapshotStateForFilter() final {}

  void validateStateStack() const final;

  bool hasAlpha() const final { return creationAttributes().alpha(); }
  bool isContextLost() const override;

  ImageBitmap* transferToImageBitmap(ScriptState*) final;

 protected:
  OffscreenCanvasRenderingContext2D(
      ScriptState*,
      OffscreenCanvas*,
      const CanvasContextCreationAttributes& attrs);
  DECLARE_VIRTUAL_TRACE();

 private:
  bool m_needsMatrixClipRestore = false;
  std::unique_ptr<ImageBuffer> m_imageBuffer;

  bool isPaintable() const final;

  RefPtr<StaticBitmapImage> transferToStaticBitmapImage();
};

DEFINE_TYPE_CASTS(OffscreenCanvasRenderingContext2D,
                  CanvasRenderingContext,
                  context,
                  context->is2d() && context->getOffscreenCanvas(),
                  context.is2d() && context.getOffscreenCanvas());

}  // namespace blink

#endif  // OffscreenCanvasRenderingContext2D_h
