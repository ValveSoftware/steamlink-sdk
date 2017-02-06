// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_LAYER_IMPL_H_
#define CC_BLINK_WEB_LAYER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/layers/layer_client.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebDoublePoint.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace blink {
struct WebFloatRect;
}

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {
class FilterOperations;
class Layer;
}

namespace cc_blink {

class WebLayerImpl : public blink::WebLayer {
 public:
  CC_BLINK_EXPORT WebLayerImpl();
  CC_BLINK_EXPORT explicit WebLayerImpl(scoped_refptr<cc::Layer>);
  ~WebLayerImpl() override;

  CC_BLINK_EXPORT cc::Layer* layer() const;

  // If set to true, content opaqueness cannot be changed using setOpaque.
  // However, it can still be modified using SetContentsOpaque on the
  // cc::Layer.
  CC_BLINK_EXPORT void SetContentsOpaqueIsFixed(bool fixed);

  // WebLayer implementation.
  int id() const override;
  void invalidateRect(const blink::WebRect&) override;
  void invalidate() override;
  void addChild(blink::WebLayer* child) override;
  void insertChild(blink::WebLayer* child, size_t index) override;
  void replaceChild(blink::WebLayer* reference,
                    blink::WebLayer* new_layer) override;
  void removeFromParent() override;
  void removeAllChildren() override;
  void setBounds(const blink::WebSize& bounds) override;
  blink::WebSize bounds() const override;
  void setMasksToBounds(bool masks_to_bounds) override;
  bool masksToBounds() const override;
  void setMaskLayer(blink::WebLayer* mask) override;
  void setReplicaLayer(blink::WebLayer* replica) override;
  void setOpacity(float opacity) override;
  float opacity() const override;
  void setBlendMode(blink::WebBlendMode blend_mode) override;
  blink::WebBlendMode blendMode() const override;
  void setIsRootForIsolatedGroup(bool root) override;
  bool isRootForIsolatedGroup() override;
  void setOpaque(bool opaque) override;
  bool opaque() const override;
  void setPosition(const blink::WebFloatPoint& position) override;
  blink::WebFloatPoint position() const override;
  void setTransform(const SkMatrix44& transform) override;
  void setTransformOrigin(const blink::WebFloatPoint3D& point) override;
  blink::WebFloatPoint3D transformOrigin() const override;
  SkMatrix44 transform() const override;
  void setDrawsContent(bool draws_content) override;
  bool drawsContent() const override;
  void setDoubleSided(bool double_sided) override;
  void setShouldFlattenTransform(bool flatten) override;
  void setRenderingContext(int context) override;
  void setUseParentBackfaceVisibility(bool visible) override;
  void setBackgroundColor(blink::WebColor color) override;
  blink::WebColor backgroundColor() const override;
  void setFilters(const cc::FilterOperations& filters) override;
  void setBackgroundFilters(const cc::FilterOperations& filters) override;
  bool hasActiveAnimationForTesting() override;
  void setScrollPositionDouble(blink::WebDoublePoint position) override;
  blink::WebDoublePoint scrollPositionDouble() const override;
  void setScrollClipLayer(blink::WebLayer* clip_layer) override;
  bool scrollable() const override;
  void setUserScrollable(bool horizontal, bool vertical) override;
  bool userScrollableHorizontal() const override;
  bool userScrollableVertical() const override;
  void addMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons) override;
  void clearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear) override;
  uint32_t mainThreadScrollingReasons() override;
  bool shouldScrollOnMainThread() const override;
  void setNonFastScrollableRegion(
      const blink::WebVector<blink::WebRect>& region) override;
  blink::WebVector<blink::WebRect> nonFastScrollableRegion() const override;
  void setTouchEventHandlerRegion(
      const blink::WebVector<blink::WebRect>& region) override;
  blink::WebVector<blink::WebRect> touchEventHandlerRegion() const override;
  void setIsContainerForFixedPositionLayers(bool is_container) override;
  bool isContainerForFixedPositionLayers() const override;
  void setPositionConstraint(
      const blink::WebLayerPositionConstraint& constraint) override;
  blink::WebLayerPositionConstraint positionConstraint() const override;
  void setScrollClient(blink::WebLayerScrollClient* client) override;
  void setLayerClient(cc::LayerClient* client) override;
  const cc::Layer* ccLayer() const override;
  cc::Layer* ccLayer() override;
  void setElementId(const cc::ElementId&) override;
  cc::ElementId elementId() const override;
  void setCompositorMutableProperties(uint32_t properties) override;
  uint32_t compositorMutableProperties() const override;
  void setHasWillChangeTransformHint(bool has_will_change) override;

  void setScrollParent(blink::WebLayer* parent) override;
  void setClipParent(blink::WebLayer* parent) override;

 protected:
  scoped_refptr<cc::Layer> layer_;

  bool contents_opaque_is_fixed_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebLayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_LAYER_IMPL_H_
