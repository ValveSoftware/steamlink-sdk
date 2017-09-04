// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGFilterPainter_h
#define SVGFilterPainter_h

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/Allocator.h"
#include <memory>

namespace blink {

class FilterData;
class LayoutObject;
class LayoutSVGResourceFilter;

class SVGFilterRecordingContext {
  USING_FAST_MALLOC(SVGFilterRecordingContext);
  WTF_MAKE_NONCOPYABLE(SVGFilterRecordingContext);

 public:
  explicit SVGFilterRecordingContext(GraphicsContext& initialContext)
      : m_initialContext(initialContext) {}

  GraphicsContext* beginContent(FilterData*);
  void endContent(FilterData*);

  GraphicsContext& paintingContext() const { return m_initialContext; }

 private:
  std::unique_ptr<PaintController> m_paintController;
  std::unique_ptr<GraphicsContext> m_context;
  GraphicsContext& m_initialContext;
};

class SVGFilterPainter {
  STACK_ALLOCATED();

 public:
  SVGFilterPainter(LayoutSVGResourceFilter& filter) : m_filter(filter) {}

  // Returns the context that should be used to paint the filter contents, or
  // null if the content should not be recorded.
  GraphicsContext* prepareEffect(const LayoutObject&,
                                 SVGFilterRecordingContext&);
  void finishEffect(const LayoutObject&, SVGFilterRecordingContext&);

 private:
  LayoutSVGResourceFilter& m_filter;
};

}  // namespace blink

#endif  // SVGFilterPainter_h
