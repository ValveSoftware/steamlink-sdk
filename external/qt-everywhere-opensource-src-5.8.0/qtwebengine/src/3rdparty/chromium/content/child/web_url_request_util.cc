// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_request_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/WebCachePolicy.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

using blink::WebCachePolicy;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

const char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";

class HeaderFlattener : public blink::WebHTTPHeaderVisitor {
 public:
  HeaderFlattener() {}

  void visitHeader(const WebString& name, const WebString& value) override {
    // Headers are latin1.
    const std::string& name_latin1 = name.latin1();
    const std::string& value_latin1 = value.latin1();

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.
    if (base::LowerCaseEqualsASCII(name_latin1, "referer"))
      return;

    if (!buffer_.empty())
      buffer_.append("\r\n");
    buffer_.append(name_latin1 + ": " + value_latin1);
  }

  const std::string& GetBuffer() {
    return buffer_;
  }

 private:
  std::string buffer_;
};

}  // namespace

ResourceType WebURLRequestToResourceType(const WebURLRequest& request) {
  WebURLRequest::RequestContext requestContext = request.getRequestContext();
  if (request.getFrameType() != WebURLRequest::FrameTypeNone) {
    DCHECK(requestContext == WebURLRequest::RequestContextForm ||
           requestContext == WebURLRequest::RequestContextFrame ||
           requestContext == WebURLRequest::RequestContextHyperlink ||
           requestContext == WebURLRequest::RequestContextIframe ||
           requestContext == WebURLRequest::RequestContextInternal ||
           requestContext == WebURLRequest::RequestContextLocation);
    if (request.getFrameType() == WebURLRequest::FrameTypeTopLevel ||
        request.getFrameType() == WebURLRequest::FrameTypeAuxiliary) {
      return RESOURCE_TYPE_MAIN_FRAME;
    }
    if (request.getFrameType() == WebURLRequest::FrameTypeNested)
      return RESOURCE_TYPE_SUB_FRAME;
    NOTREACHED();
    return RESOURCE_TYPE_SUB_RESOURCE;
  }

  switch (requestContext) {
    // CSP report
    case WebURLRequest::RequestContextCSPReport:
      return RESOURCE_TYPE_CSP_REPORT;

    // Favicon
    case WebURLRequest::RequestContextFavicon:
      return RESOURCE_TYPE_FAVICON;

    // Font
    case WebURLRequest::RequestContextFont:
      return RESOURCE_TYPE_FONT_RESOURCE;

    // Image
    case WebURLRequest::RequestContextImage:
    case WebURLRequest::RequestContextImageSet:
      return RESOURCE_TYPE_IMAGE;

    // Media
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextVideo:
      return RESOURCE_TYPE_MEDIA;

    // Object
    case WebURLRequest::RequestContextEmbed:
    case WebURLRequest::RequestContextObject:
      return RESOURCE_TYPE_OBJECT;

    // Ping
    case WebURLRequest::RequestContextBeacon:
    case WebURLRequest::RequestContextPing:
      return RESOURCE_TYPE_PING;

    // Subresource of plugins
    case WebURLRequest::RequestContextPlugin:
      return RESOURCE_TYPE_PLUGIN_RESOURCE;

    // Prefetch
    case WebURLRequest::RequestContextPrefetch:
      return RESOURCE_TYPE_PREFETCH;

    // Script
    case WebURLRequest::RequestContextImport:
    case WebURLRequest::RequestContextScript:
      return RESOURCE_TYPE_SCRIPT;

    // Style
    case WebURLRequest::RequestContextXSLT:
    case WebURLRequest::RequestContextStyle:
      return RESOURCE_TYPE_STYLESHEET;

    // Subresource
    case WebURLRequest::RequestContextDownload:
    case WebURLRequest::RequestContextManifest:
    case WebURLRequest::RequestContextSubresource:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // TextTrack
    case WebURLRequest::RequestContextTrack:
      return RESOURCE_TYPE_MEDIA;

    // Workers
    case WebURLRequest::RequestContextServiceWorker:
      return RESOURCE_TYPE_SERVICE_WORKER;
    case WebURLRequest::RequestContextSharedWorker:
      return RESOURCE_TYPE_SHARED_WORKER;
    case WebURLRequest::RequestContextWorker:
      return RESOURCE_TYPE_WORKER;

    // Unspecified
    case WebURLRequest::RequestContextInternal:
    case WebURLRequest::RequestContextUnspecified:
      return RESOURCE_TYPE_SUB_RESOURCE;

    // XHR
    case WebURLRequest::RequestContextEventSource:
    case WebURLRequest::RequestContextFetch:
    case WebURLRequest::RequestContextXMLHttpRequest:
      return RESOURCE_TYPE_XHR;

    // These should be handled by the FrameType checks at the top of the
    // function.
    case WebURLRequest::RequestContextForm:
    case WebURLRequest::RequestContextHyperlink:
    case WebURLRequest::RequestContextLocation:
    case WebURLRequest::RequestContextFrame:
    case WebURLRequest::RequestContextIframe:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;

    default:
      NOTREACHED();
      return RESOURCE_TYPE_SUB_RESOURCE;
  }
}

