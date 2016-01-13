// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/file_downloader.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/native_client/src/trusted/plugin/callback_source.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

FileDownloader::FileDownloader(Plugin* instance)
   : instance_(instance),
     file_open_notify_callback_(pp::BlockUntilComplete()),
     stream_finish_callback_(pp::BlockUntilComplete()),
     mode_(DOWNLOAD_NONE),
     data_stream_callback_source_(NULL) {
  callback_factory_.Initialize(this);
  temp_buffer_.resize(kTempBufferSize);
}

bool FileDownloader::OpenStream(
    const nacl::string& url,
    const pp::CompletionCallback& callback,
    StreamCallbackSource* stream_callback_source) {
  data_stream_callback_source_ = stream_callback_source;
  PLUGIN_PRINTF(("FileDownloader::Open (url=%s)\n", url.c_str()));
  if (callback.pp_completion_callback().func == NULL || instance_ == NULL)
    return false;

  status_code_ = -1;
  file_open_notify_callback_ = callback;
  mode_ = DOWNLOAD_TO_BUFFER_AND_STREAM;
  pp::URLRequestInfo url_request(instance_);

  // Allow CORS.
  // Note that "SetAllowCrossOriginRequests" (currently) has the side effect of
  // preventing credentials from being sent on same-origin requests.  We
  // therefore avoid setting this flag unless we know for sure it is a
  // cross-origin request, resulting in behavior similar to XMLHttpRequest.
  if (!instance_->DocumentCanRequest(url))
    url_request.SetAllowCrossOriginRequests(true);

  if (!extra_request_headers_.empty())
    url_request.SetHeaders(extra_request_headers_);

  // Reset the url loader and file reader.
  // Note that we have the only reference to the underlying objects, so
  // this will implicitly close any pending IO and destroy them.
  url_loader_ = pp::URLLoader(instance_);
  url_request.SetRecordDownloadProgress(true);

  // Prepare the url request.
  url_request.SetURL(url);

  // Request asynchronous download of the url providing an on-load callback.
  // As long as this step is guaranteed to be asynchronous, we can call
  // synchronously all other internal callbacks that eventually result in the
  // invocation of the user callback. The user code will not be reentered.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&FileDownloader::URLLoadStartNotify);
  int32_t pp_error = url_loader_.Open(url_request, onload_callback);
  PLUGIN_PRINTF(("FileDownloader::Open (pp_error=%" NACL_PRId32 ")\n",
                 pp_error));
  CHECK(pp_error == PP_OK_COMPLETIONPENDING);
  return true;
}

bool FileDownloader::InitialResponseIsValid() {
  // Process the response, validating the headers to confirm successful loading.
  url_response_ = url_loader_.GetResponseInfo();
  if (url_response_.is_null()) {
    PLUGIN_PRINTF((
        "FileDownloader::InitialResponseIsValid (url_response_=NULL)\n"));
    return false;
  }

  pp::Var full_url = url_response_.GetURL();
  if (!full_url.is_string()) {
    PLUGIN_PRINTF((
        "FileDownloader::InitialResponseIsValid (url is not a string)\n"));
    return false;
  }
  full_url_ = full_url.AsString();

  status_code_ = url_response_.GetStatusCode();
  PLUGIN_PRINTF(("FileDownloader::InitialResponseIsValid ("
                 "response status_code=%" NACL_PRId32 ")\n", status_code_));
  return status_code_ == NACL_HTTP_STATUS_OK;
}

void FileDownloader::URLLoadStartNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    file_open_notify_callback_.RunAndClear(pp_error);
    return;
  }

  if (!InitialResponseIsValid()) {
    file_open_notify_callback_.RunAndClear(PP_ERROR_FAILED);
    return;
  }

  file_open_notify_callback_.RunAndClear(PP_OK);
}

void FileDownloader::BeginStreaming(
    const pp::CompletionCallback& callback) {
  stream_finish_callback_ = callback;

  // Finish streaming the body providing an optional callback.
  pp::CompletionCallback onread_callback =
      callback_factory_.NewOptionalCallback(
          &FileDownloader::URLReadBodyNotify);
  int32_t temp_size = static_cast<int32_t>(temp_buffer_.size());
  int32_t pp_error = url_loader_.ReadResponseBody(&temp_buffer_[0],
                                                  temp_size,
                                                  onread_callback);
  if (pp_error != PP_OK_COMPLETIONPENDING)
    onread_callback.RunAndClear(pp_error);
}

void FileDownloader::URLReadBodyNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLReadBodyNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error < PP_OK) {
    stream_finish_callback_.RunAndClear(pp_error);
  } else if (pp_error == PP_OK) {
    data_stream_callback_source_->GetCallback().RunAndClear(PP_OK);
    stream_finish_callback_.RunAndClear(PP_OK);
  } else {
    PLUGIN_PRINTF(("Running data_stream_callback, temp_buffer_=%p\n",
                   &temp_buffer_[0]));
    StreamCallback cb = data_stream_callback_source_->GetCallback();
    *(cb.output()) = &temp_buffer_;
    cb.RunAndClear(pp_error);

    pp::CompletionCallback onread_callback =
        callback_factory_.NewOptionalCallback(
            &FileDownloader::URLReadBodyNotify);
    int32_t temp_size = static_cast<int32_t>(temp_buffer_.size());
    pp_error = url_loader_.ReadResponseBody(&temp_buffer_[0],
                                            temp_size,
                                            onread_callback);
    if (pp_error != PP_OK_COMPLETIONPENDING)
      onread_callback.RunAndClear(pp_error);
  }
}

bool FileDownloader::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) const {
  return url_loader_.GetDownloadProgress(bytes_received,
                                         total_bytes_to_be_received);
}

nacl::string FileDownloader::GetResponseHeaders() const {
  pp::Var headers = url_response_.GetHeaders();
  if (!headers.is_string()) {
    PLUGIN_PRINTF((
        "FileDownloader::GetResponseHeaders (headers are not a string)\n"));
    return nacl::string();
  }
  return headers.AsString();
}

}  // namespace plugin
