// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_ASYNC_REVALIDATION_DRIVER_H_
#define CONTENT_BROWSER_LOADER_ASYNC_REVALIDATION_DRIVER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"

namespace net {
class HttpCache;
}

namespace content {

// This class is responsible for driving the URLRequest for an async
// revalidation. It is passed an instance of ResourceThrottle created by
// content::ResourceScheduler to perform throttling on the request.
class CONTENT_EXPORT AsyncRevalidationDriver : public net::URLRequest::Delegate,
                                               public ResourceController {
 public:
  // |completion_callback| is guaranteed to be called on completion,
  // regardless of success or failure.
  AsyncRevalidationDriver(std::unique_ptr<net::URLRequest> request,
                          std::unique_ptr<ResourceThrottle> throttle,
                          const base::Closure& completion_callback);
  ~AsyncRevalidationDriver() override;

  void StartRequest();

 private:
  // This enum is logged as histogram "Net.AsyncRevalidation.Result". Only add
  // new entries at the end and update histograms.xml to match.
  enum AsyncRevalidationResult {
    RESULT_LOADED,
    RESULT_REVALIDATED,
    RESULT_NET_ERROR,
    RESULT_READ_ERROR,
    RESULT_GOT_REDIRECT,
    RESULT_AUTH_FAILED,
    RESULT_RESPONSE_TIMEOUT,
    RESULT_BODY_TIMEOUT,
    RESULT_MAX
  };

  // net::URLRequest::Delegate implementation:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer) override;
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* info) override;
  void OnBeforeNetworkStart(net::URLRequest* request, bool* defer) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // ResourceController implementation:
  void Resume() override;

  // For simplicity, this class assumes that ResourceScheduler never cancels
  // requests, and so these three methods are never called.
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;

  // Internal methods.
  void StartRequestInternal();
  void CancelRequestInternal(int error, AsyncRevalidationResult result);
  void StartReading(bool is_continuation);
  void ReadMore(int* bytes_read);
  // Logs the result histogram, then calls and clears |completion_callback_|.
  void ResponseCompleted(AsyncRevalidationResult result);
  void OnTimeout(AsyncRevalidationResult result);
  void RecordDefer();

  bool is_deferred_ = false;

  scoped_refptr<net::IOBuffer> read_buffer_;
  base::OneShotTimer timer_;

  std::unique_ptr<net::URLRequest> request_;
  std::unique_ptr<ResourceThrottle> throttle_;

  base::Closure completion_callback_;

  base::WeakPtrFactory<AsyncRevalidationDriver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncRevalidationDriver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_ASYNC_REVALIDATION_DRIVER_H_
