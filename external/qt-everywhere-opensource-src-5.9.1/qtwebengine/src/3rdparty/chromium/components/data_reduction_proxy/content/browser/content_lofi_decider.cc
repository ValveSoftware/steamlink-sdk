// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

#include <string>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace data_reduction_proxy {

ContentLoFiDecider::ContentLoFiDecider() {}

ContentLoFiDecider::~ContentLoFiDecider() {}

bool ContentLoFiDecider::IsUsingLoFiMode(const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  // The Lo-Fi directive should not be added for users in the Lo-Fi field
  // trial "Control" group. Check that the user is in a group that can get
  // "q=low".
  bool lofi_enabled_via_flag_or_field_trial =
      params::IsLoFiOnViaFlags() || params::IsIncludedInLoFiEnabledFieldTrial();

  // Return if the user is using Lo-Fi and not part of the "Control" group.
  if (request_info)
    return request_info->IsUsingLoFi() && lofi_enabled_via_flag_or_field_trial;
  return false;
}

void ContentLoFiDecider::MaybeSetAcceptTransformHeader(
    const net::URLRequest& request,
    bool is_previews_disabled,
    net::HttpRequestHeaders* headers) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);

  if (!request_info)
    return;

  // Previews only operate on HTTP.
  if (!request.url().SchemeIs("http"))
    return;

  // Chrome-Proxy-Accept-Transform takes at most one token.
  if (headers->HasHeader(chrome_proxy_accept_transform_header()))
    return;

  content::ResourceType resource_type = request_info->GetResourceType();

  if (resource_type == content::RESOURCE_TYPE_MEDIA) {
    headers->SetHeader(chrome_proxy_accept_transform_header(),
                       compressed_video_directive());
    return;
  }

  // The Lo-Fi and Lite Page directives should not be added for users in the
  // Lo-Fi field trial "Control" group.
  bool lofi_enabled_via_flags_or_field_trial =
      params::IsLoFiOnViaFlags() || params::IsIncludedInLoFiEnabledFieldTrial();

  bool lite_page_enabled_via_flags_or_field_trial =
      (params::IsLoFiOnViaFlags() && params::AreLitePagesEnabledViaFlags()) ||
      params::IsIncludedInLitePageFieldTrial();

  // User does not have previews enabled.
  if (!lofi_enabled_via_flags_or_field_trial &&
      !lite_page_enabled_via_flags_or_field_trial) {
    return;
  }

  // Previews has been disabled.
  if (is_previews_disabled)
    return;

  // LoFi is not allowed on the main frame, stylesheet, script, font resource,
  // media, service worker, or CSP report.
  bool resource_type_supports_empty_image =
      !(resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
        resource_type == content::RESOURCE_TYPE_STYLESHEET ||
        resource_type == content::RESOURCE_TYPE_SCRIPT ||
        resource_type == content::RESOURCE_TYPE_FONT_RESOURCE ||
        resource_type == content::RESOURCE_TYPE_MEDIA ||
        resource_type == content::RESOURCE_TYPE_CSP_REPORT);

  // If in the lite page field trial or the lite page flag is enabled, only add
  // the "lite-page" directive on main frame requests. Do not add "empty-image"
  // directives to other requests when Lite Page previews are enabled.
  // Add the "if-heavy" qualifier to allow the server to provide a preview when
  // the page is data heavy on if a preview was not otherwise triggered.
  std::string accept_transform_value;
  if (lite_page_enabled_via_flags_or_field_trial) {
    if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
      accept_transform_value = lite_page_directive();
  } else if (lofi_enabled_via_flags_or_field_trial) {
    if (resource_type_supports_empty_image)
      accept_transform_value = empty_image_directive();
  }
  if (accept_transform_value.empty())
    return;

  if (!request_info->IsUsingLoFi())
    accept_transform_value += base::StringPrintf(";%s", if_heavy_qualifier());

  headers->SetHeader(chrome_proxy_accept_transform_header(),
                     accept_transform_value);
}

bool ContentLoFiDecider::IsSlowPagePreviewRequested(
    const net::HttpRequestHeaders& headers) const {
  std::string accept_transform_header_value;
  if (!headers.GetHeader(chrome_proxy_accept_transform_header(),
                         &accept_transform_header_value)) {
    return false;
  }

  std::vector<std::string> tokens =
      base::SplitString(base::ToLowerASCII(accept_transform_header_value), ";",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // A slow page preview is a request for any unqualified transform type.
  if (tokens.size() != 1)
    return false;
  std::string transform_type;
  base::TrimWhitespaceASCII(tokens[0], base::TRIM_ALL, &transform_type);
  return (transform_type == lite_page_directive() ||
          transform_type == empty_image_directive());
}

bool ContentLoFiDecider::IsLitePagePreviewRequested(
    const net::HttpRequestHeaders& headers) const {
  std::string accept_transform_header_value;
  if (!headers.GetHeader(chrome_proxy_accept_transform_header(),
                         &accept_transform_header_value)) {
    return false;
  }
  std::vector<std::string> tokens =
      base::SplitString(base::ToLowerASCII(accept_transform_header_value), ";",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.empty())
    return false;
  std::string transform_type;
  base::TrimWhitespaceASCII(tokens[0], base::TRIM_ALL, &transform_type);
  return transform_type == lite_page_directive();
}

void ContentLoFiDecider::RemoveAcceptTransformHeader(
    net::HttpRequestHeaders* headers) const {
  headers->RemoveHeader(chrome_proxy_accept_transform_header());
}

void ContentLoFiDecider::MaybeSetIgnorePreviewsBlacklistDirective(
    net::HttpRequestHeaders* headers) const {
  if (!headers || !params::AreLitePagesEnabledViaFlags() ||
      !IsLitePagePreviewRequested(*headers)) {
    return;
  }
  std::string chrome_proxy_header_value;
  headers->GetHeader(chrome_proxy_header(), &chrome_proxy_header_value);
  headers->RemoveHeader(chrome_proxy_header());
  if (!chrome_proxy_header_value.empty())
    chrome_proxy_header_value += ", ";
  chrome_proxy_header_value +=
      chrome_proxy_lite_page_ignore_blacklist_directive();
  headers->SetHeader(chrome_proxy_header(), chrome_proxy_header_value);
}

bool ContentLoFiDecider::ShouldRecordLoFiUMA(
    const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);

  // User is not using Lo-Fi.
  if (!request_info || !request_info->IsUsingLoFi())
    return false;

  return params::IsIncludedInLoFiEnabledFieldTrial() ||
         params::IsIncludedInLoFiControlFieldTrial();
}

}  // namespace data_reduction_proxy
