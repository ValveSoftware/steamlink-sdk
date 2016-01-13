// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/buffered_resource_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/loader/certificate_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/stream_resource_handler.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/webplugininfo.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_response_headers.h"

namespace content {

namespace {

void RecordSnifferMetrics(bool sniffing_blocked,
                          bool we_would_like_to_sniff,
                          const std::string& mime_type) {
  static base::HistogramBase* nosniff_usage(NULL);
  if (!nosniff_usage)
    nosniff_usage = base::BooleanHistogram::FactoryGet(
        "nosniff.usage", base::HistogramBase::kUmaTargetedHistogramFlag);
  nosniff_usage->AddBoolean(sniffing_blocked);

  if (sniffing_blocked) {
    static base::HistogramBase* nosniff_otherwise(NULL);
    if (!nosniff_otherwise)
      nosniff_otherwise = base::BooleanHistogram::FactoryGet(
          "nosniff.otherwise", base::HistogramBase::kUmaTargetedHistogramFlag);
    nosniff_otherwise->AddBoolean(we_would_like_to_sniff);

    static base::HistogramBase* nosniff_empty_mime_type(NULL);
    if (!nosniff_empty_mime_type)
      nosniff_empty_mime_type = base::BooleanHistogram::FactoryGet(
          "nosniff.empty_mime_type",
          base::HistogramBase::kUmaTargetedHistogramFlag);
    nosniff_empty_mime_type->AddBoolean(mime_type.empty());
  }
}

// Used to write into an existing IOBuffer at a given offset.
class DependentIOBuffer : public net::WrappedIOBuffer {
 public:
  DependentIOBuffer(net::IOBuffer* buf, int offset)
      : net::WrappedIOBuffer(buf->data() + offset),
        buf_(buf) {
  }

 private:
  virtual ~DependentIOBuffer() {}

  scoped_refptr<net::IOBuffer> buf_;
};

}  // namespace

BufferedResourceHandler::BufferedResourceHandler(
    scoped_ptr<ResourceHandler> next_handler,
    ResourceDispatcherHostImpl* host,
    net::URLRequest* request)
    : LayeredResourceHandler(request, next_handler.Pass()),
      state_(STATE_STARTING),
      host_(host),
      read_buffer_size_(0),
      bytes_read_(0),
      must_download_(false),
      must_download_is_set_(false),
      weak_ptr_factory_(this) {
}

BufferedResourceHandler::~BufferedResourceHandler() {
}

void BufferedResourceHandler::SetController(ResourceController* controller) {
  ResourceHandler::SetController(controller);

  // Downstream handlers see us as their ResourceController, which allows us to
  // consume part or all of the resource response, and then later replay it to
  // downstream handler.
  DCHECK(next_handler_.get());
  next_handler_->SetController(this);
}

bool BufferedResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                bool* defer) {
  response_ = response;

  // TODO(darin): It is very odd to special-case 304 responses at this level.
  // We do so only because the code always has, see r24977 and r29355.  The
  // fact that 204 is no longer special-cased this way suggests that 304 need
  // not be special-cased either.
  //
  // The network stack only forwards 304 responses that were not received in
  // response to a conditional request (i.e., If-Modified-Since).  Other 304
  // responses end up being translated to 200 or whatever the cached response
  // code happens to be.  It should be very rare to see a 304 at this level.

  if (!(response_->head.headers.get() &&
        response_->head.headers->response_code() == 304)) {
    if (ShouldSniffContent()) {
      state_ = STATE_BUFFERING;
      return true;
    }

    if (response_->head.mime_type.empty()) {
      // Ugg.  The server told us not to sniff the content but didn't give us
      // a mime type.  What's a browser to do?  Turns out, we're supposed to
      // treat the response as "text/plain".  This is the most secure option.
      response_->head.mime_type.assign("text/plain");
    }

    // Treat feed types as text/plain.
    if (response_->head.mime_type == "application/rss+xml" ||
        response_->head.mime_type == "application/atom+xml") {
      response_->head.mime_type.assign("text/plain");
    }
  }

  state_ = STATE_PROCESSING;
  return ProcessResponse(defer);
}

// We'll let the original event handler provide a buffer, and reuse it for
// subsequent reads until we're done buffering.
bool BufferedResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                         int* buf_size,
                                         int min_size) {
  if (state_ == STATE_STREAMING)
    return next_handler_->OnWillRead(buf, buf_size, min_size);

  DCHECK_EQ(-1, min_size);

  if (read_buffer_.get()) {
    CHECK_LT(bytes_read_, read_buffer_size_);
    *buf = new DependentIOBuffer(read_buffer_.get(), bytes_read_);
    *buf_size = read_buffer_size_ - bytes_read_;
  } else {
    if (!next_handler_->OnWillRead(buf, buf_size, min_size))
      return false;

    read_buffer_ = *buf;
    read_buffer_size_ = *buf_size;
    DCHECK_GE(read_buffer_size_, net::kMaxBytesToSniff * 2);
  }
  return true;
}

