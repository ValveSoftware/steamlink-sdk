// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_owner_properties.h"

namespace content {

FrameOwnerProperties::FrameOwnerProperties()
    : scrolling_mode(blink::WebFrameOwnerProperties::ScrollingMode::Auto),
      margin_width(-1),
      margin_height(-1),
      allow_fullscreen(false),
      allow_payment_request(false) {}

FrameOwnerProperties::FrameOwnerProperties(const FrameOwnerProperties& other) =
    default;

FrameOwnerProperties::FrameOwnerProperties(
    const blink::WebFrameOwnerProperties& web_frame_owner_properties)
    : scrolling_mode(web_frame_owner_properties.scrollingMode),
      margin_width(web_frame_owner_properties.marginWidth),
      margin_height(web_frame_owner_properties.marginHeight),
      allow_fullscreen(web_frame_owner_properties.allowFullscreen),
      allow_payment_request(web_frame_owner_properties.allowPaymentRequest),
      required_csp(web_frame_owner_properties.requiredCsp.utf8()),
      delegated_permissions(
          web_frame_owner_properties.delegatedPermissions.begin(),
          web_frame_owner_properties.delegatedPermissions.end()) {}

FrameOwnerProperties::~FrameOwnerProperties() {}

blink::WebFrameOwnerProperties FrameOwnerProperties::ToWebFrameOwnerProperties()
    const {
  blink::WebFrameOwnerProperties result;

  result.scrollingMode = scrolling_mode;
  result.marginWidth = margin_width;
  result.marginHeight = margin_height;
  result.allowFullscreen = allow_fullscreen;
  result.allowPaymentRequest = allow_payment_request;
  result.requiredCsp = blink::WebString::fromUTF8(required_csp);
  result.delegatedPermissions =
      blink::WebVector<blink::WebPermissionType>(delegated_permissions);

  return result;
}

bool FrameOwnerProperties::operator==(const FrameOwnerProperties& other) const {
  return scrolling_mode == other.scrolling_mode &&
         margin_width == other.margin_width &&
         margin_height == other.margin_height &&
         allow_fullscreen == other.allow_fullscreen &&
         allow_payment_request == other.allow_payment_request &&
         required_csp == other.required_csp &&
         std::equal(delegated_permissions.begin(), delegated_permissions.end(),
                    other.delegated_permissions.begin());
}

}  // namespace content
