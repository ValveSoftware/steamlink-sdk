// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/url_loader_impl.h"

#include "mojo/common/common_type_converters.h"
#include "mojo/services/network/network_context.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"

namespace mojo {
namespace {

const uint32_t kMaxReadSize = 64 * 1024;

// Generates an URLResponsePtr from the response state of a net::URLRequest.
URLResponsePtr MakeURLResponse(const net::URLRequest* url_request) {
  URLResponsePtr response(URLResponse::New());
  response->url = url_request->url().spec();

  const net::HttpResponseHeaders* headers = url_request->response_headers();
  if (headers) {
    response->status_code = headers->response_code();
    response->status_line = headers->GetStatusLine();

    std::vector<String> header_lines;
    void* iter = NULL;
    std::string name, value;
    while (headers->EnumerateHeaderLines(&iter, &name, &value))
      header_lines.push_back(name + ": " + value);
    if (!header_lines.empty())
      response->headers.Swap(&header_lines);
  }

  return response.Pass();
}

}  // namespace

// Keeps track of a pending two-phase write on a DataPipeProducerHandle.
class URLLoaderImpl::PendingWriteToDataPipe :
    public base::RefCountedThreadSafe<PendingWriteToDataPipe> {
 public:
  explicit PendingWriteToDataPipe(ScopedDataPipeProducerHandle handle)
      : handle_(handle.Pass()),
        buffer_(NULL) {
  }

  bool BeginWrite(uint32_t* num_bytes) {
    MojoResult result = BeginWriteDataRaw(handle_.get(), &buffer_, num_bytes,
                                          MOJO_WRITE_DATA_FLAG_NONE);
    if (*num_bytes > kMaxReadSize)
      *num_bytes = kMaxReadSize;

    return result == MOJO_RESULT_OK;
  }

  ScopedDataPipeProducerHandle Complete(uint32_t num_bytes) {
    EndWriteDataRaw(handle_.get(), num_bytes);
    buffer_ = NULL;
    return handle_.Pass();
  }

  char* buffer() { return static_cast<char*>(buffer_); }

 private:
  friend class base::RefCountedThreadSafe<PendingWriteToDataPipe>;

  ~PendingWriteToDataPipe() {
    if (handle_.is_valid())
      EndWriteDataRaw(handle_.get(), 0);
  }

  ScopedDataPipeProducerHandle handle_;
  void* buffer_;

  DISALLOW_COPY_AND_ASSIGN(PendingWriteToDataPipe);
};

// Takes ownership of a pending two-phase write on a DataPipeProducerHandle,
// and makes its buffer available as a net::IOBuffer.
class URLLoaderImpl::DependentIOBuffer : public net::WrappedIOBuffer {
 public:
  DependentIOBuffer(PendingWriteToDataPipe* pending_write)
      : net::WrappedIOBuffer(pending_write->buffer()),
        pending_write_(pending_write) {
  }
 private:
  virtual ~DependentIOBuffer() {}
  scoped_refptr<PendingWriteToDataPipe> pending_write_;
};

URLLoaderImpl::URLLoaderImpl(NetworkContext* context)
    : context_(context),
      auto_follow_redirects_(true),
      weak_ptr_factory_(this) {
}

URLLoaderImpl::~URLLoaderImpl() {
}

void URLLoaderImpl::OnConnectionError() {
  delete this;
}

void URLLoaderImpl::Start(URLRequestPtr request,
                          ScopedDataPipeProducerHandle response_body_stream) {
  // Do not allow starting another request.
  if (url_request_) {
    SendError(net::ERR_UNEXPECTED);
    url_request_.reset();
    response_body_stream_.reset();
    return;
  }

  if (!request) {
    SendError(net::ERR_INVALID_ARGUMENT);
    return;
  }

  response_body_stream_ = response_body_stream.Pass();

  GURL url(request->url);
  url_request_.reset(
      new net::URLRequest(url,
                          net::DEFAULT_PRIORITY,
                          this,
                          context_->url_request_context()));
  url_request_->set_method(request->method);
  if (request->headers) {
    net::HttpRequestHeaders headers;
    for (size_t i = 0; i < request->headers.size(); ++i)
      headers.AddHeaderFromString(request->headers[i].To<base::StringPiece>());
    url_request_->SetExtraRequestHeaders(headers);
  }
  if (request->bypass_cache)
    url_request_->SetLoadFlags(net::LOAD_BYPASS_CACHE);
  // TODO(darin): Handle request body.

  auto_follow_redirects_ = request->auto_follow_redirects;

  url_request_->Start();
}

void URLLoaderImpl::FollowRedirect() {
  if (auto_follow_redirects_) {
    DLOG(ERROR) << "Spurious call to FollowRedirect";
  } else {
    if (url_request_)
      url_request_->FollowDeferredRedirect();
  }
}

void URLLoaderImpl::OnReceivedRedirect(net::URLRequest* url_request,
                                       const GURL& new_url,
                                       bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());
  DCHECK(url_request->status().is_success());

