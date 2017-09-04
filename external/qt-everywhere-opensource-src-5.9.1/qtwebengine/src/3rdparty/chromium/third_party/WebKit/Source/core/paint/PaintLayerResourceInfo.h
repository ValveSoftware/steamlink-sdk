/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef PaintLayerResourceInfo_h
#define PaintLayerResourceInfo_h

#include "core/svg/SVGResourceClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class FilterEffect;
class PaintLayer;

// PaintLayerResourceInfo holds the filter information for painting
// https://drafts.fxtf.org/filters/. It also acts as the resource client for
// change notifications from <clipPath> elements for the clip-path property.
//
// Because PaintLayer is not allocated for SVG objects, SVG filters (both
// software and hardware-accelerated) use a different code path to paint the
// filters (SVGFilterPainter), but both code paths use the same abstraction for
// painting non-hardware accelerated filters (FilterEffect). Hardware
// accelerated CSS filters use CompositorFilterOperations, that is backed by cc.
class PaintLayerResourceInfo final
    : public GarbageCollectedFinalized<PaintLayerResourceInfo>,
      public SVGResourceClient {
  WTF_MAKE_NONCOPYABLE(PaintLayerResourceInfo);
  USING_GARBAGE_COLLECTED_MIXIN(PaintLayerResourceInfo);

 public:
  explicit PaintLayerResourceInfo(PaintLayer*);
  ~PaintLayerResourceInfo() override;

  void setLastEffect(FilterEffect*);
  FilterEffect* lastEffect() const;
  void invalidateFilterChain();

  void clearLayer() { m_layer = nullptr; }

  TreeScope* treeScope() override;

  void resourceContentChanged() override;
  void resourceElementChanged() override;

  DECLARE_TRACE();

 private:
  // |clearLayer| must be called before *m_layer becomes invalid.
  PaintLayer* m_layer;
  Member<FilterEffect> m_lastEffect;
};

}  // namespace blink

#endif  // PaintLayerResourceInfo_h
