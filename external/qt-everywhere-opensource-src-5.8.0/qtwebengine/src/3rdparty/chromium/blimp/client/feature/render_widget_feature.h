// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_RENDER_WIDGET_FEATURE_H_
#define BLIMP_CLIENT_FEATURE_RENDER_WIDGET_FEATURE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/input_message_generator.h"

namespace blink {
class WebGestureEvent;
}

namespace cc {
namespace proto {
class CompositorMessage;
}
}

namespace blimp {
class RenderWidgetMessage;
class CompositorMessage;

namespace client {

// Handles all incoming and outgoing protobuf message types tied to a specific
// RenderWidget. This includes BlimpMessage::INPUT, BlimpMessage::COMPOSITOR,
// and BlimpMessage::RENDER_WIDGET messages.  Delegates can be added to be
// notified of incoming messages. This class automatically attaches a specific
// id so that the engine can drop stale RenderWidget related messages after it
// sends a RenderWidgetMessage::INITIALIZE message.
class RenderWidgetFeature : public BlimpMessageProcessor {
 public:
  // A delegate to be notified of specific RenderWidget related incoming events.
  class RenderWidgetFeatureDelegate {
   public:
    virtual ~RenderWidgetFeatureDelegate() {}
    virtual void OnRenderWidgetCreated(int render_widget_id) = 0;

    // Called when the engine's RenderWidget has changed for a particular
    // WebContents.  When this is received all state related to the existing
    // client RenderWidget should be reset.
    virtual void OnRenderWidgetInitialized(int render_widget_id) = 0;

    virtual void OnRenderWidgetDeleted(int render_widget_id) = 0;

    // Called when the engine sent a CompositorMessage.  These messages should
    // be sent to the client's RemoteChannel of the compositor.
    virtual void OnCompositorMessageReceived(
        int render_widget_id,
        std::unique_ptr<cc::proto::CompositorMessage> message) = 0;
  };

  RenderWidgetFeature();
  ~RenderWidgetFeature() override;

  // Set the BlimpMessageProcessor that will be used to send BlimpMessage::INPUT
  // messages to the engine.
  void set_outgoing_input_message_processor(
      std::unique_ptr<BlimpMessageProcessor> processor);

  // Set the BlimpMessageProcessor that will be used to send
  // BlimpMessage::COMPOSITOR messages to the engine.
  void set_outgoing_compositor_message_processor(
      std::unique_ptr<BlimpMessageProcessor> processor);

  // Sends a WebGestureEvent for |tab_id| to the engine.
  void SendWebGestureEvent(const int tab_id,
                           const int render_widget_id,
                           const blink::WebGestureEvent& event);

  // Sends a CompositorMessage for |tab_id| to the engine.
  void SendCompositorMessage(const int tab_id,
                             const int render_widget_id,
                             const cc::proto::CompositorMessage& message);

  // Sets a RenderWidgetMessageDelegate to be notified of all incoming
  // RenderWidget related messages for |tab_id| from the engine.  There can only
  // be one RenderWidgetMessageDelegate per tab.
  // virtual for testing.
  virtual void SetDelegate(int tab_id, RenderWidgetFeatureDelegate* delegate);
  virtual void RemoveDelegate(const int tab_id);

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  void ProcessRenderWidgetMessage(
      RenderWidgetFeatureDelegate* delegate,
      const RenderWidgetMessage& message);

  void ProcessCompositorMessage(RenderWidgetFeatureDelegate* delegate,
                                const CompositorMessage& message);

  // Returns nullptr if no delegate is found.
  RenderWidgetFeatureDelegate* FindDelegate(const int tab_id);

  typedef base::SmallMap<std::map<int, RenderWidgetFeatureDelegate*>>
      DelegateMap;

  DelegateMap delegates_;

  InputMessageGenerator input_message_generator_;

  // Used to send BlimpMessage::INPUT type messages to the engine.
  std::unique_ptr<BlimpMessageProcessor> outgoing_input_message_processor_;

  // Used to send BlimpMessage::COMPOSITOR messages to the engine.
  std::unique_ptr<BlimpMessageProcessor> outgoing_compositor_message_processor_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_RENDER_WIDGET_FEATURE_H_
