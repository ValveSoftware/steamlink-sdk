// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_SESSION_BLIMP_ENGINE_SESSION_H_
#define BLIMP_ENGINE_SESSION_BLIMP_ENGINE_SESSION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/engine/feature/engine_render_widget_feature.h"
#include "blimp/engine/feature/engine_settings_feature.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/connection_error_observer.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/base/completion_callback.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class WindowTreeHost;

namespace client {
class DefaultCaptureClient;
class WindowTreeClient;
}  // namespace client
}  // namespace aura

namespace content {
class BrowserContext;
class RenderViewHost;
class WebContents;
}

namespace gfx {
class Size;
}

namespace net {
class NetLog;
}

namespace wm {
class FocusController;
}

namespace blimp {

class BlimpConnection;
class BlimpMessage;
class BlimpMessageThreadPipe;
class BlobCache;
class BlobChannelSender;
class HeliumBlobSenderDelegate;
class ThreadPipeManager;
class SettingsManager;

namespace engine {

class BlimpBrowserContext;
class BlimpEngineConfig;
class BlimpFocusClient;
class BlimpScreen;
class BlimpWindowTreeHost;
class EngineNetworkComponents;
class Tab;

class BlimpEngineSession
    : public BlimpMessageProcessor,
      public content::WebContentsDelegate,
      public ui::InputMethodObserver,
      public EngineRenderWidgetFeature::RenderWidgetMessageDelegate {
 public:
  using GetPortCallback = base::Callback<void(uint16_t)>;

  BlimpEngineSession(std::unique_ptr<BlimpBrowserContext> browser_context,
                     net::NetLog* net_log,
                     BlimpEngineConfig* config,
                     SettingsManager* settings_manager);
  ~BlimpEngineSession() override;

  // Starts the network stack on the IO thread, and sets default placeholder
  // values for e.g. screen size pending real values being supplied by the
  // client.
  void Initialize();

  BlimpBrowserContext* browser_context() { return browser_context_.get(); }

  BlobChannelSender* blob_channel_sender() {
    return blob_channel_sender_.get();
  }

  // Gets Engine's listening port. Invokes callback with the allocated port.
  void GetEnginePortForTesting(const GetPortCallback& callback);

  // BlimpMessageProcessor implementation.
  // This object handles incoming TAB_CONTROL and NAVIGATION messages directly.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  // Creates ThreadPipeManager, registers features, and then starts to accept
  // incoming connection.
  void RegisterFeatures();

  // TabControlMessage handler methods.
  // Creates a new tab, which will be indexed by |target_tab_id|.
  // Returns true if a new tab is created, false otherwise.
  bool CreateTab(const int target_tab_id);

  // Closes an existing tab, indexed by |target_tab_id|.
  void CloseTab(const int target_tab_id);

  // Resizes screen to |size| in pixels, and updates its device pixel ratio to
  // |device_pixel_ratio|.
  void HandleResize(float device_pixel_ratio, const gfx::Size& size);

  // RenderWidgetMessage handler methods.
  // RenderWidgetMessageDelegate implementation.
  void OnWebGestureEvent(
      content::RenderWidgetHost* render_widget_host,
      std::unique_ptr<blink::WebGestureEvent> event) override;
  void OnCompositorMessageReceived(
      content::RenderWidgetHost* render_widget_host,
      const std::vector<uint8_t>& message) override;

  // content::WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void CloseContents(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  void ForwardCompositorProto(
      content::RenderWidgetHost* render_widget_host,
      const std::vector<uint8_t>& proto) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;

  // ui::InputMethodObserver overrides.
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;
  void OnShowImeIfNeeded() override;

  // Sets up |new_contents| to be associated with the root window.
  void PlatformSetContents(std::unique_ptr<content::WebContents> new_contents,
                           const int target_tab_id);

  // Presents the client's single screen.
  // Screen should be deleted after browser context (crbug.com/613372).
  std::unique_ptr<BlimpScreen> screen_;

  // Content BrowserContext for this session.
  std::unique_ptr<BlimpBrowserContext> browser_context_;

  // Engine configuration including assigned client token.
  BlimpEngineConfig* engine_config_;

  // Represents the (currently single) browser window into which tab(s) will
  // be rendered.
  std::unique_ptr<BlimpWindowTreeHost> window_tree_host_;

  // Used to apply standard focus conventions to the windows in the
  // WindowTreeHost hierarchy.
  std::unique_ptr<wm::FocusController> focus_client_;

  // Used to manage input capture.
  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;

  // Used to attach null-parented windows (e.g. popups) to the root window.
  std::unique_ptr<aura::client::WindowTreeClient> window_tree_client_;

  // Manages all global settings for the engine session.
  SettingsManager* settings_manager_;

  // Handles all incoming messages for type SETTINGS.
  EngineSettingsFeature settings_feature_;

  // Handles all incoming and outgoing messages related to RenderWidget,
  // including INPUT, COMPOSITOR and RENDER_WIDGET messages.
  EngineRenderWidgetFeature render_widget_feature_;

  // Sends outgoing blob data as BlimpMessages.
  HeliumBlobSenderDelegate* blob_delegate_;

  // Receives image data from the renderer and sends it to the client via
  // |blob_delegate_|.
  std::unique_ptr<BlobChannelSender> blob_channel_sender_;

  // Container for connection manager, authentication handler, and
  // browser connection handler. The components run on the I/O thread, and
  // this object is destroyed there.
  std::unique_ptr<EngineNetworkComponents> net_components_;

  std::unique_ptr<ThreadPipeManager> thread_pipe_manager_;

  // Used to send TAB_CONTROL or NAVIGATION messages to client.
  std::unique_ptr<BlimpMessageProcessor> tab_control_message_sender_;
  std::unique_ptr<BlimpMessageProcessor> navigation_message_sender_;

  // TODO(haibinlu): Support more than one tab (crbug/547231)
  std::unique_ptr<Tab> tab_;

  DISALLOW_COPY_AND_ASSIGN(BlimpEngineSession);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_SESSION_BLIMP_ENGINE_SESSION_H_
