// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasRenderingContextFactory_h
#define CanvasRenderingContextFactory_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "wtf/Allocator.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class HTMLCanvasElement;
class OffscreenCanvas;

class CORE_EXPORT CanvasRenderingContextFactory {
  USING_FAST_MALLOC(CanvasRenderingContextFactory);
  WTF_MAKE_NONCOPYABLE(CanvasRenderingContextFactory);

 public:
  CanvasRenderingContextFactory() = default;
  virtual ~CanvasRenderingContextFactory() {}

  virtual CanvasRenderingContext* create(HTMLCanvasElement*,
                                         const CanvasContextCreationAttributes&,
                                         Document&) {
    return nullptr;
  }
  virtual CanvasRenderingContext* create(
      ScriptState*,
      OffscreenCanvas*,
      const CanvasContextCreationAttributes&) {
    return nullptr;
  }
  virtual CanvasRenderingContext::ContextType getContextType() const = 0;
  virtual void onError(HTMLCanvasElement*, const String& error){};
  virtual void onError(OffscreenCanvas*, const String& error){};
};

}  // namespace blink

#endif  // CanvasRenderingContextFactory_h
