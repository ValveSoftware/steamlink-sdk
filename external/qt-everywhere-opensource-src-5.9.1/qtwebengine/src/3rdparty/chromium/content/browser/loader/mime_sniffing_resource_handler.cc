// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mime_sniffing_resource_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/mime_util/mime_util.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/loader/intercepting_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/stream_resource_handler.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/webplugininfo.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/origin.h"

namespace content {

namespace {

const char kAcceptHeader[] = "Accept";
const char kFrameAcceptHeader[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
    "*/*;q=0.8";
const char kStylesheetAcceptHeader[] = "text/css,*/*;q=0.1";
const char kImageAcceptHeader[] = "image/webp,image/*,*/*;q=0.8";
const char kDefaultAcceptHeader[] = "*/*";

// Used to write into an existing IOBuffer at a given offset.
class DependentIOBuffer : public net::WrappedIOBuffer {
 public:
  DependentIOBuffer(net::IOBuffer* buf, int offset)
      : net::WrappedIOBuffer(buf->data() + offset), buf_(buf) {}

 private:
  ~DependentIOBuffer() override {}

  scoped_refptr<net::IOBuffer> buf_;
};

}  // namespace

MimeSniffingResourceHandler::MimeSniffingResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    ResourceDispatcherHostImpl* host,
    PluginService* plugin_service,
    InterceptingResourceHandler* intercepting_handler,
    net::URLRequest* request,
    RequestContextType request_context_type)
    : LayeredResourceHandler(request, std::move(next_handler)),
      state_(STATE_STARTING),
      host_(host),
#if defined(ENABLE_PLUGINS)
      plugin_service_(plugin_service),
#endif
      must_download_(false),
      must_download_is_set_(false),
      read_buffer_size_(0),
      bytes_read_(0),
      intercepting_handler_(intercepting_handler),
      request_context_type_(request_context_type),
      weak_ptr_factory_(this) {
}

MimeSniffingResourceHandler::~MimeSniffingResourceHandler() {}

void MimeSniffingResourceHandler::SetController(
    ResourceController* controller) {
  ResourceHandler::SetController(controller);

  // Downstream handlers see the MimeSniffingResourceHandler as their
  // ResourceController, which allows it to consume part or all of the resource
  // response, and then later replay it to downstream handler.
  DCHECK(next_handler_.get());
  next_handler_->SetController(this);
}

bool MimeSniffingResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  const char* accept_value = nullptr;
  switch (GetRequestInfo()->GetResourceType()) {
    case RESOURCE_TYPE_MAIN_FRAME:
    case RESOURCE_TYPE_SUB_FRAME:
      accept_value = kFrameAcceptHeader;
      break;
    case RESOURCE_TYPE_STYLESHEET:
      accept_value = kStylesheetAcceptHeader;
      break;
    case RESOURCE_TYPE_FAVICON:
    case RESOURCE_TYPE_IMAGE:
      accept_value = kImageAcceptHeader;
      break;
    case RESOURCE_TYPE_SCRIPT:
    case RESOURCE_TYPE_FONT_RESOURCE:
    case RESOURCE_TYPE_SUB_RESOURCE:
    case RESOURCE_TYPE_OBJECT:
    case RESOURCE_TYPE_MEDIA:
    case RESOURCE_TYPE_WORKER:
    case RESOURCE_TYPE_SHARED_WORKER:
    case RESOURCE_TYPE_PREFETCH:
    case RESOURCE_TYPE_XHR:
    case RESOURCE_TYPE_PING:
    case RESOURCE_TYPE_SERVICE_WORKER:
    case RESOURCE_TYPE_CSP_REPORT:
    case RESOURCE_TYPE_PLUGIN_RESOURCE:
      accept_value = kDefaultAcceptHeader;
      break;
    case RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED();
      break;
  }

  // The false parameter prevents overwriting an existing accept header value,
  // which is needed because JS can manually set an accept header on an XHR.
  request()->SetExtraRequestHeaderByName(kAcceptHeader, accept_value, false);
  return next_handler_->OnWillStart(url, defer);
}

bool MimeSniffingResourceHandler::OnResponseStarted(ResourceResponse* response,
                                                    bool* defer) {
  DCHECK_EQ(STATE_STARTING, state_);
  response_ = response;

  state_ = STATE_BUFFERING;
  // A 304 response should not contain a Content-Type header (RFC 7232 section
  // 4.1). The following code may incorrectly attempt to add a Content-Type to
  // the response, and so must be skipped for 304 responses.
  if (!(response_->head.headers.get() &&
        response_->head.headers->response_code() == 304)) {
    if (ShouldSniffContent())
      return true;

    if (response_->head.mime_type.empty()) {
      // Ugg.  The server told us not to sniff the content but didn't give us a
      // mime type.  What's a browser to do?  Turns out, we're supposed to
      // treat the response as "text/plain".  This is the most secure option.
      response_->head.mime_type.assign("text/plain");
    }

    // Treat feed types as text/plain.
    if (response_->head.mime_type == "application/rss+xml" ||
        response_->head.mime_type == "application/atom+xml") {
      response_->head.mime_type.assign("text/plain");
    }
  }

  return ProcessState(defer);
}

