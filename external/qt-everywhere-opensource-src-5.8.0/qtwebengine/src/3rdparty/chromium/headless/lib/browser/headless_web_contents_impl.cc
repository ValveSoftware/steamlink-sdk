// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_web_contents_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/render_frame.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_client_impl.h"
#include "ui/aura/window.h"

namespace headless {

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
  explicit Delegate(HeadlessBrowserImpl* browser) : browser_(browser) {}

  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override {
    browser_->RegisterWebContents(
        HeadlessWebContentsImpl::CreateFromWebContents(new_contents, browser_));
  }

 private:
  HeadlessBrowserImpl* browser_;  // Not owned.
  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

// static
std::unique_ptr<HeadlessWebContentsImpl> HeadlessWebContentsImpl::Create(
    HeadlessWebContents::Builder* builder,
    aura::Window* parent_window,
    HeadlessBrowserImpl* browser) {
  content::BrowserContext* context =
      HeadlessBrowserContextImpl::From(builder->browser_context_);
  content::WebContents::CreateParams create_params(context, nullptr);
  create_params.initial_size = builder->window_size_;

  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      base::WrapUnique(new HeadlessWebContentsImpl(
          content::WebContents::Create(create_params), browser));

  headless_web_contents->InitializeScreen(parent_window, builder->window_size_);
  if (!headless_web_contents->OpenURL(builder->initial_url_))
    return nullptr;
  return headless_web_contents;
}

// static
std::unique_ptr<HeadlessWebContentsImpl>
HeadlessWebContentsImpl::CreateFromWebContents(
    content::WebContents* web_contents,
    HeadlessBrowserImpl* browser) {
  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      base::WrapUnique(new HeadlessWebContentsImpl(web_contents, browser));

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
    HeadlessBrowserImpl* browser)
    : web_contents_delegate_(new HeadlessWebContentsImpl::Delegate(browser)),
      web_contents_(web_contents),
      browser_(browser) {
  web_contents_->SetDelegate(web_contents_delegate_.get());
}

HeadlessWebContentsImpl::~HeadlessWebContentsImpl() {
  web_contents_->Close();
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
  browser_->DestroyWebContents(this);
}

void HeadlessWebContentsImpl::AddObserver(Observer* observer) {
  DCHECK(observer_map_.find(observer) == observer_map_.end());
  observer_map_[observer] = base::WrapUnique(
      new WebContentsObserverAdapter(web_contents_.get(), observer));
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
  if (!agent_host_)
    agent_host_ = content::DevToolsAgentHost::GetOrCreateFor(web_contents());
  HeadlessDevToolsClientImpl::From(client)->AttachToHost(agent_host_.get());
}

void HeadlessWebContentsImpl::DetachClient(HeadlessDevToolsClient* client) {
  DCHECK(agent_host_);
  HeadlessDevToolsClientImpl::From(client)->DetachFromHost(agent_host_.get());
}

content::WebContents* HeadlessWebContentsImpl::web_contents() const {
  return web_contents_.get();
}

HeadlessWebContents::Builder::Builder(HeadlessBrowserImpl* browser)
    : browser_(browser),
      browser_context_(
          browser->browser_main_parts()->default_browser_context()) {}

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

HeadlessWebContents::Builder& HeadlessWebContents::Builder::SetBrowserContext(
    HeadlessBrowserContext* browser_context) {
  browser_context_ = browser_context;
  return *this;
}

HeadlessWebContents* HeadlessWebContents::Builder::Build() {
  return browser_->CreateWebContents(this);
}

}  // namespace headless
