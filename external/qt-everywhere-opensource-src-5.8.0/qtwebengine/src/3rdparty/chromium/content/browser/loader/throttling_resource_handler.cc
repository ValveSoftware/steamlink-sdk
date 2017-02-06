// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/throttling_resource_handler.h"

#include <utility>

#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_response.h"
#include "net/url_request/url_request.h"

namespace content {

ThrottlingResourceHandler::ThrottlingResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request,
    ScopedVector<ResourceThrottle> throttles)
    : LayeredResourceHandler(request, std::move(next_handler)),
      deferred_stage_(DEFERRED_NONE),
      throttles_(std::move(throttles)),
      next_index_(0),
      cancelled_by_resource_throttle_(false) {
  for (size_t i = 0; i < throttles_.size(); ++i) {
    throttles_[i]->set_controller(this);
    // Throttles must have a name, as otherwise, bugs where a throttle fails
    // to resume a request can be very difficult to debug.
    DCHECK(throttles_[i]->GetNameForLogging());
  }
}

ThrottlingResourceHandler::~ThrottlingResourceHandler() {
}

bool ThrottlingResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  DCHECK(!cancelled_by_resource_throttle_);

  *defer = false;
  while (next_index_ < throttles_.size()) {
    int index = next_index_;
    throttles_[index]->WillRedirectRequest(redirect_info, defer);
    next_index_++;
    if (cancelled_by_resource_throttle_)
      return false;
    if (*defer) {
      OnRequestDefered(index);
      deferred_stage_ = DEFERRED_REDIRECT;
      deferred_redirect_ = redirect_info;
      deferred_response_ = response;
      return true;  // Do not cancel.
    }
  }

  next_index_ = 0;  // Reset for next time.

  return next_handler_->OnRequestRedirected(redirect_info, response, defer);
}

bool ThrottlingResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  DCHECK(!cancelled_by_resource_throttle_);

  *defer = false;
  while (next_index_ < throttles_.size()) {
    int index = next_index_;
    throttles_[index]->WillStartRequest(defer);
    next_index_++;
    if (cancelled_by_resource_throttle_)
      return false;
    if (*defer) {
      OnRequestDefered(index);
      deferred_stage_ = DEFERRED_START;
      deferred_url_ = url;
      return true;  // Do not cancel.
    }
  }

  next_index_ = 0;  // Reset for next time.

  return next_handler_->OnWillStart(url, defer);
}

bool ThrottlingResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                     bool* defer) {
  DCHECK(!cancelled_by_resource_throttle_);

  *defer = false;
  while (next_index_ < throttles_.size()) {
    int index = next_index_;
    throttles_[index]->WillStartUsingNetwork(defer);
    next_index_++;
    if (cancelled_by_resource_throttle_)
      return false;
    if (*defer) {
      OnRequestDefered(index);
      deferred_stage_ = DEFERRED_NETWORK_START;
      deferred_url_ = url;
      return true;  // Do not cancel.
    }
  }

  next_index_ = 0;  // Reset for next time.

  return next_handler_->OnBeforeNetworkStart(url, defer);
}

bool ThrottlingResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                  bool* defer) {
  DCHECK(!cancelled_by_resource_throttle_);

  while (next_index_ < throttles_.size()) {
    int index = next_index_;
    throttles_[index]->WillProcessResponse(defer);
    next_index_++;
    if (cancelled_by_resource_throttle_)
      return false;
    if (*defer) {
      OnRequestDefered(index);
      deferred_stage_ = DEFERRED_RESPONSE;
      deferred_response_ = response;
      return true;  // Do not cancel.
    }
  }

  next_index_ = 0;  // Reset for next time.

  return next_handler_->OnResponseStarted(response, defer);
}

void ThrottlingResourceHandler::Cancel() {
  cancelled_by_resource_throttle_ = true;
  controller()->Cancel();
}

void ThrottlingResourceHandler::CancelAndIgnore() {
  cancelled_by_resource_throttle_ = true;
  controller()->CancelAndIgnore();
}

void ThrottlingResourceHandler::CancelWithError(int error_code) {
  cancelled_by_resource_throttle_ = true;
  controller()->CancelWithError(error_code);
}

void ThrottlingResourceHandler::Resume() {
  DCHECK(!cancelled_by_resource_throttle_);

  DeferredStage last_deferred_stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  // Clear information about the throttle that delayed the request.
  request()->LogUnblocked();
  switch (last_deferred_stage) {
    case DEFERRED_NONE:
      NOTREACHED();
      break;
    case DEFERRED_START:
      ResumeStart();
      break;
    case DEFERRED_NETWORK_START:
      ResumeNetworkStart();
      break;
    case DEFERRED_REDIRECT:
      ResumeRedirect();
      break;
    case DEFERRED_RESPONSE:
      ResumeResponse();
      break;
  }
}

void ThrottlingResourceHandler::ResumeStart() {
  DCHECK(!cancelled_by_resource_throttle_);

  GURL url = deferred_url_;
  deferred_url_ = GURL();

  bool defer = false;
  if (!OnWillStart(url, &defer)) {
    controller()->Cancel();
  } else if (!defer) {
    controller()->Resume();
  }
}

void ThrottlingResourceHandler::ResumeNetworkStart() {
  DCHECK(!cancelled_by_resource_throttle_);

  GURL url = deferred_url_;
  deferred_url_ = GURL();

  bool defer = false;
  if (!OnBeforeNetworkStart(url, &defer)) {
    controller()->Cancel();
  } else if (!defer) {
    controller()->Resume();
  }
}

void ThrottlingResourceHandler::ResumeRedirect() {
  DCHECK(!cancelled_by_resource_throttle_);

  net::RedirectInfo redirect_info = deferred_redirect_;
  deferred_redirect_ = net::RedirectInfo();
  scoped_refptr<ResourceResponse> response;
  deferred_response_.swap(response);

  bool defer = false;
  if (!OnRequestRedirected(redirect_info, response.get(), &defer)) {
    controller()->Cancel();
  } else if (!defer) {
    controller()->Resume();
  }
}

void ThrottlingResourceHandler::ResumeResponse() {
  DCHECK(!cancelled_by_resource_throttle_);

  scoped_refptr<ResourceResponse> response;
  deferred_response_.swap(response);

  bool defer = false;
  if (!OnResponseStarted(response.get(), &defer)) {
    controller()->Cancel();
  } else if (!defer) {
    controller()->Resume();
  }
}

void ThrottlingResourceHandler::OnRequestDefered(int throttle_index) {
  request()->LogBlockedBy(throttles_[throttle_index]->GetNameForLogging());
}

}  // namespace content
