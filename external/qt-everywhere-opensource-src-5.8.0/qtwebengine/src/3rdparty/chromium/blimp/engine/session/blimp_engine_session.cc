// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/session/blimp_engine_session.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "blimp/common/blob_cache/in_memory_blob_cache.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/tab_control.pb.h"
#include "blimp/engine/app/blimp_engine_config.h"
#include "blimp/engine/app/settings_manager.h"
#include "blimp/engine/app/switches.h"
#include "blimp/engine/app/ui/blimp_layout_manager.h"
#include "blimp/engine/app/ui/blimp_screen.h"
#include "blimp/engine/app/ui/blimp_window_tree_client.h"
#include "blimp/engine/app/ui/blimp_window_tree_host.h"
#include "blimp/engine/common/blimp_browser_context.h"
#include "blimp/engine/common/blimp_user_agent.h"
#include "blimp/engine/session/tab.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "blimp/net/blimp_message_multiplexer.h"
#include "blimp/net/blimp_message_thread_pipe.h"
#include "blimp/net/blob_channel/blob_channel_sender_impl.h"
#include "blimp/net/blob_channel/helium_blob_sender_delegate.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/common.h"
#include "blimp/net/engine_authentication_handler.h"
#include "blimp/net/engine_connection_manager.h"
#include "blimp/net/null_blimp_message_processor.h"
#include "blimp/net/tcp_engine_transport.h"
#include "blimp/net/thread_pipe_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/focus_controller.h"

namespace blimp {
namespace engine {
namespace {

const int kDummyTabId = 0;
const float kDefaultScaleFactor = 1.f;
const int kDefaultDisplayWidth = 800;
const int kDefaultDisplayHeight = 600;
const uint16_t kDefaultPort = 25467;

// Focus rules that support activating an child window.
class FocusRulesImpl : public wm::BaseFocusRules {
 public:
  FocusRulesImpl() {}
  ~FocusRulesImpl() override {}

  bool SupportsChildActivation(aura::Window* window) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusRulesImpl);
};

net::IPAddress GetIPv4AnyAddress() {
  return net::IPAddress(0, 0, 0, 0);
}

// Proxies calls to TaskRunner::PostTask while stripping the return value,
// which provides a suitable function prototype for binding a base::Closure.
void PostTask(const scoped_refptr<base::TaskRunner>& task_runner,
              const base::Closure& closure) {
  task_runner->PostTask(FROM_HERE, closure);
}

// Returns a closure that quits the current (bind-time) MessageLoop.
base::Closure QuitCurrentMessageLoopClosure() {
  return base::Bind(&PostTask, base::ThreadTaskRunnerHandle::Get(),
                    base::MessageLoop::QuitWhenIdleClosure());
}

uint16_t GetListeningPort() {
  unsigned port_parsed = 0;
  if (!base::StringToUint(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              kEnginePort),
          &port_parsed)) {
    return kDefaultPort;
  }
  if (port_parsed > 65535) {
    LOG(FATAL) << "--engine-port must be a value between 0 and 65535.";
    return kDefaultPort;
  }
  return port_parsed;
}

}  // namespace

// EngineNetworkComponents is created by the BlimpEngineSession on the UI
// thread, and then used and destroyed on the IO thread.
class EngineNetworkComponents : public ConnectionHandler,
                                public ConnectionErrorObserver {
 public:
  // |net_log|: The log to use for network-related events.
  // |quit_closure|: A closure which will terminate the engine when
  //                 invoked.
  EngineNetworkComponents(net::NetLog* net_log,
                          const base::Closure& quit_closure);
  ~EngineNetworkComponents() override;

  // Sets up network components and starts listening for incoming connection.
  // This should be called after all features have been registered so that
  // received messages can be properly handled.
  void Initialize(const std::string& client_token);

  uint16_t GetPortForTesting() { return port_; }

  BrowserConnectionHandler* GetBrowserConnectionHandler();

 private:
  // ConnectionHandler implementation.
  void HandleConnection(std::unique_ptr<BlimpConnection> connection) override;

  // ConnectionErrorObserver implementation.
  // Signals the engine session that an authenticated connection was
  // terminated.
  void OnConnectionError(int error) override;

  net::NetLog* net_log_;
  base::Closure quit_closure_;
  uint16_t port_ = 0;

  std::unique_ptr<BrowserConnectionHandler> connection_handler_;
  std::unique_ptr<EngineAuthenticationHandler> authentication_handler_;
  std::unique_ptr<EngineConnectionManager> connection_manager_;
  BlimpConnectionStatistics blimp_connection_statistics_;

  DISALLOW_COPY_AND_ASSIGN(EngineNetworkComponents);
};

