// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/browser/loader/temporary_file_stream.h"
#include "content/common/content_export.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace net {
class FileStream;
class GrowableIOBuffer;
}

namespace storage {
class ShareableFileReference;
}

namespace content {

// Redirects network data to a file.  This is intended to be layered in front of
// either the AsyncResourceHandler or the SyncResourceHandler.  The downstream
// resource handler does not see OnWillRead or OnReadCompleted calls. Instead,
// the ResourceResponse contains the path to a temporary file and
// OnDataDownloaded is called as the file downloads.
class CONTENT_EXPORT RedirectToFileResourceHandler
    : public LayeredResourceHandler {
 public:
  typedef base::Callback<void(const CreateTemporaryFileStreamCallback&)>
      CreateTemporaryFileStreamFunction;

  // Create a RedirectToFileResourceHandler for |request| which wraps
  // |next_handler|.
  RedirectToFileResourceHandler(std::unique_ptr<ResourceHandler> next_handler,
                                net::URLRequest* request);
  ~RedirectToFileResourceHandler() override;

  // Replace the CreateTemporaryFileStream implementation with a mocked one for
  // testing purposes. The function should create a net::FileStream and a
  // ShareableFileReference and then asynchronously pass them to the
  // CreateTemporaryFileStreamCallback.
  void SetCreateTemporaryFileStreamFunctionForTesting(
      const CreateTemporaryFileStreamFunction& create_temporary_file_stream);

  // LayeredResourceHandler implementation:
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL& url, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;

 private:
  void DidCreateTemporaryFile(base::File::Error error_code,
                              std::unique_ptr<net::FileStream> file_stream,
                              storage::ShareableFileReference* deletable_file);

  // Called by RedirectToFileResourceHandler::Writer.
  void DidWriteToFile(int result);

  bool WriteMore();
  bool BufIsFull() const;
  void ResumeIfDeferred();

  CreateTemporaryFileStreamFunction create_temporary_file_stream_;

  // We allocate a single, fixed-size IO buffer (buf_) used to read from the
  // network (buf_write_pending_ is true while the system is copying data into
  // buf_), and then write this buffer out to disk (write_callback_pending_ is
  // true while writing to disk).  Reading from the network is suspended while
  // the buffer is full (BufIsFull returns true).  The write_cursor_ member
  // tracks the offset into buf_ that we are writing to disk.

  scoped_refptr<net::GrowableIOBuffer> buf_;
  bool buf_write_pending_;
  int write_cursor_;

  // Helper writer object which maintains references to the net::FileStream and
  // storage::ShareableFileReference. This is maintained separately so that,
  // on Windows, the temporary file isn't deleted until after it is closed.
  class Writer;
  Writer* writer_;

  // |next_buffer_size_| is the size of the buffer to be allocated on the next
  // OnWillRead() call.  We exponentially grow the size of the buffer allocated
  // when our owner fills our buffers. On the first OnWillRead() call, we
  // allocate a buffer of 32k and double it in OnReadCompleted() if the buffer
  // was filled, up to a maximum size of 512k.
  int next_buffer_size_;

  bool did_defer_;

  bool completed_during_write_;
  GURL will_start_url_;
  net::URLRequestStatus completed_status_;
  std::string completed_security_info_;

  base::WeakPtrFactory<RedirectToFileResourceHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RedirectToFileResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_REDIRECT_TO_FILE_RESOURCE_HANDLER_H_
