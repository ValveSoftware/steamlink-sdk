// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_RESOURCE_HANDLER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/save_types.h"
#include "content/browser/loader/resource_handler.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {
class SaveFileManager;

// Forwards data to the save thread.
class SaveFileResourceHandler : public ResourceHandler {
 public:
  // Unauthorized requests are cancelled from OnWillStart callback.
  //
  // This way of handling unauthorized requests allows unified handling of all
  // SaveFile requests - communicating the failure to OnResponseCompleted
  // happens in a generic, typical way, reusing common infrastructure code
  // (rather than forcing an ad-hoc, Save-File-specific call to
  // OnResponseCompleted from ResourceDispatcherHostImpl::BeginSaveFile).
  enum class AuthorizationState {
    AUTHORIZED,
    NOT_AUTHORIZED,
  };

  SaveFileResourceHandler(net::URLRequest* request,
                          SaveItemId save_item_id,
                          SavePackageId save_package_id,
                          int render_process_host_id,
                          int render_frame_routing_id,
                          const GURL& url,
                          SaveFileManager* manager,
                          AuthorizationState authorization_state);
  ~SaveFileResourceHandler() override;

  // ResourceHandler Implementation:

  // Saves the redirected URL to final_url_, we need to use the original
  // URL to match original request.
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;

  // Sends the download creation information to the download thread.
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;

  // Pass-through implementation.
  bool OnWillStart(const GURL& url, bool* defer) override;

  // Pass-through implementation.
  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override;

  // Creates a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;

  // Passes the buffer to the download file writer.
  bool OnReadCompleted(int bytes_read, bool* defer) override;

  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;

  // N/A to this flavor of SaveFileResourceHandler.
  void OnDataDownloaded(int bytes_downloaded) override;

  // If the content-length header is not present (or contains something other
  // than numbers), StringToInt64 returns 0, which indicates 'unknown size' and
  // is handled correctly by the SaveManager.
  void set_content_length(const std::string& content_length);

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

 private:
  SaveItemId save_item_id_;
  SavePackageId save_package_id_;
  int render_process_id_;
  int render_frame_routing_id_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  std::string content_disposition_;
  GURL url_;
  GURL final_url_;
  int64_t content_length_;
  SaveFileManager* save_manager_;

  AuthorizationState authorization_state_;

  static const int kReadBufSize = 32768;  // bytes

  DISALLOW_COPY_AND_ASSIGN(SaveFileResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_RESOURCE_HANDLER_H_