bool MimeSniffingResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
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

bool MimeSniffingResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  if (state_ == STATE_STREAMING)
    return next_handler_->OnReadCompleted(bytes_read, defer);

  DCHECK_EQ(state_, STATE_BUFFERING);
  bytes_read_ += bytes_read;

  const std::string& type_hint = response_->head.mime_type;

  std::string new_type;
  bool made_final_decision =
      net::SniffMimeType(read_buffer_->data(), bytes_read_, request()->url(),
                         type_hint, &new_type);

  // SniffMimeType() returns false if there is not enough data to determine
  // the mime type. However, even if it returns false, it returns a new type
  // that is probably better than the current one.
  response_->head.mime_type.assign(new_type);

  if (!made_final_decision && (bytes_read > 0))
    return true;

  return ProcessState(defer);
}

void MimeSniffingResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    bool* defer) {
  // Upon completion, act like a pass-through handler in case the downstream
  // handler defers OnResponseCompleted.
  state_ = STATE_STREAMING;

  next_handler_->OnResponseCompleted(status, defer);
}

void MimeSniffingResourceHandler::Resume() {
  // If no information is currently being transmitted to downstream handlers,
  // they should not attempt to resume the request.
  if (state_ == STATE_BUFFERING) {
    NOTREACHED();
    return;
  }

  // If the BufferingHandler is acting as a pass-through handler, just ask the
  // upwards ResourceController to resume the request.
  if (state_ == STATE_STARTING || state_ == STATE_STREAMING) {
    controller()->Resume();
    return;
  }

  // Otherwise proceed with the replay of the response. If it is successful,
  // it will resume the request.
  AdvanceState();
}

void MimeSniffingResourceHandler::Cancel() {
  controller()->Cancel();
}

void MimeSniffingResourceHandler::CancelAndIgnore() {
  controller()->CancelAndIgnore();
}

void MimeSniffingResourceHandler::CancelWithError(int error_code) {
  controller()->CancelWithError(error_code);
}

void MimeSniffingResourceHandler::AdvanceState() {
  bool defer = false;
  if (!ProcessState(&defer)) {
    Cancel();
  } else if (!defer) {
    DCHECK_EQ(STATE_STREAMING, state_);
    controller()->Resume();
  }
}

