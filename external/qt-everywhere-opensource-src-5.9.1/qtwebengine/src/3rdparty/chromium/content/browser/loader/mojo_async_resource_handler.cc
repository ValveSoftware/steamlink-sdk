// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mojo_async_resource_handler.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/loader/netlog_observer.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/common/resource_request_completion_status.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/url_request/redirect_info.h"

namespace content {
namespace {

int g_allocation_size = MojoAsyncResourceHandler::kDefaultAllocationSize;

// MimeTypeResourceHandler *implicitly* requires that the buffer size
// returned from OnWillRead should be larger than certain size.
// TODO(yhirano): Fix MimeTypeResourceHandler.
constexpr size_t kMinAllocationSize = 2 * net::kMaxBytesToSniff;

constexpr size_t kMaxChunkSize = 32 * 1024;

void GetNumericArg(const std::string& name, int* result) {
  const std::string& value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(name);
  if (!value.empty())
    base::StringToInt(value, result);
}

void InitializeResourceBufferConstants() {
  static bool did_init = false;
  if (did_init)
    return;
  did_init = true;

  GetNumericArg("resource-buffer-size", &g_allocation_size);
}

}  // namespace

// This class is for sharing the ownership of a ScopedDataPipeProducerHandle
// between WriterIOBuffer and MojoAsyncResourceHandler.
class MojoAsyncResourceHandler::SharedWriter final
    : public base::RefCountedThreadSafe<SharedWriter> {
 public:
  explicit SharedWriter(mojo::ScopedDataPipeProducerHandle writer)
      : writer_(std::move(writer)) {}
  mojo::DataPipeProducerHandle writer() { return writer_.get(); }

 private:
  friend class base::RefCountedThreadSafe<SharedWriter>;
  ~SharedWriter() {}

  const mojo::ScopedDataPipeProducerHandle writer_;

  DISALLOW_COPY_AND_ASSIGN(SharedWriter);
};

// This class is a IOBuffer subclass for data gotten from a
// ScopedDataPipeProducerHandle.
class MojoAsyncResourceHandler::WriterIOBuffer final
    : public net::IOBufferWithSize {
 public:
  // |data| and |size| should be gotten from |writer| via BeginWriteDataRaw.
  // They will be accesible via IOBuffer methods. As |writer| is stored in this
  // instance, |data| will be kept valid as long as the following conditions
  // hold:
  //  1. |data| is not invalidated via EndWriteDataRaw.
  //  2. |this| instance is alive.
  WriterIOBuffer(scoped_refptr<SharedWriter> writer, void* data, size_t size)
      : net::IOBufferWithSize(static_cast<char*>(data), size),
        writer_(std::move(writer)) {}

 private:
  ~WriterIOBuffer() override {
    // Avoid deleting |data_| in the IOBuffer destructor.
    data_ = nullptr;
  }

  // This member is for keeping the writer alive.
  scoped_refptr<SharedWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(WriterIOBuffer);
};

MojoAsyncResourceHandler::MojoAsyncResourceHandler(
    net::URLRequest* request,
    ResourceDispatcherHostImpl* rdh,
    mojom::URLLoaderAssociatedRequest mojo_request,
    mojom::URLLoaderClientAssociatedPtr url_loader_client)
    : ResourceHandler(request),
      rdh_(rdh),
      binding_(this, std::move(mojo_request)),
      url_loader_client_(std::move(url_loader_client)) {
  DCHECK(url_loader_client_);
  InitializeResourceBufferConstants();
  // This unretained pointer is safe, because |binding_| is owned by |this| and
  // the callback will never be called after |this| is destroyed.
  binding_.set_connection_error_handler(
      base::Bind(&MojoAsyncResourceHandler::Cancel, base::Unretained(this)));
}

MojoAsyncResourceHandler::~MojoAsyncResourceHandler() {
  if (has_checked_for_sufficient_resources_)
    rdh_->FinishedWithResourcesForRequest(request());
}

bool MojoAsyncResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  // Unlike OnResponseStarted, OnRequestRedirected will NOT be preceded by
  // OnWillRead.
  DCHECK(!shared_writer_);

  *defer = true;
  request()->LogBlockedBy("MojoAsyncResourceHandler");
  did_defer_on_redirect_ = true;

  NetLogObserver::PopulateResponseInfo(request(), response);
  response->head.encoded_data_length = request()->GetTotalReceivedBytes();
  response->head.request_start = request()->creation_time();
  response->head.response_start = base::TimeTicks::Now();
  // TODO(davidben): Is it necessary to pass the new first party URL for
  // cookies? The only case where it can change is top-level navigation requests
  // and hopefully those will eventually all be owned by the browser. It's
  // possible this is still needed while renderer-owned ones exist.
  url_loader_client_->OnReceiveRedirect(redirect_info, response->head);
  return true;
}