  URLResponsePtr response = MakeURLResponse(url_request);
  std::string redirect_method =
      net::URLRequest::ComputeMethodForRedirect(url_request->method(),
                                                response->status_code);
  client()->OnReceivedRedirect(
      response.Pass(), new_url.spec(), redirect_method);

  *defer_redirect = !auto_follow_redirects_;
}

void URLLoaderImpl::OnResponseStarted(net::URLRequest* url_request) {
  DCHECK(url_request == url_request_.get());

  if (!url_request->status().is_success()) {
    SendError(url_request->status().error());
    return;
  }

  client()->OnReceivedResponse(MakeURLResponse(url_request));

  // Start reading...
  ReadMore();
}

void URLLoaderImpl::OnReadCompleted(net::URLRequest* url_request,
                                    int bytes_read) {
  if (url_request->status().is_success()) {
    DidRead(static_cast<uint32_t>(bytes_read), false);
  } else {
    pending_write_ = NULL;  // This closes the data pipe.
    // TODO(darin): Perhaps we should communicate this error to our client.
  }
}

void URLLoaderImpl::SendError(int error_code) {
  NetworkErrorPtr error(NetworkError::New());
  error->code = error_code;
  error->description = net::ErrorToString(error_code);
  client()->OnReceivedError(error.Pass());
}

void URLLoaderImpl::ReadMore() {
  DCHECK(!pending_write_);

  pending_write_ = new PendingWriteToDataPipe(response_body_stream_.Pass());

  uint32_t num_bytes;
  if (!pending_write_->BeginWrite(&num_bytes))
    CHECK(false);  // Oops! TODO(darin): crbug/386877: The pipe might be full!
  if (num_bytes > static_cast<uint32_t>(std::numeric_limits<int>::max()))
    CHECK(false);  // Oops!

  scoped_refptr<net::IOBuffer> buf = new DependentIOBuffer(pending_write_);

  int bytes_read;
  url_request_->Read(buf, static_cast<int>(num_bytes), &bytes_read);

  // Drop our reference to the buffer.
  buf = NULL;

  if (url_request_->status().is_io_pending()) {
    // Wait for OnReadCompleted.
  } else if (url_request_->status().is_success() && bytes_read > 0) {
    DidRead(static_cast<uint32_t>(bytes_read), true);
  } else {
    pending_write_->Complete(0);
    pending_write_ = NULL;  // This closes the data pipe.
    if (bytes_read == 0) {
      client()->OnReceivedEndOfResponseBody();
    } else {
      DCHECK(!url_request_->status().is_success());
      SendError(url_request_->status().error());
    }
  }
}

void URLLoaderImpl::DidRead(uint32_t num_bytes, bool completed_synchronously) {
  DCHECK(url_request_->status().is_success());

  response_body_stream_ = pending_write_->Complete(num_bytes);
  pending_write_ = NULL;

  if (completed_synchronously) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&URLLoaderImpl::ReadMore, weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

}  // namespace mojo