EngineNetworkComponents::EngineNetworkComponents(
    net::NetLog* net_log,
    const base::Closure& quit_closure)
    : net_log_(net_log),
      quit_closure_(quit_closure),
      connection_handler_(new BrowserConnectionHandler) {}

EngineNetworkComponents::~EngineNetworkComponents() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void EngineNetworkComponents::Initialize(const std::string& client_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!connection_manager_);

  // Plumb authenticated connections from the authentication handler
  // to |this| (which will then pass it to |connection_handler_|.
  authentication_handler_ =
      base::WrapUnique(new EngineAuthenticationHandler(this, client_token));

  // Plumb unauthenticated connections to |authentication_handler_|.
  connection_manager_ = base::WrapUnique(
      new EngineConnectionManager(authentication_handler_.get()));

  // Adds BlimpTransports to connection_manager_.
  net::IPEndPoint address(GetIPv4AnyAddress(), GetListeningPort());
  TCPEngineTransport* transport =
      new TCPEngineTransport(address, &blimp_connection_statistics_, net_log_);
  connection_manager_->AddTransport(base::WrapUnique(transport));

  transport->GetLocalAddress(&address);
  port_ = address.port();
}

void EngineNetworkComponents::HandleConnection(
    std::unique_ptr<BlimpConnection> connection) {
  // Observe |connection| for disconnection events.
  connection->AddConnectionErrorObserver(this);
  connection_handler_->HandleConnection(std::move(connection));
}

void EngineNetworkComponents::OnConnectionError(int error) {
  DVLOG(1) << "EngineNetworkComponents::OnConnectionError(" << error << ")";
  quit_closure_.Run();
}

BrowserConnectionHandler*
EngineNetworkComponents::GetBrowserConnectionHandler() {
  return connection_handler_.get();
}

BlimpEngineSession::BlimpEngineSession(
    std::unique_ptr<BlimpBrowserContext> browser_context,
    net::NetLog* net_log,
    BlimpEngineConfig* engine_config,
    SettingsManager* settings_manager)
    : screen_(new BlimpScreen),
      browser_context_(std::move(browser_context)),
      engine_config_(engine_config),
      settings_manager_(settings_manager),
      settings_feature_(settings_manager_),
      render_widget_feature_(settings_manager_),
      net_components_(
          new EngineNetworkComponents(net_log,
                                      QuitCurrentMessageLoopClosure())) {
  DCHECK(engine_config_);
  DCHECK(settings_manager_);

  screen_->UpdateDisplayScaleAndSize(
      kDefaultScaleFactor,
      gfx::Size(kDefaultDisplayWidth, kDefaultDisplayHeight));
  render_widget_feature_.SetDelegate(kDummyTabId, this);

  std::unique_ptr<HeliumBlobSenderDelegate> helium_blob_delegate(
      new HeliumBlobSenderDelegate);
  blob_delegate_ = helium_blob_delegate.get();
  blob_channel_sender_ = base::WrapUnique(
      new BlobChannelSenderImpl(base::WrapUnique(new InMemoryBlobCache),
                                std::move(helium_blob_delegate)));
}

BlimpEngineSession::~BlimpEngineSession() {
  render_widget_feature_.RemoveDelegate(kDummyTabId);

  window_tree_host_->GetInputMethod()->RemoveObserver(this);

  // Ensure that all tabs are torn down first, since teardown will
  // trigger RenderViewDeleted callbacks to their observers, which will in turn
  // send messages to net_components_, which is already deleted due to the line
  // below.
  tab_.reset();

  // Safely delete network components on the IO thread.
  content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                     net_components_.release());
}

void BlimpEngineSession::Initialize() {
  DCHECK(!display::Screen::GetScreen());
  display::Screen::SetScreenInstance(screen_.get());

  window_tree_host_.reset(new BlimpWindowTreeHost());

  screen_->set_window_tree_host(window_tree_host_.get());
  window_tree_host_->InitHost();
  window_tree_host_->window()->SetLayoutManager(
      new BlimpLayoutManager(window_tree_host_->window()));
  focus_client_.reset(new wm::FocusController(new FocusRulesImpl));
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  aura::client::SetActivationClient(window_tree_host_->window(),
                                    focus_client_.get());
  capture_client_.reset(
      new aura::client::DefaultCaptureClient(window_tree_host_->window()));

  window_tree_client_.reset(
      new BlimpWindowTreeClient(window_tree_host_->window()));

  window_tree_host_->GetInputMethod()->AddObserver(this);

  window_tree_host_->SetBounds(gfx::Rect(screen_->GetPrimaryDisplay().size()));

  RegisterFeatures();

  // Initialize must only be posted after the RegisterFeature calls have
  // completed.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&EngineNetworkComponents::Initialize,
                 base::Unretained(net_components_.get()),
                 engine_config_->client_token()));
}

