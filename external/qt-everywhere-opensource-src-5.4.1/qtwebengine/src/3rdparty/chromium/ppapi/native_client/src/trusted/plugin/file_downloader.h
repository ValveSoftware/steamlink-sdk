// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_

#include <deque>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/public/nacl_file_info.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/native_client/src/trusted/plugin/callback_source.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace plugin {

class Plugin;

typedef enum {
  DOWNLOAD_TO_BUFFER_AND_STREAM = 0,
  DOWNLOAD_NONE
} DownloadMode;

typedef std::vector<char>* FileStreamData;
typedef CallbackSource<FileStreamData> StreamCallbackSource;
typedef pp::CompletionCallbackWithOutput<FileStreamData> StreamCallback;

// A class that wraps PPAPI URLLoader and FileIO functionality for downloading
// the url into a file and providing an open file descriptor.
class FileDownloader {
 public:
  explicit FileDownloader(Plugin* instance);
  ~FileDownloader() {}

  // Issues a GET on |url| to start downloading the response into a file,
  // and finish streaming it. |callback| will be run after streaming is
  // done or if an error prevents streaming from completing.
  // Returns true when callback is scheduled to be called on success or failure.
  // Returns false if callback is NULL, or if the PPB_FileIO_Trusted interface
  // is not available.
  // If |record_progress| is true, then download progress will be recorded,
  // and can be polled through GetDownloadProgress().
  // If |progress_callback| is not NULL and |record_progress| is true,
  // then the callback will be invoked for every progress update received
  // by the loader.

  // Similar to Open(), but used for streaming the |url| data directly to the
  // caller without writing to a temporary file. The callbacks provided by
  // |stream_callback_source| are expected to copy the data before returning.
  // |callback| is called once the response headers are received,
  // and streaming must be completed separately via BeginStreaming().
  bool OpenStream(const nacl::string& url,
                  const pp::CompletionCallback& callback,
                  StreamCallbackSource* stream_callback_source);

  // Finish streaming the response body for a URL request started by either
  // OpenStream(). Runs the given |callback| when streaming is done.
  void BeginStreaming(const pp::CompletionCallback& callback);

  // Once the GET request has finished, and the contents of the file
  // represented by |url_| are available, |full_url_| is the full URL including
  // the scheme, host and full path.
  // Returns an empty string before the GET request has finished.
  const nacl::string& full_url() const { return full_url_; }

  // GetDownloadProgress() returns the current download progress, which is
  // meaningful after Open() has been called. Progress only refers to the
  // response body and does not include the headers.
  //
  // This data is only available if the |record_progress| true in the
  // Open() call.  If progress is being recorded, then |bytes_received|
  // will be set to the number of bytes received thus far,
  // and |total_bytes_to_be_received| will be set to the total number
  // of bytes to be received.  The total bytes to be received may be unknown,
  // in which case |total_bytes_to_be_received| will be set to -1.
  bool GetDownloadProgress(int64_t* bytes_received,
                           int64_t* total_bytes_to_be_received) const;

  int status_code() const { return status_code_; }
  nacl::string GetResponseHeaders() const;

  void set_request_headers(const nacl::string& extra_request_headers) {
    extra_request_headers_ = extra_request_headers;
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(FileDownloader);

  // For DOWNLOAD_TO_BUFFER_AND_STREAM, the process is very similar:
  //   1) Ask the browser to start streaming |url_| to an internal buffer.
  //   2) Ask the browser to finish streaming to |temp_buffer_| on success.
  //   3) Wait for streaming to finish, passing the data directly to the user.
  // Each step is done asynchronously using callbacks.  We create callbacks
  // through a factory to take advantage of ref-counting.
  // The public Open*() functions start step 1), and the public BeginStreaming
  // function proceeds to step 2) and 3).
  bool InitialResponseIsValid();
  void URLLoadStartNotify(int32_t pp_error);
  void URLReadBodyNotify(int32_t pp_error);

  Plugin* instance_;
  nacl::string full_url_;

  nacl::string extra_request_headers_;
  pp::URLResponseInfo url_response_;
  pp::CompletionCallback file_open_notify_callback_;
  pp::CompletionCallback stream_finish_callback_;
  pp::URLLoader url_loader_;
  pp::CompletionCallbackFactory<FileDownloader> callback_factory_;
  int32_t status_code_;
  DownloadMode mode_;
  static const uint32_t kTempBufferSize = 16384;
  std::vector<char> temp_buffer_;
  StreamCallbackSource* data_stream_callback_source_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_