std::string GetWebURLRequestHeaders(const blink::WebURLRequest& request) {
  HeaderFlattener flattener;
  request.visitHTTPHeaderFields(&flattener);
  return flattener.GetBuffer();
}

int GetLoadFlagsForWebURLRequest(const blink::WebURLRequest& request) {
  int load_flags = net::LOAD_NORMAL;
  GURL url = request.url();
  switch (request.getCachePolicy()) {
    case WebCachePolicy::ValidatingCacheData:
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case WebCachePolicy::BypassingCache:
      load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case WebCachePolicy::ReturnCacheDataElseLoad:
      load_flags |= net::LOAD_PREFERRING_CACHE;
      break;
    case WebCachePolicy::ReturnCacheDataDontLoad:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case WebCachePolicy::UseProtocolCachePolicy:
      break;
    default:
      NOTREACHED();
  }

  if (!request.allowStoredCredentials()) {
    load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
    load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  }

  if (!request.allowStoredCredentials())
    load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;

  return load_flags;
}

WebHTTPBody GetWebHTTPBodyForRequestBody(
    const scoped_refptr<ResourceRequestBodyImpl>& input) {
  WebHTTPBody http_body;
  http_body.initialize();
  http_body.setIdentifier(input->identifier());
  for (const auto& element : *input->elements()) {
    switch (element.type()) {
      case ResourceRequestBodyImpl::Element::TYPE_BYTES:
        http_body.appendData(WebData(element.bytes(), element.length()));
        break;
      case ResourceRequestBodyImpl::Element::TYPE_FILE:
        http_body.appendFileRange(
            element.path().AsUTF16Unsafe(), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case ResourceRequestBodyImpl::Element::TYPE_FILE_FILESYSTEM:
        http_body.appendFileSystemURLRange(
            element.filesystem_url(), element.offset(),
            (element.length() != std::numeric_limits<uint64_t>::max())
                ? element.length()
                : -1,
            element.expected_modification_time().ToDoubleT());
        break;
      case ResourceRequestBodyImpl::Element::TYPE_BLOB:
        http_body.appendBlob(WebString::fromUTF8(element.blob_uuid()));
        break;
      case ResourceRequestBodyImpl::Element::TYPE_BYTES_DESCRIPTION:
      case ResourceRequestBodyImpl::Element::TYPE_DISK_CACHE_ENTRY:
      default:
        NOTREACHED();
        break;
    }
  }
  return http_body;
}

scoped_refptr<ResourceRequestBodyImpl> GetRequestBodyForWebURLRequest(
    const blink::WebURLRequest& request) {
  scoped_refptr<ResourceRequestBodyImpl> request_body;

  if (request.httpBody().isNull()) {
    return request_body;
  }

  const std::string& method = request.httpMethod().latin1();
  // GET and HEAD requests shouldn't have http bodies.
  DCHECK(method != "GET" && method != "HEAD");

  return GetRequestBodyForWebHTTPBody(request.httpBody());
}

scoped_refptr<ResourceRequestBodyImpl> GetRequestBodyForWebHTTPBody(
    const blink::WebHTTPBody& httpBody) {
  scoped_refptr<ResourceRequestBodyImpl> request_body =
      new ResourceRequestBodyImpl();
  size_t i = 0;
  WebHTTPBody::Element element;
  while (httpBody.elementAt(i++, element)) {
    switch (element.type) {
      case WebHTTPBody::Element::TypeData:
        if (!element.data.isEmpty()) {
          // Blink sometimes gives empty data to append. These aren't
          // necessary so they are just optimized out here.
          request_body->AppendBytes(
              element.data.data(), static_cast<int>(element.data.size()));
        }
        break;
      case WebHTTPBody::Element::TypeFile:
        if (element.fileLength == -1) {
          request_body->AppendFileRange(
              blink::WebStringToFilePath(element.filePath), 0,
              std::numeric_limits<uint64_t>::max(), base::Time());
        } else {
          request_body->AppendFileRange(
              blink::WebStringToFilePath(element.filePath),
              static_cast<uint64_t>(element.fileStart),
              static_cast<uint64_t>(element.fileLength),
              base::Time::FromDoubleT(element.modificationTime));
        }
        break;
      case WebHTTPBody::Element::TypeFileSystemURL: {
        GURL file_system_url = element.fileSystemURL;
        DCHECK(file_system_url.SchemeIsFileSystem());
        request_body->AppendFileSystemFileRange(
            file_system_url, static_cast<uint64_t>(element.fileStart),
            static_cast<uint64_t>(element.fileLength),
            base::Time::FromDoubleT(element.modificationTime));
        break;
      }
      case WebHTTPBody::Element::TypeBlob:
        request_body->AppendBlob(element.blobUUID.utf8());
        break;
      default:
        NOTREACHED();
    }
  }
  request_body->set_identifier(httpBody.identifier());
  return request_body;
}

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_SAME_ORIGIN,
                   WebURLRequest::FetchRequestModeSameOrigin);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_NO_CORS,
                   WebURLRequest::FetchRequestModeNoCORS);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_CORS,
                   WebURLRequest::FetchRequestModeCORS);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
                   WebURLRequest::FetchRequestModeCORSWithForcedPreflight);
