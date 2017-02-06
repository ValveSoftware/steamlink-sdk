// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/link_header_support.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

void RegisterServiceWorkerFinished(int64_t trace_id, bool result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "LinkHeaderResourceThrottle::HandleServiceWorkerLink",
                         trace_id, "Success", result);
}

void HandleServiceWorkerLink(
    const net::URLRequest* request,
    const std::string& url,
    const std::unordered_map<std::string, base::Optional<std::string>>& params,
    ServiceWorkerContextWrapper* service_worker_context_for_testing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    // TODO(mek): Integrate with experimental framework.
    return;
  }

  if (ContainsKey(params, "anchor"))
    return;

  const ResourceRequestInfoImpl* request_info =
      ResourceRequestInfoImpl::ForRequest(request);
  ResourceMessageFilter* filter = request_info->filter();
  ServiceWorkerContext* service_worker_context =
      filter ? filter->service_worker_context()
             : service_worker_context_for_testing;
  if (!service_worker_context)
    return;

  if (ServiceWorkerUtils::IsMainResourceType(request_info->GetResourceType())) {
    // In case of navigations, make sure the navigation will actually result in
    // a secure context.
    ServiceWorkerProviderHost* provider_host =
        ServiceWorkerRequestHandler::GetProviderHost(request);
    if (!provider_host || !provider_host->IsContextSecureForServiceWorker())
      return;
  } else {
    // If this is not a navigation, make sure the request was initiated from a
    // secure context.
    if (!request_info->initiated_in_secure_context())
      return;
  }

  // TODO(mek): support for a serviceworker link on a request that wouldn't ever
  // be able to be intercepted by a serviceworker isn't very useful, so this
  // should share logic with ServiceWorkerRequestHandler and
  // ForeignFetchRequestHandler to limit the requests for which serviceworker
  // links are processed.

  GURL context_url = request->url();
  GURL script_url = context_url.Resolve(url);
  auto scope_param = params.find("scope");
  GURL scope_url = scope_param == params.end()
                       ? script_url.Resolve("./")
                       : context_url.Resolve(scope_param->second.value_or(""));

  if (!context_url.is_valid() || !script_url.is_valid() ||
      !scope_url.is_valid())
    return;
  if (!ServiceWorkerUtils::CanRegisterServiceWorker(context_url, scope_url,
                                                    script_url))
    return;
  std::string error;
  if (ServiceWorkerUtils::ContainsDisallowedCharacter(scope_url, script_url,
                                                      &error))
    return;

  int render_process_id = -1;
  int render_frame_id = -1;
  ResourceRequestInfo::GetRenderFrameForRequest(request, &render_process_id,
                                                &render_frame_id);

  if (!GetContentClient()->browser()->AllowServiceWorker(
          scope_url, request->first_party_for_cookies(),
          request_info->GetContext(), render_process_id, render_frame_id))
    return;

  static int64_t trace_id = 0;
  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "LinkHeaderResourceThrottle::HandleServiceWorkerLink",
      ++trace_id, "Pattern", scope_url.spec(), "Script URL", script_url.spec());
  service_worker_context->RegisterServiceWorker(
      scope_url, script_url,
      base::Bind(&RegisterServiceWorkerFinished, trace_id));
}

void ProcessLinkHeaderValueForRequest(
    const net::URLRequest* request,
    std::string::const_iterator value_begin,
    std::string::const_iterator value_end,
    ServiceWorkerContextWrapper* service_worker_context_for_testing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string url;
  std::unordered_map<std::string, base::Optional<std::string>> params;
  if (!link_header_util::ParseLinkHeaderValue(value_begin, value_end, &url,
                                              &params))
    return;

  auto rel_param = params.find("rel");
  if (rel_param == params.end() || !rel_param->second)
    return;
  for (const auto& rel : base::SplitStringPiece(rel_param->second.value(),
                                                HTTP_LWS, base::TRIM_WHITESPACE,
                                                base::SPLIT_WANT_NONEMPTY)) {
    if (base::EqualsCaseInsensitiveASCII(rel, "serviceworker"))
      HandleServiceWorkerLink(request, url, params,
                              service_worker_context_for_testing);
  }
}

}  // namespace

void ProcessRequestForLinkHeaders(const net::URLRequest* request) {
  std::string link_header;
  request->GetResponseHeaderByName("link", &link_header);
  if (link_header.empty())
    return;

  ProcessLinkHeaderForRequest(request, link_header);
}

void ProcessLinkHeaderForRequest(
    const net::URLRequest* request,
    const std::string& link_header,
    ServiceWorkerContextWrapper* service_worker_context_for_testing) {
  for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
    ProcessLinkHeaderValueForRequest(request, value.first, value.second,
                                     service_worker_context_for_testing);
  }
}

}  // namespace content