void BlimpEngineSession::GetEnginePortForTesting(
    const GetPortCallback& callback) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&EngineNetworkComponents::GetPortForTesting,
                 base::Unretained(net_components_.get())),
      callback);
}

void BlimpEngineSession::RegisterFeatures() {
  thread_pipe_manager_.reset(new ThreadPipeManager(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO),
      net_components_->GetBrowserConnectionHandler()));

  // Register features' message senders and receivers.
  tab_control_message_sender_ =
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kTabControl, this);
  navigation_message_sender_ =
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kNavigation, this);
  render_widget_feature_.set_render_widget_message_sender(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kRenderWidget,
                                            &render_widget_feature_));
  render_widget_feature_.set_input_message_sender(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kInput,
                                            &render_widget_feature_));
  render_widget_feature_.set_compositor_message_sender(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kCompositor,
                                            &render_widget_feature_));
  render_widget_feature_.set_ime_message_sender(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kIme,
                                            &render_widget_feature_));
  blob_delegate_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kBlobChannel,
                                            blob_delegate_));

  // The Settings feature does not need an outgoing message processor, since we
  // don't send any messages to the client right now.
  thread_pipe_manager_->RegisterFeature(BlimpMessage::kSettings,
                                        &settings_feature_);
}

bool BlimpEngineSession::CreateTab(const int target_tab_id) {
  DVLOG(1) << "Create tab " << target_tab_id;
  if (tab_) {
    return false;
  }

  content::WebContents::CreateParams create_params(browser_context_.get(),
                                                   nullptr);
  std::unique_ptr<content::WebContents> new_contents =
      base::WrapUnique(content::WebContents::Create(create_params));
  PlatformSetContents(std::move(new_contents), target_tab_id);

  return true;
}

void BlimpEngineSession::CloseTab(const int target_tab_id) {
  DVLOG(1) << "Close tab " << target_tab_id;
  tab_.reset();
}

void BlimpEngineSession::HandleResize(float device_pixel_ratio,
                                      const gfx::Size& size) {
  DVLOG(1) << "Resize to " << size.ToString() << ", " << device_pixel_ratio;
  screen_->UpdateDisplayScaleAndSize(device_pixel_ratio, size);
  window_tree_host_->SetBounds(gfx::Rect(size));
  if (tab_) {
    tab_->Resize(device_pixel_ratio,
                 screen_->GetPrimaryDisplay().bounds().size());
  }
}

void BlimpEngineSession::OnWebGestureEvent(
    content::RenderWidgetHost* render_widget_host,
    std::unique_ptr<blink::WebGestureEvent> event) {
  TRACE_EVENT1("blimp", "BlimpEngineSession::OnWebGestureEvent", "type",
               event->type);
  render_widget_host->ForwardGestureEvent(*event);
}

void BlimpEngineSession::OnCompositorMessageReceived(
    content::RenderWidgetHost* render_widget_host,
    const std::vector<uint8_t>& message) {
  TRACE_EVENT0("blimp", "BlimpEngineSession::OnCompositorMessageReceived");

  render_widget_host->HandleCompositorProto(message);
}

void BlimpEngineSession::OnTextInputTypeChanged(
    const ui::TextInputClient* client) {}

void BlimpEngineSession::OnFocus() {}

void BlimpEngineSession::OnBlur() {}

void BlimpEngineSession::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {}

// Called when either:
//  - the TextInputClient is changed (e.g. by a change of focus)
//  - the TextInputType of the TextInputClient changes
void BlimpEngineSession::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!tab_ || !tab_->web_contents()->GetRenderWidgetHostView())
    return;

  ui::TextInputType type =
      client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;

  // TODO(shaktisahu): Propagate the new type to the client.
  // Hide IME, when text input is out of focus, i.e. if the text input type
  // changes to ui::TEXT_INPUT_TYPE_NONE. For other text input types,
  // OnShowImeIfNeeded is used instead to send show IME request to client.
  if (type == ui::TEXT_INPUT_TYPE_NONE)
    render_widget_feature_.SendHideImeRequest(
        kDummyTabId,
        tab_->web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
}

void BlimpEngineSession::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {}

