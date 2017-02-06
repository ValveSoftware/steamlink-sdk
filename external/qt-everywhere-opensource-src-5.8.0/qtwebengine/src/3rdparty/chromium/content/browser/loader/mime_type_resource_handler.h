// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_MIME_TYPE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_MIME_TYPE_RESOURCE_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_controller.h"

namespace net {
class URLRequest;
}

namespace content {
class PluginService;
class ResourceDispatcherHostImpl;
struct WebPluginInfo;

// ResourceHandler that, if necessary, buffers a response body without passing
// it to the next ResourceHandler until it can perform mime sniffing on it.
// Once a response's MIME type is known, initiates special handling of the
// response if needed (starts downloads, sends data to some plugin types via a
// special channel).
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
class CONTENT_EXPORT MimeTypeResourceHandler
    : public LayeredResourceHandler,
      public ResourceController {
 public:
  // If ENABLE_PLUGINS is defined, |plugin_service| must not be NULL.
  MimeTypeResourceHandler(std::unique_ptr<ResourceHandler> next_handler,
                          ResourceDispatcherHostImpl* host,
                          PluginService* plugin_service,
                          net::URLRequest* request);
  ~MimeTypeResourceHandler() override;

 private:
  // ResourceHandler implementation:
  void SetController(ResourceController* controller) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL&, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;

  // ResourceController implementation:
  void Resume() override;
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;

  bool ProcessResponse(bool* defer);

  bool ShouldSniffContent();
  bool DetermineMimeType();
  // Determines whether a plugin will handle the current request, and if so,
  // sets up the handler to direct the response to that plugin. Returns false
  // if there is an error and the request should be cancelled and true
  // otherwise. |defer| is set to true if plugin data is stale and needs to be
  // refreshed before the request can be handled (in this case the function
  // still returns true). If the request is directed to a plugin,
  // |handled_by_plugin| is set to true.
  bool SelectPluginHandler(bool* defer, bool* handled_by_plugin);
  // Returns false if the request should be cancelled.
  bool SelectNextHandler(bool* defer);
  bool UseAlternateNextHandler(std::unique_ptr<ResourceHandler> handler,
                               const std::string& payload_for_old_handler);

  bool ReplayReadCompleted(bool* defer);
  void CallReplayReadCompleted();

  bool MustDownload();

  // Copies data from |read_buffer_| to |next_handler_|.
  bool CopyReadBufferToNextHandler();

  // Called on the IO thread once the list of plugins has been loaded.
  void OnPluginsLoaded(const std::vector<WebPluginInfo>& plugins);

  enum State {
    STATE_STARTING,

    // In this state, we are filling read_buffer_ with data for the purpose
    // of sniffing the mime type of the response.
    STATE_BUFFERING,

    // In this state, we are select an appropriate downstream ResourceHandler
    // based on the mime type of the response.  We are also potentially waiting
    // for plugins to load so that we can determine if a plugin is available to
    // handle the mime type.
    STATE_PROCESSING,

    // In this state, we are replaying buffered events (OnResponseStarted and
    // OnReadCompleted) to the downstream ResourceHandler.
    STATE_REPLAYING,

    // In this state, we are just a blind pass-through ResourceHandler.
    STATE_STREAMING
  };
  State state_;

  scoped_refptr<ResourceResponse> response_;
  ResourceDispatcherHostImpl* host_;
#if defined(ENABLE_PLUGINS)
  PluginService* plugin_service_;
#endif
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  int bytes_read_;

  bool must_download_;
  bool must_download_is_set_;

  base::WeakPtrFactory<MimeTypeResourceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MimeTypeResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_MIME_TYPE_RESOURCE_HANDLER_H_
