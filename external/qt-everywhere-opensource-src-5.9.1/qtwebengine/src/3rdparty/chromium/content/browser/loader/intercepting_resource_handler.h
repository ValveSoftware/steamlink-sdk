// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_INTERCEPTING_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_INTERCEPTING_RESOURCE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/browser/loader/resource_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_controller.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request_status.h"

namespace net {
class URLRequest;
}

namespace content {

// ResourceHandler that initiates special handling of the response if needed,
// based on the response's MIME type (starts downloads, sends data to some
// plugin types via a special channel).
//
// An InterceptingResourceHandler holds two handlers (|next_handler| and
// |new_handler|). It assumes the following:
//  - OnResponseStarted on |next_handler| never sets |*defer|.
//  - OnResponseCompleted on |next_handler| never sets |*defer|.
class CONTENT_EXPORT InterceptingResourceHandler
    : public LayeredResourceHandler,
      public ResourceController {
 public:
  InterceptingResourceHandler(std::unique_ptr<ResourceHandler> next_handler,
                              net::URLRequest* request);
  ~InterceptingResourceHandler() override;

  // ResourceHandler implementation:
  void SetController(ResourceController* controller) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           bool* defer) override;

  // ResourceController implementation:
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;
  void Resume() override;

  // Replaces the next handler with |new_handler|, sending
  // |payload_for_old_handler| to the old handler. Must be called after
  // OnWillStart and OnRequestRedirected and before OnResponseStarted. One
  // OnWillRead call is permitted beforehand. |new_handler|'s OnWillStart and
  // OnRequestRedirected methods will not be called.
  void UseNewHandler(std::unique_ptr<ResourceHandler> new_handler,
                     const std::string& payload_for_old_handler);

  // Used in tests.
  ResourceHandler* new_handler_for_testing() const {
    return new_handler_.get();
  }

 private:
  enum class State {
    // The InterceptingResourceHandler is waiting for the mime type of the
    // response to be identified, to check if the next handler should be
    // replaced with an appropriate one.
    STARTING,

    // The InterceptingResourceHandler is sending the payload given via
    // UseNewHandler to the old handler and waiting for its completion via
    // Resume().
    SENDING_PAYLOAD_TO_OLD_HANDLER,

    // The InterceptingResourcHandler is calling the new handler's
    // OnResponseStarted method and waiting for its completion via Resume().
    // After completion, the InterceptingResourceHandler will transition to
    // SENDING_ON_RESPONSE_STARTED_TO_NEW_HANDLER on success.
    SENDING_ON_WILL_START_TO_NEW_HANDLER,

    // The InterceptingResourcHandler is calling the new handler's
    // OnResponseStarted method and waiting for its completion via Resume().
    // After completion, the InterceptingResourceHandler will transition to
    // WAITING_FOR_ON_READ_COMPLETED on success.
    SENDING_ON_RESPONSE_STARTED_TO_NEW_HANDLER,

    // The InterceptingResourcHandler is waiting for OnReadCompleted to be
    // called.
    WAITING_FOR_ON_READ_COMPLETED,

    // The InterceptingResourceHandler is sending the old handler's contents to
    // the new handler and waiting for its completion via Resume().
    SENDING_BUFFER_TO_NEW_HANDLER,

    // The InterceptingResourceHandler has replaced its next ResourceHandler if
    // needed, and has ensured the buffered read data was properly transmitted
    // to the new ResourceHandler. The InterceptingResourceHandler now acts as
    // a pass-through ResourceHandler.
    PASS_THROUGH,
  };

  // Runs necessary operations depending on |state_|. Returns false when an
  // error happens, and set |*defer| to true if the operation continues upon
  // return.
  bool DoLoop(bool* defer);

  // The return value and |defer| has the same meaning as DoLoop.
  bool SendPayloadToOldHandler(bool* defer);
  bool SendFirstReadBufferToNewHandler(bool* defer);
  bool SendOnResponseStartedToNewHandler(bool* defer);

  State state_ = State::STARTING;

  std::unique_ptr<ResourceHandler> new_handler_;
  std::string payload_for_old_handler_;
  size_t payload_bytes_written_ = 0;

  // Result of the first read, that may have to be passed to an alternate
  // ResourceHandler instead of the original ResourceHandler.
  scoped_refptr<net::IOBuffer> first_read_buffer_;
  // Instead of |first_read_buffer_|, this handler creates a new IOBuffer with
  // the same size and return it to the client.
  scoped_refptr<net::IOBuffer> first_read_buffer_double_;
  size_t first_read_buffer_size_ = 0;
  size_t first_read_buffer_bytes_read_ = 0;
  size_t first_read_buffer_bytes_written_ = 0;

  scoped_refptr<ResourceResponse> response_;

  DISALLOW_COPY_AND_ASSIGN(InterceptingResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_INTERCEPTING_RESOURCE_HANDLER_H_