bool BufferedResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  if (state_ == STATE_STREAMING)
    return next_handler_->OnReadCompleted(bytes_read, defer);

  DCHECK_EQ(state_, STATE_BUFFERING);
  bytes_read_ += bytes_read;

  if (!DetermineMimeType() && (bytes_read > 0))
    return true;  // Needs more data, so keep buffering.

  state_ = STATE_PROCESSING;
  return ProcessResponse(defer);
}

void BufferedResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  // Upon completion, act like a pass-through handler in case the downstream
  // handler defers OnResponseCompleted.
  state_ = STATE_STREAMING;

  next_handler_->OnResponseCompleted(status, security_info, defer);
}

void BufferedResourceHandler::Resume() {
  switch (state_) {
    case STATE_BUFFERING:
    case STATE_PROCESSING:
      NOTREACHED();
      break;
    case STATE_REPLAYING:
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&BufferedResourceHandler::CallReplayReadCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    case STATE_STARTING:
    case STATE_STREAMING:
      controller()->Resume();
      break;
  }
}

void BufferedResourceHandler::Cancel() {
  controller()->Cancel();
}

void BufferedResourceHandler::CancelAndIgnore() {
  controller()->CancelAndIgnore();
}

void BufferedResourceHandler::CancelWithError(int error_code) {
  controller()->CancelWithError(error_code);
}

bool BufferedResourceHandler::ProcessResponse(bool* defer) {
  DCHECK_EQ(STATE_PROCESSING, state_);

  // TODO(darin): Stop special-casing 304 responses.
  if (!(response_->head.headers.get() &&
        response_->head.headers->response_code() == 304)) {
    if (!SelectNextHandler(defer))
      return false;
    if (*defer)
      return true;
  }

  state_ = STATE_REPLAYING;

  if (!next_handler_->OnResponseStarted(response_.get(), defer))
    return false;

  if (!read_buffer_.get()) {
    state_ = STATE_STREAMING;
    return true;
  }

  if (!*defer)
    return ReplayReadCompleted(defer);

  return true;
}

bool BufferedResourceHandler::ShouldSniffContent() {
  const std::string& mime_type = response_->head.mime_type;

  std::string content_type_options;
  request()->GetResponseHeaderByName("x-content-type-options",
                                     &content_type_options);

  bool sniffing_blocked =
      LowerCaseEqualsASCII(content_type_options, "nosniff");
  bool we_would_like_to_sniff =
      net::ShouldSniffMimeType(request()->url(), mime_type);

  RecordSnifferMetrics(sniffing_blocked, we_would_like_to_sniff, mime_type);

  if (!sniffing_blocked && we_would_like_to_sniff) {
    // We're going to look at the data before deciding what the content type
    // is.  That means we need to delay sending the ResponseStarted message
    // over the IPC channel.
    VLOG(1) << "To buffer: " << request()->url().spec();
    return true;
  }

  return false;
}

bool BufferedResourceHandler::DetermineMimeType() {
  DCHECK_EQ(STATE_BUFFERING, state_);

  const std::string& type_hint = response_->head.mime_type;

  std::string new_type;
  bool made_final_decision =
      net::SniffMimeType(read_buffer_->data(), bytes_read_, request()->url(),
                         type_hint, &new_type);

  // SniffMimeType() returns false if there is not enough data to determine
  // the mime type. However, even if it returns false, it returns a new type
  // that is probably better than the current one.
  response_->head.mime_type.assign(new_type);

  return made_final_decision;
}

