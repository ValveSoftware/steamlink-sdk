// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_DETACHABLE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_DETACHABLE_RESOURCE_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/loader/resource_handler.h"
#include "content/public/browser/resource_controller.h"

namespace net {
class IOBuffer;
class URLRequest;
}  // namespace net

namespace content {

// A ResourceHandler which delegates all calls to the next handler, unless
// detached. Once detached, it drives the request to completion itself. This is
// used for requests which outlive the owning renderer, such as <link
// rel=prefetch> and <a ping>. Requests do not start out detached so, e.g.,
// prefetches appear in DevTools and get placed in the renderer's local
// cache. If the request does not complete after a timeout on detach, it is
// cancelled.
//
// Note that, once detached, the request continues without the original next
// handler, so any policy decisions in that handler are skipped.
class DetachableResourceHandler : public ResourceHandler,
                                  public ResourceController {
 public:
  DetachableResourceHandler(net::URLRequest* request,
                            base::TimeDelta cancel_delay,
                            scoped_ptr<ResourceHandler> next_handler);
  virtual ~DetachableResourceHandler();

  bool is_detached() const { return next_handler_ == NULL; }
  void Detach();

  void set_cancel_delay(base::TimeDelta cancel_delay) {
    cancel_delay_ = cancel_delay;
  }

  // ResourceHandler implementation:
  virtual void SetController(ResourceController* controller) OVERRIDE;
  virtual bool OnUploadProgress(uint64 position, uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(const GURL& url, bool* defer) OVERRIDE;
  virtual bool OnBeforeNetworkStart(const GURL& url, bool* defer) OVERRIDE;
  virtual bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int bytes_read, bool* defer) OVERRIDE;
  virtual void OnResponseCompleted(const net::URLRequestStatus& status,
                                   const std::string& security_info,
                                   bool* defer) OVERRIDE;
  virtual void OnDataDownloaded(int bytes_downloaded) OVERRIDE;

  // ResourceController implementation:
  virtual void Resume() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void CancelAndIgnore() OVERRIDE;
  virtual void CancelWithError(int error_code) OVERRIDE;

 private:
  scoped_ptr<ResourceHandler> next_handler_;
  scoped_refptr<net::IOBuffer> read_buffer_;

  scoped_ptr<base::OneShotTimer<DetachableResourceHandler> > detached_timer_;
  base::TimeDelta cancel_delay_;

  bool is_deferred_;
  bool is_finished_;

  DISALLOW_COPY_AND_ASSIGN(DetachableResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_DETACHABLE_RESOURCE_HANDLER_H_
