// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_LAYER_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_LAYER_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer_client.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebAnimation.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/skia/include/utils/SkMatrix44.h"

namespace cc {
class Layer;
}

namespace blink {
class WebFilterOperations;
class WebLayerClient;
struct WebFloatRect;
}

namespace base {
namespace debug {
class ConvertableToTraceFormat;
}
}

namespace content {

class WebToCCAnimationDelegateAdapter;

class WebLayerImpl : public blink::WebLayer, public cc::LayerClient {
 public:
  CONTENT_EXPORT WebLayerImpl();
  CONTENT_EXPORT explicit WebLayerImpl(
      scoped_refptr<cc::Layer>);
  virtual ~WebLayerImpl();

  static bool UsingPictureLayer();
  CONTENT_EXPORT static void SetImplSidePaintingEnabled(
      bool enabled);

  CONTENT_EXPORT cc::Layer* layer() const;

  // WebLayer implementation.
  virtual int id() const;
  virtual void invalidateRect(const blink::WebFloatRect&);
  virtual void invalidate();
  virtual void addChild(blink::WebLayer* child);
  virtual void insertChild(blink::WebLayer* child, size_t index);
  virtual void replaceChild(blink::WebLayer* reference,
                            blink::WebLayer* new_layer);
  virtual void removeFromParent();
  virtual void removeAllChildren();
  virtual void setBounds(const blink::WebSize& bounds);
  virtual blink::WebSize bounds() const;
  virtual void setMasksToBounds(bool masks_to_bounds);
  virtual bool masksToBounds() const;
  virtual void setMaskLayer(blink::WebLayer* mask);
  virtual void setReplicaLayer(blink::WebLayer* replica);
  virtual void setOpacity(float opacity);
  virtual float opacity() const;
  virtual void setBlendMode(blink::WebBlendMode blend_mode);
  virtual blink::WebBlendMode blendMode() const;
  virtual void setIsRootForIsolatedGroup(bool root);
  virtual bool isRootForIsolatedGroup();
  virtual void setOpaque(bool opaque);
  virtual bool opaque() const;
  virtual void setPosition(const blink::WebFloatPoint& position);
  virtual blink::WebFloatPoint position() const;
  virtual void setTransform(const SkMatrix44& transform);
  virtual void setTransformOrigin(const blink::WebFloatPoint3D& point);
  virtual blink::WebFloatPoint3D transformOrigin() const;

  // FIXME: get rid of these once Blink is no longer using them.
  virtual void setAnchorPoint(const blink::WebFloatPoint&);
  virtual blink::WebFloatPoint anchorPoint() const;
  virtual void setAnchorPointZ(float);
  virtual float anchorPointZ() const ;

  virtual SkMatrix44 transform() const;
  virtual void setDrawsContent(bool draws_content);
  virtual bool drawsContent() const;
  virtual void setShouldFlattenTransform(bool flatten);
  virtual void setRenderingContext(int context);
  virtual void setUseParentBackfaceVisibility(bool visible);
  virtual void setBackgroundColor(blink::WebColor color);
  virtual blink::WebColor backgroundColor() const;
  virtual void setFilters(const blink::WebFilterOperations& filters);
  virtual void setBackgroundFilters(const blink::WebFilterOperations& filters);
  virtual void setAnimationDelegate(blink::WebAnimationDelegate* delegate);
  virtual bool addAnimation(blink::WebAnimation* animation);
  virtual void removeAnimation(int animation_id);
  virtual void removeAnimation(int animation_id,
                               blink::WebAnimation::TargetProperty);
  virtual void pauseAnimation(int animation_id, double time_offset);
  virtual bool hasActiveAnimation();
  virtual void setForceRenderSurface(bool force);
  virtual void setScrollPosition(blink::WebPoint position);
  virtual blink::WebPoint scrollPosition() const;
  virtual void setScrollClipLayer(blink::WebLayer* clip_layer);
  virtual bool scrollable() const;
  virtual void setUserScrollable(bool horizontal, bool vertical);
  virtual bool userScrollableHorizontal() const;
  virtual bool userScrollableVertical() const;
  virtual void setHaveWheelEventHandlers(bool have_wheel_event_handlers);
  virtual bool haveWheelEventHandlers() const;
  virtual void setHaveScrollEventHandlers(bool have_scroll_event_handlers);
  virtual bool haveScrollEventHandlers() const;
  virtual void setShouldScrollOnMainThread(bool scroll_on_main);
  virtual bool shouldScrollOnMainThread() const;
  virtual void setNonFastScrollableRegion(
      const blink::WebVector<blink::WebRect>& region);
  virtual blink::WebVector<blink::WebRect> nonFastScrollableRegion() const;
  virtual void setTouchEventHandlerRegion(
      const blink::WebVector<blink::WebRect>& region);
  virtual blink::WebVector<blink::WebRect> touchEventHandlerRegion() const;
  virtual void setIsContainerForFixedPositionLayers(bool is_container);
  virtual bool isContainerForFixedPositionLayers() const;
  virtual void setPositionConstraint(
      const blink::WebLayerPositionConstraint& constraint);
  virtual blink::WebLayerPositionConstraint positionConstraint() const;
  virtual void setScrollClient(blink::WebLayerScrollClient* client);
  virtual bool isOrphan() const;
  virtual void setWebLayerClient(blink::WebLayerClient* client);

  // LayerClient implementation.
  virtual scoped_refptr<base::debug::ConvertableToTraceFormat>
      TakeDebugInfo() OVERRIDE;

  virtual void setScrollParent(blink::WebLayer* parent);
  virtual void setClipParent(blink::WebLayer* parent);

 protected:
  scoped_refptr<cc::Layer> layer_;
  blink::WebLayerClient* web_layer_client_;

 private:
  scoped_ptr<WebToCCAnimationDelegateAdapter> animation_delegate_adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_LAYER_IMPL_H_

