// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_view_scheduler_impl.h"

#include "base/logging.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"
#include "components/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebFrameScheduler.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace scheduler {

WebViewSchedulerImpl::WebViewSchedulerImpl(
    blink::WebView* web_view,
    RendererSchedulerImpl* renderer_scheduler,
    bool disable_background_timer_throttling)
    : virtual_time_pump_policy_(TaskQueue::PumpPolicy::AUTO),
      web_view_(web_view),
      renderer_scheduler_(renderer_scheduler),
      page_visible_(true),
      disable_background_timer_throttling_(disable_background_timer_throttling),
      allow_virtual_time_to_advance_(true) {
  renderer_scheduler->AddWebViewScheduler(this);
}

WebViewSchedulerImpl::~WebViewSchedulerImpl() {
  // TODO(alexclarke): Find out why we can't rely on the web view outliving the
  // frame.
  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->DetachFromWebViewScheduler();
  }
  renderer_scheduler_->RemoveWebViewScheduler(this);
  if (virtual_time_domain_)
    renderer_scheduler_->UnregisterTimeDomain(virtual_time_domain_.get());
}

void WebViewSchedulerImpl::setPageVisible(bool page_visible) {
  if (disable_background_timer_throttling_ || page_visible_ == page_visible)
    return;

  page_visible_ = page_visible;

  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->setPageVisible(page_visible_);
  }
}

std::unique_ptr<WebFrameSchedulerImpl>
WebViewSchedulerImpl::createWebFrameSchedulerImpl(
    base::trace_event::BlameContext* blame_context) {
  std::unique_ptr<WebFrameSchedulerImpl> frame_scheduler(
      new WebFrameSchedulerImpl(renderer_scheduler_, this, blame_context));
  frame_scheduler->setPageVisible(page_visible_);
  frame_schedulers_.insert(frame_scheduler.get());
  return frame_scheduler;
}

std::unique_ptr<blink::WebFrameScheduler>
WebViewSchedulerImpl::createFrameScheduler(blink::BlameContext* blame_context) {
  return createWebFrameSchedulerImpl(blame_context);
}

void WebViewSchedulerImpl::Unregister(WebFrameSchedulerImpl* frame_scheduler) {
  DCHECK(frame_schedulers_.find(frame_scheduler) != frame_schedulers_.end());
  frame_schedulers_.erase(frame_scheduler);
}

void WebViewSchedulerImpl::AddConsoleWarning(const std::string& message) {
  if (!web_view_ || !web_view_->mainFrame())
    return;
  blink::WebConsoleMessage console_message(
      blink::WebConsoleMessage::LevelWarning,
      blink::WebString::fromUTF8(message));
  web_view_->mainFrame()->addMessageToConsole(console_message);
}

void WebViewSchedulerImpl::enableVirtualTime() {
  // If we've already switched to virtual time then we don't need to do
  // anything more.
  if (virtual_time_domain_.get())
    return;

  virtual_time_domain_.reset(new AutoAdvancingVirtualTimeDomain(
      renderer_scheduler_->tick_clock()->NowTicks()));
  renderer_scheduler_->RegisterTimeDomain(virtual_time_domain_.get());

  virtual_time_domain_->SetCanAdvanceVirtualTime(
      allow_virtual_time_to_advance_);

  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->OnVirtualTimeDomainChanged();
  }
}

void WebViewSchedulerImpl::setAllowVirtualTimeToAdvance(
    bool allow_virtual_time_to_advance) {
  if (allow_virtual_time_to_advance_ == allow_virtual_time_to_advance)
    return;

  allow_virtual_time_to_advance_ = allow_virtual_time_to_advance;

  if (virtual_time_domain_) {
    virtual_time_domain_->SetCanAdvanceVirtualTime(
        allow_virtual_time_to_advance);
  }
}

}  // namespace scheduler
