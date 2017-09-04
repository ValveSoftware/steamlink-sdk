// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_MOJO_ASYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_MOJO_ASYNC_RESOURCE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "content/browser/loader/resource_handler.h"
#include "content/common/content_export.h"
#include "content/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/system/watcher.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceDispatcherHostImpl;
struct ResourceResponse;

// Used to complete an asynchronous resource request in response to resource
// load events from the resource dispatcher host. This class is used only
// when LoadingWithMojo runtime flag is enabled.
//
// TODO(yhirano): Add histograms.
// TODO(yhirano): Send cached metadata.
//
// This class can be inherited only for tests.
class CONTENT_EXPORT MojoAsyncResourceHandler
    : public ResourceHandler,
      public NON_EXPORTED_BASE(mojom::URLLoader) {
 public:
  MojoAsyncResourceHandler(
      net::URLRequest* request,
      ResourceDispatcherHostImpl* rdh,
      mojom::URLLoaderAssociatedRequest mojo_request,
      mojom::URLLoaderClientAssociatedPtr url_loader_client);
  ~MojoAsyncResourceHandler() override;

  // ResourceHandler implementation:
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL& url, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           bool* defer) override;
  void OnDataDownloaded(int bytes_downloaded) override;

  // mojom::URLLoader implementation
  void FollowRedirect() override;

  void OnWritableForTesting();
  static void SetAllocationSizeForTesting(size_t size);
  static constexpr size_t kDefaultAllocationSize = 512 * 1024;

 protected:
  // These functions can be overriden only for tests.
  virtual MojoResult BeginWrite(void** data, uint32_t* available);
  virtual MojoResult EndWrite(uint32_t written);

 private:
  class SharedWriter;
  class WriterIOBuffer;

  // This funcion copies data stored in |buffer_| to |shared_writer_| and
  // resets |buffer_| to a WriterIOBuffer when all bytes are copied. Returns
  // true when done successfully.
  bool CopyReadDataToDataPipe(bool* defer);
  // Allocates a WriterIOBuffer and set it to |*buf|. Returns true when done
  // successfully.
  bool AllocateWriterIOBuffer(scoped_refptr<net::IOBufferWithSize>* buf,
                              bool* defer);

  bool CheckForSufficientResource();
  void OnWritable(MojoResult result);
  void Cancel();
  // This function can be overriden only for tests.
  virtual void ReportBadMessage(const std::string& error);

  ResourceDispatcherHostImpl* rdh_;
  mojo::AssociatedBinding<mojom::URLLoader> binding_;

  bool has_checked_for_sufficient_resources_ = false;
  bool sent_received_response_message_ = false;
  bool is_using_io_buffer_not_from_writer_ = false;
  bool did_defer_on_writing_ = false;
  bool did_defer_on_redirect_ = false;
  base::TimeTicks response_started_ticks_;
  int64_t reported_total_received_bytes_ = 0;

  mojo::Watcher handle_watcher_;
  std::unique_ptr<mojom::URLLoader> url_loader_;
  mojom::URLLoaderClientAssociatedPtr url_loader_client_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
  size_t buffer_offset_ = 0;
  size_t buffer_bytes_read_ = 0;
  scoped_refptr<SharedWriter> shared_writer_;

  DISALLOW_COPY_AND_ASSIGN(MojoAsyncResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_MOJO_ASYNC_RESOURCE_HANDLER_H_
