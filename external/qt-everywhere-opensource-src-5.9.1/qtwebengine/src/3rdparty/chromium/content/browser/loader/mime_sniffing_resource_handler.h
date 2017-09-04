// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_MIME_SNIFFING_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_MIME_SNIFFING_RESOURCE_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/common/request_context_type.h"

namespace net {
class URLRequest;
}

namespace content {
class InterceptingResourceHandler;
class PluginService;
class ResourceDispatcherHostImpl;
struct WebPluginInfo;

// ResourceHandler that, if necessary, buffers a response body without passing
// it to the next ResourceHandler until it can perform mime sniffing on it.
//
// Uses the buffer provided by the original event handler for buffering, and
// continues to reuses it until it can determine the MIME type
// subsequent reads until it's done buffering.  As a result, the buffer
// returned by the next ResourceHandler must have a capacity of at least
// net::kMaxBytesToSniff * 2.
//
// Before a request is sent, this ResourceHandler will also set an appropriate
// Accept header on the request based on its ResourceType, if one isn't already
// present.
class CONTENT_EXPORT MimeSniffingResourceHandler
    : public LayeredResourceHandler,
      public ResourceController {
 public:
  MimeSniffingResourceHandler(std::unique_ptr<ResourceHandler> next_handler,
                              ResourceDispatcherHostImpl* host,
                              PluginService* plugin_service,
                              InterceptingResourceHandler* intercepting_handler,
                              net::URLRequest* request,
                              RequestContextType request_context_type);
  ~MimeSniffingResourceHandler() override;

 private:
  friend class MimeSniffingResourceHandlerTest;
  enum State {
    // Starting state of the MimeSniffingResourceHandler. In this state, it is
    // acting as a blind pass-through ResourceHandler until the response is
    // received.
    STATE_STARTING,

    // In this state, the MimeSniffingResourceHandler is buffering the response
    // data in read_buffer_, waiting to sniff the mime type and make a choice
    // about request interception.
    STATE_BUFFERING,

    // In this state, the MimeSniffingResourceHandler has identified the mime
    // type and made a decision on whether the request should be intercepted or
    // not. It is nows attempting to replay the response to downstream
    // handlers.
    STATE_INTERCEPTION_CHECK_DONE,

    // In this state, the MimeSniffingResourceHandler is replaying the buffered
    // OnResponseStarted event to the downstream ResourceHandlers.
    STATE_REPLAYING_RESPONSE_RECEIVED,

    // In this state, the MimeSniffingResourceHandler is just a blind
    // pass-through
    // ResourceHandler.
    STATE_STREAMING,
  };

  // ResourceHandler implementation:
  void SetController(ResourceController* controller) override;
  bool OnWillStart(const GURL&, bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           bool* defer) override;

  // ResourceController implementation:
  void Resume() override;
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;

  // --------------------------------------------------------------------------
  // The following methods replay the buffered data to the downstream
  // ResourceHandlers. They return false if the request should be cancelled,
  // true otherwise. Each of them will set |defer| to true if the request will
  // proceed to the next stage asynchronously.

  // Used to advance through the states of the state machine.
  void AdvanceState();
  bool ProcessState(bool* defer);

  // Intercepts the request as a stream/download if needed.
  bool MaybeIntercept(bool* defer);

  // Replays OnResponseStarted on the downstream handlers.
  bool ReplayResponseReceived(bool* defer);

  // Replays OnReadCompleted on the downstreams handlers.
  bool ReplayReadCompleted(bool* defer);

  // --------------------------------------------------------------------------

  // Whether the response body should be sniffed in order to determine the MIME
  // type of the response.
  bool ShouldSniffContent();

  // Checks whether this request should be intercepted as a stream or a
  // download. If this is the case, sets up the new ResourceHandler that will be
  // used for interception. Returns false if teh request should be cancelled,
  // true otherwise. |defer| is set to true if the interception check needs to
  // finish asynchronously.
  bool MaybeStartInterception(bool* defer);

  // Determines whether a plugin will handle the current request. Returns false
  // if there is an error and the request should be cancelled and true
  // otherwise. |defer| is set to true if plugin data is stale and needs to be
  // refreshed before the request can be handled (in this case the function
  // still returns true). If the request is directed to a plugin,
  // |handled_by_plugin| is set to true.
  bool CheckForPluginHandler(bool* defer, bool* handled_by_plugin);

  // Whether this request is allowed to be intercepted as a download or a
  // stream.
  bool CanBeIntercepted();

  // Whether the response we received is not provisional.
  bool CheckResponseIsNotProvisional();

  bool MustDownload();

  // Called on the IO thread once the list of plugins has been loaded.
  void OnPluginsLoaded(const std::vector<WebPluginInfo>& plugins);

  State state_;

  ResourceDispatcherHostImpl* host_;
#if defined(ENABLE_PLUGINS)
  PluginService* plugin_service_;
#endif

  bool must_download_;
  bool must_download_is_set_;

  // Used to buffer the reponse received until replay.
  scoped_refptr<ResourceResponse> response_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  int bytes_read_;

  // The InterceptingResourceHandler that will perform ResourceHandler swap if
  // needed.
  InterceptingResourceHandler* intercepting_handler_;

  RequestContextType request_context_type_;

  base::WeakPtrFactory<MimeSniffingResourceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MimeSniffingResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_MIME_SNIFFING_RESOURCE_HANDLER_H_
