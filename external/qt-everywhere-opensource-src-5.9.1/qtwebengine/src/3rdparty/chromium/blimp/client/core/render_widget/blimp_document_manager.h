// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_RENDER_WIDGET_BLIMP_DOCUMENT_MANAGER_H_
#define BLIMP_CLIENT_CORE_RENDER_WIDGET_BLIMP_DOCUMENT_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "blimp/client/core/compositor/blimp_compositor.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace blimp {
namespace client {

class BlimpDocument;

// The BlimpDocumentManager is responsible for managing multiple BlimpDocument
// instances, each mapped to a render widget on the engine.
// The |document_id_| matches the |render_widget_id| of the
// render widget messages which are routed to the engine side render widget.
//
// The compositor corresponding to the render widget initialized on the
// engine will be the |active_document_|.
// All events from the native view are forwarded to this compositor.
class BlimpDocumentManager
    : public RenderWidgetFeature::RenderWidgetFeatureDelegate {
 public:
  explicit BlimpDocumentManager(
      int blimp_contents_id,
      RenderWidgetFeature* render_widget_feature,
      BlimpCompositorDependencies* compositor_dependencies);
  ~BlimpDocumentManager() override;

  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  bool OnTouchEvent(const ui::MotionEvent& motion_event);

  void RequestCopyOfCompositorOutput(
      std::unique_ptr<cc::CopyOutputRequest> copy_request,
      bool flush_pending_update);

  // Sends input event to the engine, virtual for testing.
  virtual void SendWebGestureEvent(int document_id,
                                   const blink::WebGestureEvent& gesture_event);

  // Sends compositor message to the engine, virtual for testing.
  virtual void SendCompositorMessage(
      int document_id,
      const cc::proto::CompositorMessage& message);

  scoped_refptr<cc::Layer> layer() const { return layer_; }

 protected:
  // Creates a BlimpDocument, virtual for testing.
  virtual std::unique_ptr<BlimpDocument> CreateBlimpDocument(
      int document_id,
      BlimpCompositorDependencies* compositor_dependencies);

  // Returns the blimp document from |document_id|, returns nullptr if
  // the document is not found, protected for testing.
  BlimpDocument* GetDocument(int document_id);

 private:
  // RenderWidgetFeatureDelegate implementation.
  void OnRenderWidgetCreated(int render_widget_id) override;
  void OnRenderWidgetInitialized(int render_widget_id) override;
  void OnRenderWidgetDeleted(int render_widget_id) override;
  void OnCompositorMessageReceived(
      int render_widget_id,
      std::unique_ptr<cc::proto::CompositorMessage> message) override;

  // The unique id of BlimpContentImpl which owns this document manager.
  int blimp_contents_id_;

  // The bridge to the network layer that does the proto/RenderWidget id work.
  // BlimpCompositorManager does not own this and it is expected to outlive this
  // BlimpCompositorManager instance.
  RenderWidgetFeature* render_widget_feature_;

  bool visible_;

  // The layer which holds the content from the active compositor.
  scoped_refptr<cc::Layer> layer_;

  // A map of document id to the BlimpDocument instance.
  using DocumentMap = std::map<int, std::unique_ptr<BlimpDocument>>;
  DocumentMap documents_;

  // The |active_document_| holds the compositor from the that is currently
  // visible. It corresponds to the
  // render widget currently initialized on the engine.
  BlimpDocument* active_document_;

  BlimpCompositorDependencies* compositor_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(BlimpDocumentManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_RENDER_WIDGET_BLIMP_DOCUMENT_MANAGER_H_