bool MimeSniffingResourceHandler::ProcessState(bool* defer) {
  bool return_value = true;
  while (!*defer && return_value && state_ != STATE_STREAMING) {
    switch (state_) {
      case STATE_BUFFERING:
        return_value = MaybeIntercept(defer);
        break;
      case STATE_INTERCEPTION_CHECK_DONE:
        return_value = ReplayResponseReceived(defer);
        break;
      case STATE_REPLAYING_RESPONSE_RECEIVED:
        return_value = ReplayReadCompleted(defer);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return return_value;
}

bool MimeSniffingResourceHandler::MaybeIntercept(bool* defer) {
  DCHECK_EQ(STATE_BUFFERING, state_);
  // If a request that can be intercepted failed the check for interception
  // step, it should be canceled.
  if (!MaybeStartInterception(defer))
    return false;

  if (!*defer)
    state_ = STATE_INTERCEPTION_CHECK_DONE;

  return true;
}

bool MimeSniffingResourceHandler::ReplayResponseReceived(bool* defer) {
  DCHECK_EQ(STATE_INTERCEPTION_CHECK_DONE, state_);
  state_ = STATE_REPLAYING_RESPONSE_RECEIVED;
  return next_handler_->OnResponseStarted(response_.get(), defer);
}

bool MimeSniffingResourceHandler::ReplayReadCompleted(bool* defer) {
  DCHECK_EQ(STATE_REPLAYING_RESPONSE_RECEIVED, state_);

  state_ = STATE_STREAMING;

  if (!read_buffer_.get())
    return true;

  bool result = next_handler_->OnReadCompleted(bytes_read_, defer);

  read_buffer_ = nullptr;
  read_buffer_size_ = 0;
  bytes_read_ = 0;

  return result;
}

bool MimeSniffingResourceHandler::ShouldSniffContent() {
  if (request_context_type_ == REQUEST_CONTEXT_TYPE_FETCH) {
    // MIME sniffing should be disabled for a request initiated by fetch().
    return false;
  }

  const std::string& mime_type = response_->head.mime_type;

  std::string content_type_options;
  request()->GetResponseHeaderByName("x-content-type-options",
                                     &content_type_options);

  bool sniffing_blocked =
      base::LowerCaseEqualsASCII(content_type_options, "nosniff");
  bool we_would_like_to_sniff =
      net::ShouldSniffMimeType(request()->url(), mime_type);

  if (!sniffing_blocked && we_would_like_to_sniff) {
    // We're going to look at the data before deciding what the content type
    // is.  That means we need to delay sending the ResponseStarted message
    // over the IPC channel.
    VLOG(1) << "To buffer: " << request()->url().spec();
    return true;
  }

  return false;
}

bool MimeSniffingResourceHandler::MaybeStartInterception(bool* defer) {
  if (!CanBeIntercepted())
    return true;

  DCHECK(!response_->head.mime_type.empty());

  ResourceRequestInfoImpl* info = GetRequestInfo();
  const std::string& mime_type = response_->head.mime_type;

  // https://crbug.com/568184 - Temporary hack to track servers that aren't
  // setting Content-Disposition when sending x-x509-user-cert and expecting
  // the browser to automatically install certificates; this is being
  // deprecated and will be removed upon full <keygen> removal.
  if (mime_type == "application/x-x509-user-cert" && response_->head.headers) {
    UMA_HISTOGRAM_BOOLEAN(
        "UserCert.ContentDisposition",
        response_->head.headers->HasHeader("Content-Disposition"));
  }

  // Allow requests for object/embed tags to be intercepted as streams.
  if (info->GetResourceType() == content::RESOURCE_TYPE_OBJECT) {
    DCHECK(!info->allow_download());

    bool handled_by_plugin;
    if (!CheckForPluginHandler(defer, &handled_by_plugin))
      return false;
    if (handled_by_plugin || *defer)
      return true;
  }

  if (!info->allow_download())
    return true;

  // info->allow_download() == true implies
  // info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME or
  // info->GetResourceType() == RESOURCE_TYPE_SUB_FRAME.
  DCHECK(info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME ||
         info->GetResourceType() == RESOURCE_TYPE_SUB_FRAME);

  bool must_download = MustDownload();
  if (!must_download) {
    if (mime_util::IsSupportedMimeType(mime_type))
      return true;

    bool handled_by_plugin;
    if (!CheckForPluginHandler(defer, &handled_by_plugin))
      return false;
    if (handled_by_plugin || *defer)
      return true;
  }

  // This request is a download.

  if (!CheckResponseIsNotProvisional())
    return false;

  info->set_is_download(true);
  std::unique_ptr<ResourceHandler> handler(
      host_->CreateResourceHandlerForDownload(request(),
                                              true,  // is_content_initiated
                                              must_download,
                                              false /* is_new_request */));
  intercepting_handler_->UseNewHandler(std::move(handler), std::string());
  return true;
}

bool MimeSniffingResourceHandler::CheckForPluginHandler(
    bool* defer,
    bool* handled_by_plugin) {
  *handled_by_plugin = false;
#if defined(ENABLE_PLUGINS)
  ResourceRequestInfoImpl* info = GetRequestInfo();
  bool allow_wildcard = false;
  bool stale;
  WebPluginInfo plugin;
  bool has_plugin = plugin_service_->GetPluginInfo(
      info->GetChildID(), info->GetRenderFrameID(), info->GetContext(),
      request()->url(), url::Origin(), response_->head.mime_type,
      allow_wildcard, &stale, &plugin, NULL);

  if (stale) {
    // Refresh the plugins asynchronously.
    plugin_service_->GetPlugins(
        base::Bind(&MimeSniffingResourceHandler::OnPluginsLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
    request()->LogBlockedBy("MimeSniffingResourceHandler");
    *defer = true;
    return true;
  }

  if (has_plugin && plugin.type != WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN) {
    *handled_by_plugin = true;
    return true;
  }

  // Attempt to intercept the request as a stream.
  base::FilePath plugin_path;
  if (has_plugin)
    plugin_path = plugin.path;
  std::string payload;
  std::unique_ptr<ResourceHandler> handler(host_->MaybeInterceptAsStream(
      plugin_path, request(), response_.get(), &payload));
  if (handler) {
    if (!CheckResponseIsNotProvisional())
      return false;
    *handled_by_plugin = true;
    intercepting_handler_->UseNewHandler(std::move(handler), payload);
  }
#endif
  return true;
}

bool MimeSniffingResourceHandler::CanBeIntercepted() {
  if (response_->head.headers.get() &&
      response_->head.headers->response_code() == 304) {
    return false;
  }

  return true;
}

bool MimeSniffingResourceHandler::CheckResponseIsNotProvisional() {
  if (!response_->head.headers.get() ||
      response_->head.headers->response_code() / 100 == 2) {
    return true;
  }

  // The response code indicates that this is an error page, but we don't
  // know how to display the content.  We follow Firefox here and show our
  // own error page instead of intercepting the request as a stream or a
  // download.
  // TODO(abarth): We should abstract the response_code test, but this kind
  // of check is scattered throughout our codebase.
  request()->CancelWithError(net::ERR_INVALID_RESPONSE);
  return false;
}

bool MimeSniffingResourceHandler::MustDownload() {
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

void MimeSniffingResourceHandler::OnPluginsLoaded(
    const std::vector<WebPluginInfo>& plugins) {
  // No longer blocking on the plugins being loaded.
  request()->LogUnblocked();
  if (state_ == STATE_BUFFERING)
    AdvanceState();
}

}  // namespace content
