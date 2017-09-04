// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmapRenderingContext_h
#define ImageBitmapRenderingContext_h

#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/ModulesExport.h"
#include "wtf/RefPtr.h"

namespace blink {

class ImageBitmap;

class MODULES_EXPORT ImageBitmapRenderingContext final
    : public CanvasRenderingContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
    WTF_MAKE_NONCOPYABLE(Factory);

   public:
    Factory() {}
    ~Factory() override {}

    CanvasRenderingContext* create(HTMLCanvasElement*,
                                   const CanvasContextCreationAttributes&,
                                   Document&) override;
    CanvasRenderingContext::ContextType getContextType() const override {
      return CanvasRenderingContext::ContextImageBitmap;
    }
  };

  // Script API
  void transferFromImageBitmap(ImageBitmap*, ExceptionState&);

  // CanvasRenderingContext implementation
  ContextType getContextType() const override {
    return CanvasRenderingContext::ContextImageBitmap;
  }
  void setIsHidden(bool) override {}
  bool isContextLost() const override { return false; }
  bool paint(GraphicsContext&, const IntRect&) override;
  void setCanvasGetContextResult(RenderingContext&) final;
  PassRefPtr<Image> getImage(AccelerationHint, SnapshotReason) const final {
    return m_image.get();
  }

  // TODO(junov): Implement GPU accelerated rendering using a layer bridge
  WebLayer* platformLayer() const override { return nullptr; }
  // TODO(junov): handle lost contexts when content is GPU-backed
  void loseContext(LostContextMode) override {}

  void stop() override;

  bool isPaintable() const final { return m_image.get(); }

  virtual ~ImageBitmapRenderingContext();

 private:
  ImageBitmapRenderingContext(HTMLCanvasElement*,
                              const CanvasContextCreationAttributes&,
                              Document&);

  RefPtr<Image> m_image;
};

DEFINE_TYPE_CASTS(ImageBitmapRenderingContext,
                  CanvasRenderingContext,
                  context,
                  context->getContextType() ==
                      CanvasRenderingContext::ContextImageBitmap,
                  context.getContextType() ==
                      CanvasRenderingContext::ContextImageBitmap);

}  // blink

#endif
