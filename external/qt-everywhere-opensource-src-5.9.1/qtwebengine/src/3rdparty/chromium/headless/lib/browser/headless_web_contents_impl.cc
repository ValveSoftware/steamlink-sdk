// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_web_contents_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/origin_util.h"
#include "content/public/renderer/render_frame.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_client_impl.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "ui/aura/window.h"

namespace headless {

// static
HeadlessWebContentsImpl* HeadlessWebContentsImpl::From(
    HeadlessWebContents* web_contents) {
  // This downcast is safe because there is only one implementation of
  // HeadlessWebContents.
  return static_cast<HeadlessWebContentsImpl*>(web_contents);
}

class WebContentsObserverAdapter : public content::WebContentsObserver {
 public:
  WebContentsObserverAdapter(content::WebContents* web_contents,
                             HeadlessWebContents::Observer* observer)
      : content::WebContentsObserver(web_contents), observer_(observer) {}

  ~WebContentsObserverAdapter() override {}

  void RenderViewReady() override {
    DCHECK(web_contents()->GetMainFrame()->IsRenderFrameLive());
    observer_->DevToolsTargetReady();
  }

 private:
  HeadlessWebContents::Observer* observer_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverAdapter);
};

class HeadlessWebContentsImpl::Delegate : public content::WebContentsDelegate {
 public:
  explicit Delegate(HeadlessBrowserContextImpl* browser_context)
      : browser_context_(browser_context) {}

  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override {
    std::unique_ptr<HeadlessWebContentsImpl> web_contents =
        HeadlessWebContentsImpl::CreateFromWebContents(new_contents,
                                                       browser_context_);

    DCHECK(new_contents->GetBrowserContext() == browser_context_);

    browser_context_->RegisterWebContents(std::move(web_contents));
  }

  // Return the security style of the given |web_contents|, populating
  // |security_style_explanations| to explain why the SecurityStyle was chosen.
  blink::WebSecurityStyle GetSecurityStyle(
      content::WebContents* web_contents,
      content::SecurityStyleExplanations* security_style_explanations)
      override {
    security_state::SecurityInfo security_info;
    security_state::GetSecurityInfo(
        security_state::GetVisibleSecurityState(web_contents),
        false /* used_policy_installed_certificate */,
        base::Bind(&content::IsOriginSecure), &security_info);
    return security_state::GetSecurityStyle(security_info,
                                            security_style_explanations);
  }

 private:
  HeadlessBrowserContextImpl* browser_context_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

// static
std::unique_ptr<HeadlessWebContentsImpl> HeadlessWebContentsImpl::Create(
    HeadlessWebContents::Builder* builder,
    aura::Window* parent_window) {
  content::WebContents::CreateParams create_params(builder->browser_context_,
                                                   nullptr);
  create_params.initial_size = builder->window_size_;

  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      base::WrapUnique(new HeadlessWebContentsImpl(
          content::WebContents::Create(create_params),
          builder->browser_context_));

  headless_web_contents->mojo_services_ = std::move(builder->mojo_services_);
  headless_web_contents->InitializeScreen(parent_window, builder->window_size_);
  if (!headless_web_contents->OpenURL(builder->initial_url_))
    return nullptr;
  return headless_web_contents;
}

// static
std::unique_ptr<HeadlessWebContentsImpl>
HeadlessWebContentsImpl::CreateFromWebContents(
    content::WebContents* web_contents,
    HeadlessBrowserContextImpl* browser_context) {
  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      base::WrapUnique(
          new HeadlessWebContentsImpl(web_contents, browser_context));

  return headless_web_contents;
}

void HeadlessWebContentsImpl::InitializeScreen(aura::Window* parent_window,
                                               const gfx::Size& initial_size) {
  aura::Window* contents = web_contents_->GetNativeView();
  DCHECK(!parent_window->Contains(contents));
  parent_window->AddChild(contents);
  contents->Show();

  contents->SetBounds(gfx::Rect(initial_size));
  content::RenderWidgetHostView* host_view =
      web_contents_->GetRenderWidgetHostView();
  if (host_view)
    host_view->SetSize(initial_size);
}

HeadlessWebContentsImpl::HeadlessWebContentsImpl(
    content::WebContents* web_contents,
    HeadlessBrowserContextImpl* browser_context)
    : content::WebContentsObserver(web_contents),
      web_contents_delegate_(
          new HeadlessWebContentsImpl::Delegate(browser_context)),
      web_contents_(web_contents),
      agent_host_(content::DevToolsAgentHost::GetOrCreateFor(web_contents)),
      browser_context_(browser_context) {
  web_contents_->SetDelegate(web_contents_delegate_.get());
}

HeadlessWebContentsImpl::~HeadlessWebContentsImpl() {
  web_contents_->Close();
}

void HeadlessWebContentsImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!mojo_services_.empty()) {
    render_frame_host->GetRenderViewHost()->AllowBindings(
        content::BINDINGS_POLICY_HEADLESS);
  }

  service_manager::InterfaceRegistry* interface_registry =
      render_frame_host->GetInterfaceRegistry();

  for (const MojoService& service : mojo_services_) {
    interface_registry->AddInterface(service.service_name,
                                     service.service_factory,
                                     browser()->BrowserMainThread());
  }
}