STATIC_ASSERT_ENUM(FETCH_REQUEST_MODE_NAVIGATE,
                   WebURLRequest::FetchRequestModeNavigate);

FetchRequestMode GetFetchRequestModeForWebURLRequest(
    const blink::WebURLRequest& request) {
  return static_cast<FetchRequestMode>(request.getFetchRequestMode());
}

STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_OMIT,
                   WebURLRequest::FetchCredentialsModeOmit);
STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_SAME_ORIGIN,
                   WebURLRequest::FetchCredentialsModeSameOrigin);
STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_INCLUDE,
                   WebURLRequest::FetchCredentialsModeInclude);
STATIC_ASSERT_ENUM(FETCH_CREDENTIALS_MODE_PASSWORD,
                   WebURLRequest::FetchCredentialsModePassword);

FetchCredentialsMode GetFetchCredentialsModeForWebURLRequest(
    const blink::WebURLRequest& request) {
  return static_cast<FetchCredentialsMode>(request.getFetchCredentialsMode());
}

STATIC_ASSERT_ENUM(FetchRedirectMode::FOLLOW_MODE,
                   WebURLRequest::FetchRedirectModeFollow);
STATIC_ASSERT_ENUM(FetchRedirectMode::ERROR_MODE,
                   WebURLRequest::FetchRedirectModeError);
STATIC_ASSERT_ENUM(FetchRedirectMode::MANUAL_MODE,
                   WebURLRequest::FetchRedirectModeManual);

FetchRedirectMode GetFetchRedirectModeForWebURLRequest(
    const blink::WebURLRequest& request) {
  return static_cast<FetchRedirectMode>(request.getFetchRedirectMode());
}

STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_AUXILIARY,
                   WebURLRequest::FrameTypeAuxiliary);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_NESTED,
                   WebURLRequest::FrameTypeNested);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_NONE,
                   WebURLRequest::FrameTypeNone);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
                   WebURLRequest::FrameTypeTopLevel);

RequestContextFrameType GetRequestContextFrameTypeForWebURLRequest(
    const blink::WebURLRequest& request) {
  return static_cast<RequestContextFrameType>(request.getFrameType());
}

STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_UNSPECIFIED,
                   WebURLRequest::RequestContextUnspecified);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_AUDIO,
                   WebURLRequest::RequestContextAudio);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_BEACON,
                   WebURLRequest::RequestContextBeacon);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_CSP_REPORT,
                   WebURLRequest::RequestContextCSPReport);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_DOWNLOAD,
                   WebURLRequest::RequestContextDownload);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_EMBED,
                   WebURLRequest::RequestContextEmbed);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_EVENT_SOURCE,
                   WebURLRequest::RequestContextEventSource);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FAVICON,
                   WebURLRequest::RequestContextFavicon);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FETCH,
                   WebURLRequest::RequestContextFetch);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FONT,
                   WebURLRequest::RequestContextFont);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FORM,
                   WebURLRequest::RequestContextForm);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_FRAME,
                   WebURLRequest::RequestContextFrame);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_HYPERLINK,
                   WebURLRequest::RequestContextHyperlink);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IFRAME,
                   WebURLRequest::RequestContextIframe);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IMAGE,
                   WebURLRequest::RequestContextImage);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IMAGE_SET,
                   WebURLRequest::RequestContextImageSet);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_IMPORT,
                   WebURLRequest::RequestContextImport);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_INTERNAL,
                   WebURLRequest::RequestContextInternal);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_LOCATION,
                   WebURLRequest::RequestContextLocation);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_MANIFEST,
                   WebURLRequest::RequestContextManifest);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_OBJECT,
                   WebURLRequest::RequestContextObject);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_PING,
                   WebURLRequest::RequestContextPing);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_PLUGIN,
                   WebURLRequest::RequestContextPlugin);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_PREFETCH,
                   WebURLRequest::RequestContextPrefetch);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SCRIPT,
                   WebURLRequest::RequestContextScript);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SERVICE_WORKER,
                   WebURLRequest::RequestContextServiceWorker);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SHARED_WORKER,
                   WebURLRequest::RequestContextSharedWorker);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_SUBRESOURCE,
                   WebURLRequest::RequestContextSubresource);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_STYLE,
                   WebURLRequest::RequestContextStyle);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_TRACK,
                   WebURLRequest::RequestContextTrack);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_VIDEO,
                   WebURLRequest::RequestContextVideo);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_WORKER,
                   WebURLRequest::RequestContextWorker);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_XML_HTTP_REQUEST,
                   WebURLRequest::RequestContextXMLHttpRequest);
STATIC_ASSERT_ENUM(REQUEST_CONTEXT_TYPE_XSLT,
                   WebURLRequest::RequestContextXSLT);

RequestContextType GetRequestContextTypeForWebURLRequest(
    const blink::WebURLRequest& request) {
  return static_cast<RequestContextType>(request.getRequestContext());
}

STATIC_ASSERT_ENUM(SkipServiceWorker::NONE,
                   WebURLRequest::SkipServiceWorker::None);
STATIC_ASSERT_ENUM(SkipServiceWorker::CONTROLLING,
                   WebURLRequest::SkipServiceWorker::Controlling);
STATIC_ASSERT_ENUM(SkipServiceWorker::ALL,
                   WebURLRequest::SkipServiceWorker::All);

SkipServiceWorker GetSkipServiceWorkerForWebURLRequest(
    const blink::WebURLRequest& request) {
  return static_cast<SkipServiceWorker>(request.skipServiceWorker());
}

blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason) {
  blink::WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = reason;
  error.unreachableURL = unreachable_url;
  error.staleCopyInCache = stale_copy_in_cache;
  if (reason == net::ERR_ABORTED) {
    error.isCancellation = true;
  } else if (reason == net::ERR_TEMPORARILY_THROTTLED) {
    error.localizedDescription =
        WebString::fromUTF8(kThrottledErrorDescription);
  } else {
    error.localizedDescription =
        WebString::fromUTF8(net::ErrorToString(reason));
  }
  return error;
}

blink::WebURLError CreateWebURLError(const blink::WebURL& unreachable_url,
                                     bool stale_copy_in_cache,
                                     int reason,
                                     bool was_ignored_by_handler) {
  blink::WebURLError error =
      CreateWebURLError(unreachable_url, stale_copy_in_cache, reason);
  error.wasIgnoredByHandler = was_ignored_by_handler;
  return error;
}

}  // namespace content