// Called when a user input should trigger showing the IME.
void BlimpEngineSession::OnShowImeIfNeeded() {
  TRACE_EVENT0("blimp", "BlimpEngineSession::OnShowImeIfNeeded");
  if (!tab_ || !tab_->web_contents()->GetRenderWidgetHostView() ||
      !window_tree_host_->GetInputMethod()->GetTextInputClient())
    return;

  render_widget_feature_.SendShowImeRequest(
      kDummyTabId,
      tab_->web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost(),
      window_tree_host_->GetInputMethod()->GetTextInputClient());
}

void BlimpEngineSession::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  TRACE_EVENT1("blimp", "BlimpEngineSession::ProcessMessage", "TabId",
               message->target_tab_id());
  DCHECK(!callback.is_null());
  DCHECK(BlimpMessage::kTabControl == message->feature_case() ||
         BlimpMessage::kNavigation == message->feature_case());

  net::Error result = net::OK;
  if (message->has_tab_control()) {
    switch (message->tab_control().tab_control_case()) {
      case TabControlMessage::kCreateTab:
        if (!CreateTab(message->target_tab_id()))
          result = net::ERR_FAILED;
        break;
      case TabControlMessage::kCloseTab:
        CloseTab(message->target_tab_id());
      case TabControlMessage::kSize:
        HandleResize(message->tab_control().size().device_pixel_ratio(),
                     gfx::Size(message->tab_control().size().width(),
                               message->tab_control().size().height()));
        break;
      default:
        NOTIMPLEMENTED();
        result = net::ERR_NOT_IMPLEMENTED;
    }
  } else if (message->has_navigation()) {
    if (tab_) {
      switch (message->navigation().type()) {
        case NavigationMessage::LOAD_URL:
          tab_->LoadUrl(GURL(message->navigation().load_url().url()));
          break;
        case NavigationMessage::GO_BACK:
          tab_->GoBack();
          break;
        case NavigationMessage::GO_FORWARD:
          tab_->GoForward();
          break;
        case NavigationMessage::RELOAD:
          tab_->Reload();
          break;
        default:
          NOTIMPLEMENTED();
          result = net::ERR_NOT_IMPLEMENTED;
      }
    } else {
      VLOG(1) << "Tab " << message->target_tab_id() << " does not exist";
    }
  } else {
    NOTREACHED();
    result = net::ERR_FAILED;
  }

  callback.Run(result);
}

void BlimpEngineSession::AddNewContents(content::WebContents* source,
                                        content::WebContents* new_contents,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_rect,
                                        bool user_gesture,
                                        bool* was_blocked) {
  NOTIMPLEMENTED();
}

content::WebContents* BlimpEngineSession::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // CURRENT_TAB is the only one we implement for now.
  if (params.disposition != CURRENT_TAB) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  // TODO(haibinlu): Add helper method to get LoadURLParams from OpenURLParams.
  content::NavigationController::LoadURLParams load_url_params(params.url);
  load_url_params.source_site_instance = params.source_site_instance;
  load_url_params.referrer = params.referrer;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.transition_type = params.transition;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;
  load_url_params.is_renderer_initiated = params.is_renderer_initiated;
  load_url_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_TRUE;

  source->GetController().LoadURLWithParams(load_url_params);
  return source;
}

void BlimpEngineSession::RequestToLockMouse(content::WebContents* web_contents,
                                            bool user_gesture,
                                            bool last_unlocked_by_target) {
  web_contents->GotResponseToLockMouseRequest(true);
}

void BlimpEngineSession::CloseContents(content::WebContents* source) {
  if (source == tab_->web_contents()) {
    tab_.reset();
  }
}

void BlimpEngineSession::ActivateContents(content::WebContents* contents) {
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

void BlimpEngineSession::ForwardCompositorProto(
    content::RenderWidgetHost* render_widget_host,
    const std::vector<uint8_t>& proto) {
  TRACE_EVENT0("blimp", "BlimpEngineSession::ForwardCompositorProto");
  render_widget_feature_.SendCompositorMessage(kDummyTabId, render_widget_host,
                                               proto);
}

void BlimpEngineSession::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  TRACE_EVENT0("blimp", "BlimpEngineSession::NavigationStateChanged");
  if (source == tab_->web_contents())
    tab_->NavigationStateChanged(changed_flags);
}

void BlimpEngineSession::PlatformSetContents(
    std::unique_ptr<content::WebContents> new_contents,
    const int target_tab_id) {
  new_contents->SetDelegate(this);

  aura::Window* parent = window_tree_host_->window();
  aura::Window* content = new_contents->GetNativeView();
  if (!parent->Contains(content))
    parent->AddChild(content);
  content->Show();

  tab_ = base::WrapUnique(new Tab(std::move(new_contents), target_tab_id,
                                  &render_widget_feature_,
                                  navigation_message_sender_.get()));
}

}  // namespace engine
}  // namespace blimp