bool MojoAsyncResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                 bool* defer) {
  const ResourceRequestInfoImpl* info = GetRequestInfo();

  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(request(), info->GetContext(),
                                        response);
  }

  NetLogObserver::PopulateResponseInfo(request(), response);

  response->head.request_start = request()->creation_time();
  response->head.response_start = base::TimeTicks::Now();
  sent_received_response_message_ = true;
  url_loader_client_->OnReceiveResponse(response->head);
  return true;
}

bool MojoAsyncResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool MojoAsyncResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                          int* buf_size,
                                          int min_size) {
  DCHECK_EQ(-1, min_size);

  if (!CheckForSufficientResource())
    return false;

  if (!shared_writer_) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = g_allocation_size;
    mojo::DataPipe data_pipe(options);

    url_loader_client_->OnStartLoadingResponseBody(
        std::move(data_pipe.consumer_handle));
    if (!data_pipe.producer_handle.is_valid())
      return false;

    shared_writer_ = new SharedWriter(std::move(data_pipe.producer_handle));
    handle_watcher_.Start(shared_writer_->writer(), MOJO_HANDLE_SIGNAL_WRITABLE,
                          base::Bind(&MojoAsyncResourceHandler::OnWritable,
                                     base::Unretained(this)));

    bool defer = false;
    scoped_refptr<net::IOBufferWithSize> buffer;
    if (!AllocateWriterIOBuffer(&buffer, &defer))
      return false;
    if (!defer) {
      if (static_cast<size_t>(buffer->size()) >= kMinAllocationSize) {
        *buf = buffer_ = buffer;
        *buf_size = buffer_->size();
        return true;
      }

      // The allocated buffer is too small.
      if (EndWrite(0) != MOJO_RESULT_OK)
        return false;
    }
    DCHECK(!is_using_io_buffer_not_from_writer_);
    is_using_io_buffer_not_from_writer_ = true;
    buffer_ = new net::IOBufferWithSize(kMinAllocationSize);
  }

  DCHECK_EQ(0u, buffer_offset_);
  *buf = buffer_;
  *buf_size = buffer_->size();
  return true;
}

bool MojoAsyncResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_GE(bytes_read, 0);
  DCHECK(buffer_);

  if (!bytes_read)
    return true;

  if (is_using_io_buffer_not_from_writer_) {
    // Couldn't allocate a buffer on the data pipe in OnWillRead.
    DCHECK_EQ(0u, buffer_bytes_read_);
    buffer_bytes_read_ = bytes_read;
    if (!CopyReadDataToDataPipe(defer))
      return false;
    if (*defer) {
      request()->LogBlockedBy("MojoAsyncResourceHandler");
      did_defer_on_writing_ = true;
    }
    return true;
  }

  if (EndWrite(bytes_read) != MOJO_RESULT_OK)
    return false;
  // Allocate a buffer for the next OnWillRead call here, because OnWillRead
  // doesn't have |defer| parameter.
  if (!AllocateWriterIOBuffer(&buffer_, defer))
    return false;
  if (*defer) {
    request()->LogBlockedBy("MojoAsyncResourceHandler");
    did_defer_on_writing_ = true;
  }
  return true;
}

void MojoAsyncResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  int64_t total_received_bytes = request()->GetTotalReceivedBytes();
  int64_t bytes_to_report =
      total_received_bytes - reported_total_received_bytes_;
  reported_total_received_bytes_ = total_received_bytes;
  DCHECK_LE(0, bytes_to_report);

  url_loader_client_->OnDataDownloaded(bytes_downloaded, bytes_to_report);
}

void MojoAsyncResourceHandler::FollowRedirect() {
  if (!request()->status().is_success()) {
    DVLOG(1) << "FollowRedirect for invalid request";
    return;
  }
  if (!did_defer_on_redirect_) {
    DVLOG(1) << "Malformed FollowRedirect request";
    ReportBadMessage("Malformed FollowRedirect request");
    return;
  }

  DCHECK(!did_defer_on_writing_);
  did_defer_on_redirect_ = false;
  request()->LogUnblocked();
  controller()->Resume();
}

void MojoAsyncResourceHandler::OnWritableForTesting() {
  OnWritable(MOJO_RESULT_OK);
}

void MojoAsyncResourceHandler::SetAllocationSizeForTesting(size_t size) {
  g_allocation_size = size;
}

MojoResult MojoAsyncResourceHandler::BeginWrite(void** data,
                                                uint32_t* available) {
  MojoResult result = mojo::BeginWriteDataRaw(
      shared_writer_->writer(), data, available, MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK)
    *available = std::min(*available, static_cast<uint32_t>(kMaxChunkSize));
  return result;
}