bool HeadlessWebContentsImpl::OpenURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
  return true;
}

void HeadlessWebContentsImpl::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  browser_context()->DestroyWebContents(this);
}

std::string HeadlessWebContentsImpl::GetDevToolsAgentHostId() {
  return agent_host_->GetId();
}

void HeadlessWebContentsImpl::AddObserver(Observer* observer) {
  DCHECK(observer_map_.find(observer) == observer_map_.end());
  observer_map_[observer] = base::MakeUnique<WebContentsObserverAdapter>(
      web_contents_.get(), observer);
}

void HeadlessWebContentsImpl::RemoveObserver(Observer* observer) {
  ObserverMap::iterator it = observer_map_.find(observer);
  DCHECK(it != observer_map_.end());
  observer_map_.erase(it);
}

HeadlessDevToolsTarget* HeadlessWebContentsImpl::GetDevToolsTarget() {
  return web_contents()->GetMainFrame()->IsRenderFrameLive() ? this : nullptr;
}

void HeadlessWebContentsImpl::AttachClient(HeadlessDevToolsClient* client) {
  HeadlessDevToolsClientImpl::From(client)->AttachToHost(agent_host_.get());
}

void HeadlessWebContentsImpl::DetachClient(HeadlessDevToolsClient* client) {
  DCHECK(agent_host_);
  HeadlessDevToolsClientImpl::From(client)->DetachFromHost(agent_host_.get());
}

content::WebContents* HeadlessWebContentsImpl::web_contents() const {
  return web_contents_.get();
}

HeadlessBrowserImpl* HeadlessWebContentsImpl::browser() const {
  return browser_context_->browser();
}

HeadlessBrowserContextImpl* HeadlessWebContentsImpl::browser_context() const {
  return browser_context_;
}

HeadlessWebContents::Builder::Builder(
    HeadlessBrowserContextImpl* browser_context)
    : browser_context_(browser_context),
      window_size_(browser_context->options()->window_size()) {}

HeadlessWebContents::Builder::~Builder() = default;

HeadlessWebContents::Builder::Builder(Builder&&) = default;

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetInitialURL(
    const GURL& initial_url) {
  initial_url_ = initial_url;
  return *this;
}

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetWindowSize(
    const gfx::Size& size) {
  window_size_ = size;
  return *this;
}

HeadlessWebContents::Builder& HeadlessWebContents::Builder::AddMojoService(
    const std::string& service_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>&
        service_factory) {
  mojo_services_.emplace_back(service_name, service_factory);
  return *this;
}

HeadlessWebContents* HeadlessWebContents::Builder::Build() {
  return browser_context_->CreateWebContents(this);
}

HeadlessWebContents::Builder::MojoService::MojoService() {}

HeadlessWebContents::Builder::MojoService::MojoService(
    const std::string& service_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>& service_factory)
    : service_name(service_name), service_factory(service_factory) {}

HeadlessWebContents::Builder::MojoService::~MojoService() {}

}  // namespace headless
