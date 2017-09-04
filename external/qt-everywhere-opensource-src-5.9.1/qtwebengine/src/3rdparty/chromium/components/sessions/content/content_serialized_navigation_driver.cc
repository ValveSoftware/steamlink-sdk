// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_driver.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"

namespace sessions {

namespace {
const int kObsoleteReferrerPolicyAlways = 0;
const int kObsoleteReferrerPolicyDefault = 1;
const int kObsoleteReferrerPolicyNever = 2;
const int kObsoleteReferrerPolicyOrigin = 3;

bool IsUberOrUberReplacementURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         (url.host_piece() == content::kChromeUIHistoryHost ||
          url.host_piece() == content::kChromeUIUberHost);
}

}  // namespace

// static
SerializedNavigationDriver* SerializedNavigationDriver::Get() {
  return ContentSerializedNavigationDriver::GetInstance();
}

// static
ContentSerializedNavigationDriver*
ContentSerializedNavigationDriver::GetInstance() {
  return base::Singleton<
      ContentSerializedNavigationDriver,
      base::LeakySingletonTraits<ContentSerializedNavigationDriver>>::get();
}

ContentSerializedNavigationDriver::ContentSerializedNavigationDriver() {
}

ContentSerializedNavigationDriver::~ContentSerializedNavigationDriver() {
}

int ContentSerializedNavigationDriver::GetDefaultReferrerPolicy() const {
  return blink::WebReferrerPolicyDefault;
}

bool ContentSerializedNavigationDriver::MapReferrerPolicyToOldValues(
    int referrer_policy,
    int* mapped_referrer_policy) const {
  switch (referrer_policy) {
    case blink::WebReferrerPolicyAlways:
    case blink::WebReferrerPolicyDefault:
      // "always" and "default" are the same value in all versions.
      *mapped_referrer_policy = referrer_policy;
      return true;

    case blink::WebReferrerPolicyOrigin:
      // "origin" exists in the old encoding.
      *mapped_referrer_policy = kObsoleteReferrerPolicyOrigin;
      return true;

    default:
      // Everything else is mapped to never.
      *mapped_referrer_policy = kObsoleteReferrerPolicyNever;
      return false;
  }
}

bool ContentSerializedNavigationDriver::MapReferrerPolicyToNewValues(
    int referrer_policy,
    int* mapped_referrer_policy) const {
  switch (referrer_policy) {
    case kObsoleteReferrerPolicyAlways:
    case kObsoleteReferrerPolicyDefault:
      // "always" and "default" are the same value in all versions.
      *mapped_referrer_policy = referrer_policy;
      return true;

    default:
      // Since we don't know what encoding was used, we map the rest to "never".
      *mapped_referrer_policy = blink::WebReferrerPolicyNever;
      return false;
  }
}

std::string
ContentSerializedNavigationDriver::GetSanitizedPageStateForPickle(
    const SerializedNavigationEntry* navigation) const {
  if (!navigation->has_post_data_) {
    return navigation->encoded_page_state_;
  }
  content::PageState page_state =
      content::PageState::CreateFromEncodedData(
          navigation->encoded_page_state_);
  return page_state.RemovePasswordData().ToEncodedData();
}

void ContentSerializedNavigationDriver::Sanitize(
    SerializedNavigationEntry* navigation) const {
  content::Referrer old_referrer(
      navigation->referrer_url_,
      static_cast<blink::WebReferrerPolicy>(navigation->referrer_policy_));
  content::Referrer new_referrer =
      content::Referrer::SanitizeForRequest(navigation->virtual_url_,
                                            old_referrer);

  // Clear any Uber UI page state so that these pages are reloaded rather than
  // restored from page state. This fixes session restore when WebUI URLs
  // change.
  if (IsUberOrUberReplacementURL(navigation->virtual_url_) &&
      IsUberOrUberReplacementURL(navigation->original_request_url_)) {
    navigation->encoded_page_state_ = std::string();
  }

  // No need to compare the policy, as it doesn't change during
  // sanitization. If there has been a change, the referrer needs to be
  // stripped from the page state as well.
  if (navigation->referrer_url_ != new_referrer.url) {
    navigation->referrer_url_ = GURL();
    navigation->referrer_policy_ = GetDefaultReferrerPolicy();
    navigation->encoded_page_state_ =
        StripReferrerFromPageState(navigation->encoded_page_state_);
  }

#if defined(OS_ANDROID)
  // Rewrite the old new tab and welcome page URLs to the new NTP URL.
  if (navigation->virtual_url_.SchemeIs(content::kChromeUIScheme) &&
      (navigation->virtual_url_.host_piece() == "welcome" ||
       navigation->virtual_url_.host_piece() == "newtab")) {
    navigation->virtual_url_ = GURL("chrome-native://newtab/");
    navigation->original_request_url_ = navigation->virtual_url_;
    navigation->encoded_page_state_ = content::PageState::CreateFromURL(
        navigation->virtual_url_).ToEncodedData();
  }
#endif  // defined(OS_ANDROID)
}

std::string ContentSerializedNavigationDriver::StripReferrerFromPageState(
      const std::string& page_state) const {
  return content::PageState::CreateFromEncodedData(page_state)
      .RemoveReferrer()
      .ToEncodedData();
}

void ContentSerializedNavigationDriver::RegisterExtendedInfoHandler(
    const std::string& key,
    std::unique_ptr<ExtendedInfoHandler> handler) {
  DCHECK(!key.empty());
  DCHECK(!extended_info_handler_map_.count(key));
  DCHECK(handler.get());
  extended_info_handler_map_[key] = std::move(handler);
}

const ContentSerializedNavigationDriver::ExtendedInfoHandlerMap&
ContentSerializedNavigationDriver::GetAllExtendedInfoHandlers() const {
  return extended_info_handler_map_;
}

}  // namespace sessions
