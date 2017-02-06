// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

#include <string>

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

bool ContentLoFiDecider::MaybeAddLoFiDirectiveToHeaders(
    const net::URLRequest& request,
    net::HttpRequestHeaders* headers) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);

  if (!request_info)
    return false;

  // The Lo-Fi directive should not be added for users in the Lo-Fi field
  // trial "Control" group. Check that the user is in a group that should
  // get "q=low".
  bool lofi_enabled_via_flag_or_field_trial =
      params::IsLoFiOnViaFlags() || params::IsIncludedInLoFiEnabledFieldTrial();

  bool lofi_preview_via_flag_or_field_trial =
      params::AreLoFiPreviewsEnabledViaFlags() ||
      params::IsIncludedInLoFiPreviewFieldTrial();

  // User is not using Lo-Fi or is part of the "Control" group.
  if (!request_info->IsUsingLoFi() || !lofi_enabled_via_flag_or_field_trial)
    return false;

  std::string header_value;

  if (headers->HasHeader(chrome_proxy_header())) {
    headers->GetHeader(chrome_proxy_header(), &header_value);
    headers->RemoveHeader(chrome_proxy_header());
    header_value += ", ";
  }

  // If in the preview field trial or the preview flag is enabled, only add the
  // "q=preview" directive on main frame requests. Do not add Lo-Fi directives
  // to other requests when previews are enabled.
  if (lofi_preview_via_flag_or_field_trial) {
    if (request.load_flags() & net::LOAD_MAIN_FRAME) {
      if (params::AreLoFiPreviewsEnabledViaFlags()) {
        header_value += chrome_proxy_lo_fi_ignore_preview_blacklist_directive();
        header_value += ", ";
      }
      header_value += chrome_proxy_lo_fi_preview_directive();
    }
  } else if (!(request.load_flags() & net::LOAD_MAIN_FRAME)) {
    // If previews are not enabled, add "q=low" for requests that are not main
    // frame.
    header_value += chrome_proxy_lo_fi_directive();
  }

  if (!header_value.empty())
    headers->SetHeader(chrome_proxy_header(), header_value);

  return true;
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