MojoResult MojoAsyncResourceHandler::EndWrite(uint32_t written) {
  return mojo::EndWriteDataRaw(shared_writer_->writer(), written);
}

void MojoAsyncResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    bool* defer) {
  shared_writer_ = nullptr;
  buffer_ = nullptr;
  handle_watcher_.Cancel();

  const ResourceRequestInfoImpl* info = GetRequestInfo();

  // TODO(gavinp): Remove this CHECK when we figure out the cause of
  // http://crbug.com/124680 . This check mirrors closely check in
  // WebURLLoaderImpl::OnCompletedRequest that routes this message to a WebCore
  // ResourceHandleInternal which asserts on its state and crashes. By crashing
  // when the message is sent, we should get better crash reports.
  CHECK(status.status() != net::URLRequestStatus::SUCCESS ||
        sent_received_response_message_);

  int error_code = status.error();
  bool was_ignored_by_handler = info->WasIgnoredByHandler();

  DCHECK_NE(status.status(), net::URLRequestStatus::IO_PENDING);
  // If this check fails, then we're in an inconsistent state because all
  // requests ignored by the handler should be canceled (which should result in
  // the ERR_ABORTED error code).
  DCHECK(!was_ignored_by_handler || error_code == net::ERR_ABORTED);

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;
  request_complete_data.was_ignored_by_handler = was_ignored_by_handler;
  request_complete_data.exists_in_cache = request()->response_info().was_cached;
  request_complete_data.completion_time = base::TimeTicks::Now();
  request_complete_data.encoded_data_length =
      request()->GetTotalReceivedBytes();

  url_loader_client_->OnComplete(request_complete_data);
}

bool MojoAsyncResourceHandler::CopyReadDataToDataPipe(bool* defer) {
  while (true) {
    scoped_refptr<net::IOBufferWithSize> dest;
    if (!AllocateWriterIOBuffer(&dest, defer))
      return false;
    if (*defer)
      return true;
    if (buffer_bytes_read_ == 0) {
      // All bytes are copied. Save the buffer for the next OnWillRead call.
      buffer_ = std::move(dest);
      return true;
    }

    size_t copied_size =
        std::min(buffer_bytes_read_, static_cast<size_t>(dest->size()));
    memcpy(dest->data(), buffer_->data() + buffer_offset_, copied_size);
    buffer_offset_ += copied_size;
    buffer_bytes_read_ -= copied_size;
    if (EndWrite(copied_size) != MOJO_RESULT_OK)
      return false;

    if (buffer_bytes_read_ == 0) {
      // All bytes are copied.
      buffer_offset_ = 0;
      is_using_io_buffer_not_from_writer_ = false;
    }
  }
}

bool MojoAsyncResourceHandler::AllocateWriterIOBuffer(
    scoped_refptr<net::IOBufferWithSize>* buf,
    bool* defer) {
  void* data = nullptr;
  uint32_t available = 0;
  MojoResult result = BeginWrite(&data, &available);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    *defer = true;
    return true;
  }
  if (result != MOJO_RESULT_OK)
    return false;
  *buf = new WriterIOBuffer(shared_writer_, data, available);
  return true;
}

bool MojoAsyncResourceHandler::CheckForSufficientResource() {
  if (has_checked_for_sufficient_resources_)
    return true;
  has_checked_for_sufficient_resources_ = true;

  if (rdh_->HasSufficientResourcesForRequest(request()))
    return true;

  controller()->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
  return false;
}

void MojoAsyncResourceHandler::OnWritable(MojoResult result) {
  if (!did_defer_on_writing_)
    return;
  DCHECK(!did_defer_on_redirect_);
  did_defer_on_writing_ = false;

  if (is_using_io_buffer_not_from_writer_) {
    // |buffer_| is set to a net::IOBufferWithSize. Write the buffer contents
    // to the data pipe.
    DCHECK_GT(buffer_bytes_read_, 0u);
    if (!CopyReadDataToDataPipe(&did_defer_on_writing_)) {
      controller()->CancelWithError(net::ERR_FAILED);
      return;
    }
  } else {
    // Allocate a buffer for the next OnWillRead call here.
    if (!AllocateWriterIOBuffer(&buffer_, &did_defer_on_writing_)) {
      controller()->CancelWithError(net::ERR_FAILED);
      return;
    }
  }

  if (did_defer_on_writing_) {
    // Continue waiting.
    return;
  }
  request()->LogUnblocked();
  controller()->Resume();
}

void MojoAsyncResourceHandler::Cancel() {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  ResourceDispatcherHostImpl::Get()->CancelRequestFromRenderer(
      GlobalRequestID(info->GetChildID(), info->GetRequestID()));
}

void MojoAsyncResourceHandler::ReportBadMessage(const std::string& error) {
  mojo::ReportBadMessage(error);
}

}  // namespace content