bool BufferedResourceHandler::SelectNextHandler(bool* defer) {
  DCHECK(!response_->head.mime_type.empty());

  ResourceRequestInfoImpl* info = GetRequestInfo();
  const std::string& mime_type = response_->head.mime_type;

  if (net::IsSupportedCertificateMimeType(mime_type)) {
    // Install certificate file.
    info->set_is_download(true);
    scoped_ptr<ResourceHandler> handler(
        new CertificateResourceHandler(request()));
    return UseAlternateNextHandler(handler.Pass(), std::string());
  }

  if (!info->allow_download())
    return true;

  bool must_download = MustDownload();
  if (!must_download) {
    if (net::IsSupportedMimeType(mime_type))
      return true;

    std::string payload;
    scoped_ptr<ResourceHandler> handler(
        host_->MaybeInterceptAsStream(request(), response_.get(), &payload));
    if (handler) {
      return UseAlternateNextHandler(handler.Pass(), payload);
    }

#if defined(ENABLE_PLUGINS)
    bool stale;
    bool has_plugin = HasSupportingPlugin(&stale);
    if (stale) {
      // Refresh the plugins asynchronously.
      PluginServiceImpl::GetInstance()->GetPlugins(
          base::Bind(&BufferedResourceHandler::OnPluginsLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
      request()->LogBlockedBy("BufferedResourceHandler");
      *defer = true;
      return true;
    }
    if (has_plugin)
      return true;
#endif
  }

  // Install download handler
  info->set_is_download(true);
  scoped_ptr<ResourceHandler> handler(
      host_->CreateResourceHandlerForDownload(
          request(),
          true,  // is_content_initiated
          must_download,
          content::DownloadItem::kInvalidId,
          scoped_ptr<DownloadSaveInfo>(new DownloadSaveInfo()),
          DownloadUrlParameters::OnStartedCallback()));
  return UseAlternateNextHandler(handler.Pass(), std::string());
}

bool BufferedResourceHandler::UseAlternateNextHandler(
    scoped_ptr<ResourceHandler> new_handler,
    const std::string& payload_for_old_handler) {
  if (response_->head.headers.get() &&  // Can be NULL if FTP.
      response_->head.headers->response_code() / 100 != 2) {
    // The response code indicates that this is an error page, but we don't
    // know how to display the content.  We follow Firefox here and show our
    // own error page instead of triggering a download.
    // TODO(abarth): We should abstract the response_code test, but this kind
    //               of check is scattered throughout our codebase.
    request()->CancelWithError(net::ERR_INVALID_RESPONSE);
    return false;
  }

  // Inform the original ResourceHandler that this will be handled entirely by
  // the new ResourceHandler.
  // TODO(darin): We should probably check the return values of these.
  bool defer_ignored = false;
  next_handler_->OnResponseStarted(response_.get(), &defer_ignored);
  // Although deferring OnResponseStarted is legal, the only downstream handler
  // which does so is CrossSiteResourceHandler. Cross-site transitions should
  // not trigger when switching handlers.
  DCHECK(!defer_ignored);
  if (payload_for_old_handler.empty()) {
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    next_handler_->OnResponseCompleted(status, std::string(), &defer_ignored);
    DCHECK(!defer_ignored);
  } else {
    scoped_refptr<net::IOBuffer> buf;
    int size = 0;

    next_handler_->OnWillRead(&buf, &size, -1);
    CHECK_GE(size, static_cast<int>(payload_for_old_handler.length()));

    memcpy(buf->data(), payload_for_old_handler.c_str(),
           payload_for_old_handler.length());

    next_handler_->OnReadCompleted(payload_for_old_handler.length(),
                                   &defer_ignored);
    DCHECK(!defer_ignored);

    net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
    next_handler_->OnResponseCompleted(status, std::string(), &defer_ignored);
    DCHECK(!defer_ignored);
  }

  // This is handled entirely within the new ResourceHandler, so just reset the
  // original ResourceHandler.
  next_handler_ = new_handler.Pass();
  next_handler_->SetController(this);

  return CopyReadBufferToNextHandler();
}

bool BufferedResourceHandler::ReplayReadCompleted(bool* defer) {
  DCHECK(read_buffer_.get());

  bool result = next_handler_->OnReadCompleted(bytes_read_, defer);

  read_buffer_ = NULL;
  read_buffer_size_ = 0;
  bytes_read_ = 0;

  state_ = STATE_STREAMING;

  return result;
}

void BufferedResourceHandler::CallReplayReadCompleted() {
  bool defer = false;
  if (!ReplayReadCompleted(&defer)) {
    controller()->Cancel();
  } else if (!defer) {
    state_ = STATE_STREAMING;
    controller()->Resume();
  }
}

bool BufferedResourceHandler::MustDownload() {
  if (must_download_is_set_)
    return must_download_;

  must_download_is_set_ = true;

  std::string disposition;
  request()->GetResponseHeaderByName("content-disposition", &disposition);
  if (!disposition.empty() &&
      net::HttpContentDisposition(disposition, std::string()).is_attachment()) {
    must_download_ = true;
  } else if (host_->delegate() &&
             host_->delegate()->ShouldForceDownloadResource(
                 request()->url(), response_->head.mime_type)) {
    must_download_ = true;
  } else {
    must_download_ = false;
  }

  return must_download_;
}

bool BufferedResourceHandler::HasSupportingPlugin(bool* stale) {
#if defined(ENABLE_PLUGINS)
  ResourceRequestInfoImpl* info = GetRequestInfo();

  bool allow_wildcard = false;
  WebPluginInfo plugin;
  return PluginServiceImpl::GetInstance()->GetPluginInfo(
      info->GetChildID(), info->GetRenderFrameID(), info->GetContext(),
      request()->url(), GURL(), response_->head.mime_type, allow_wildcard,
      stale, &plugin, NULL);
#else
  if (stale)
    *stale = false;
  return false;
#endif
}

bool BufferedResourceHandler::CopyReadBufferToNextHandler() {
  if (!read_buffer_.get())
    return true;

  scoped_refptr<net::IOBuffer> buf;
  int buf_len = 0;
  if (!next_handler_->OnWillRead(&buf, &buf_len, bytes_read_))
    return false;

  CHECK((buf_len >= bytes_read_) && (bytes_read_ >= 0));
  memcpy(buf->data(), read_buffer_->data(), bytes_read_);
  return true;
}

void BufferedResourceHandler::OnPluginsLoaded(
    const std::vector<WebPluginInfo>& plugins) {
  request()->LogUnblocked();
  bool defer = false;
  if (!ProcessResponse(&defer)) {
    controller()->Cancel();
  } else if (!defer) {
    controller()->Resume();
  }
}

}  // namespace content
