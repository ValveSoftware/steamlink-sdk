// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_FEATURE_ENGINE_RENDER_WIDGET_FEATURE_H_
#define BLIMP_ENGINE_FEATURE_ENGINE_RENDER_WIDGET_FEATURE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "blimp/engine/app/settings_manager.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/input_message_converter.h"
#include "ui/base/ime/text_input_client.h"

namespace blink {
class WebGestureEvent;
}

namespace content {
class RenderWidgetHost;
}

namespace blimp {
namespace engine {

// Handles all incoming and outgoing protobuf message types tied to a specific
// RenderWidget. This includes BlimpMessage::INPUT, BlimpMessage::COMPOSITOR,
// and BlimpMessage::RENDER_WIDGET messages.  Delegates can be added to be
// notified of incoming messages. This class automatically handles dropping
// stale BlimpMessage::RENDER_WIDGET messages from the client after a
// RenderWidgetMessage::INITIALIZE message is sent.
class EngineRenderWidgetFeature : public BlimpMessageProcessor,
                                  public SettingsManager::Observer {
 public:
  // A delegate to be notified of specific RenderWidget related incoming events.
  class RenderWidgetMessageDelegate {
   public:
    // Called when the client is sending a WebGestureEvent to the engine.
    virtual void OnWebGestureEvent(
        content::RenderWidgetHost* render_widget_host,
        std::unique_ptr<blink::WebGestureEvent> event) = 0;

    // Called when the client sent a CompositorMessage.  These messages should
    // be sent to the engine's render process so they can be processed by the
    // RemoteChannel of the compositor.
    virtual void OnCompositorMessageReceived(
        content::RenderWidgetHost* render_widget_host,
        const std::vector<uint8_t>& message) = 0;
  };

  explicit EngineRenderWidgetFeature(SettingsManager* settings);
  ~EngineRenderWidgetFeature() override;

  void set_render_widget_message_sender(
      std::unique_ptr<BlimpMessageProcessor> message_processor);

  void set_input_message_sender(
      std::unique_ptr<BlimpMessageProcessor> message_processor);

  void set_compositor_message_sender(
      std::unique_ptr<BlimpMessageProcessor> message_processor);

  void set_ime_message_sender(
      std::unique_ptr<BlimpMessageProcessor> message_processor);

  // Notifes the client that a new RenderWidget for a particular WebContents has
  // been created. This will trigger the creation of the BlimpCompositor for
  // this widget on the client.
  void OnRenderWidgetCreated(const int tab_id,
                             content::RenderWidgetHost* render_widget_host);

  // Notifies the client that the RenderWidget for a particular WebContents has
  // changed.  When this is sent the native view on the client becomes bound to
  // the BlimpCompositor for this widget.
  // Since the compositor on the client performs the operations of the view for
  // this widget, this will set the visibility and draw state correctly for this
  // widget.
  // Note: This assumes that this is the RenderWidgetHost for the main frame.
  // Only one RenderWidget can be in initialized state for a tab.
  void OnRenderWidgetInitialized(const int tab_id,
                                 content::RenderWidgetHost* render_widget_host);

  void OnRenderWidgetDeleted(const int tab_id,
                             content::RenderWidgetHost* render_widget_host);

  // Notifies the client to show/hide IME.
  void SendShowImeRequest(const int tab_id,
                          content::RenderWidgetHost* render_widget_host,
                          const ui::TextInputClient* client);
  void SendHideImeRequest(const int tab_id,
                          content::RenderWidgetHost* render_widget_host);

  // Sends a CompositorMessage for |tab_id| to the client.
  void SendCompositorMessage(const int tab_id,
                             content::RenderWidgetHost* render_widget_host,
                             const std::vector<uint8_t>& message);

  // Sets a RenderWidgetMessageDelegate to be notified of all incoming
  // RenderWidget related messages for |tab_id| from the client.  There can only
  // be one RenderWidgetMessageDelegate per tab.
  void SetDelegate(const int tab_id, RenderWidgetMessageDelegate* delegate);
  void RemoveDelegate(const int tab_id);

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // Settings::Observer implementation.
  void OnWebPreferencesChanged() override;

 private:
  typedef base::SmallMap<std::map<int, RenderWidgetMessageDelegate*> >
  DelegateMap;

  typedef base::SmallMap<std::map<content::RenderWidgetHost*, int>>
      RenderWidgetToIdMap;

  typedef base::SmallMap<std::map<int, content::RenderWidgetHost*>>
      IdToRenderWidgetMap;

  typedef std::pair<RenderWidgetToIdMap, IdToRenderWidgetMap> RenderWidgetMaps;

  typedef base::SmallMap<std::map<int, RenderWidgetMaps>> TabMap;

  // Returns nullptr if no delegate is found.
  RenderWidgetMessageDelegate* FindDelegate(const int tab_id);

  // Adds the RenderWidgetHost to the map and return the render_widget_id.
  int AddRenderWidget(const int tab_id,
                      content::RenderWidgetHost* render_widget_host);

  // Deletes the RenderWidgetHost from the map and returns the render_widget_id
  // for the deleted host.
  int DeleteRenderWidget(const int tab_id,
                         content::RenderWidgetHost* render_widget_host);

  // Returns the render_widget_id for the RenderWidgetHost. Will return 0U if
  // the host is not found.
  int GetRenderWidgetId(const int tab_id,
                        content::RenderWidgetHost* render_widget_host);

  // Returns the RenderWidgetHost for the given render_widget_id. Will return
  // nullptr if no host is found.
  content::RenderWidgetHost* GetRenderWidgetHost(const int tab_id,
                                                 const int render_widget_id);

  // Inserts the text entered by the user into the |client|.
  // The existing text in the box gets replaced by the new text from IME.
  void SetTextFromIME(ui::TextInputClient* client, std::string text);

  DelegateMap delegates_;
  TabMap tabs_;

  // A RenderWidgetHost can also be uniquely identified by the
  // <process_id, routing_id> where the process_id is the id for the
  // RenderProcessHost for this widget and the routing_id is the id for the
  // widget.
  // But we generate our own ids to avoid having the render widget protocol tied
  // to always using a combination of these ids, generated by the content layer.
  // By using an AtomicSequenceNumber for identifying render widgets across
  // tabs, we can be sure that there will always be a 1:1 mapping between the
  // render widget and the consumer of the features tied to this widget on the
  // client, which is the BlimpCompositor.
  base::AtomicSequenceNumber next_widget_id_;

  InputMessageConverter input_message_converter_;

  SettingsManager* settings_manager_;

  // Outgoing message processors for RENDER_WIDGET, COMPOSITOR and INPUT types.
  std::unique_ptr<BlimpMessageProcessor> render_widget_message_sender_;
  std::unique_ptr<BlimpMessageProcessor> compositor_message_sender_;
  std::unique_ptr<BlimpMessageProcessor> input_message_sender_;
  std::unique_ptr<BlimpMessageProcessor> ime_message_sender_;

  DISALLOW_COPY_AND_ASSIGN(EngineRenderWidgetFeature);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_FEATURE_ENGINE_RENDER_WIDGET_FEATURE_H_
