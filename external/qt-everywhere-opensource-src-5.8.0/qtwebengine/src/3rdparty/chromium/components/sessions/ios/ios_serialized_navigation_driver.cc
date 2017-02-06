// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_serialized_navigation_driver.h"

#include "base/memory/singleton.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "ios/web/public/referrer.h"

namespace sessions {

namespace {
const int kObsoleteReferrerPolicyAlways = 0;
const int kObsoleteReferrerPolicyDefault = 1;
const int kObsoleteReferrerPolicyNever = 2;
const int kObsoleteReferrerPolicyOrigin = 3;
}  // namespace

// static
SerializedNavigationDriver* SerializedNavigationDriver::Get() {
  return IOSSerializedNavigationDriver::GetInstance();
}

// static
IOSSerializedNavigationDriver*
IOSSerializedNavigationDriver::GetInstance() {
  return base::Singleton<
      IOSSerializedNavigationDriver,
      base::LeakySingletonTraits<IOSSerializedNavigationDriver>>::get();
}

IOSSerializedNavigationDriver::IOSSerializedNavigationDriver() {
}

IOSSerializedNavigationDriver::~IOSSerializedNavigationDriver() {
}

int IOSSerializedNavigationDriver::GetDefaultReferrerPolicy() const {
  return web::ReferrerPolicyDefault;
}

bool IOSSerializedNavigationDriver::MapReferrerPolicyToOldValues(
    int referrer_policy,
    int* mapped_referrer_policy) const {
  switch (referrer_policy) {
    case web::ReferrerPolicyAlways:
    case web::ReferrerPolicyDefault:
      // "always" and "default" are the same value in all versions.
      *mapped_referrer_policy = referrer_policy;
      return true;

    case web::ReferrerPolicyOrigin:
      // "origin" exists in the old encoding.
      *mapped_referrer_policy = kObsoleteReferrerPolicyOrigin;
      return true;

    default:
      // Everything else is mapped to never.
      *mapped_referrer_policy = kObsoleteReferrerPolicyNever;
      return false;
  }
}

bool IOSSerializedNavigationDriver::MapReferrerPolicyToNewValues(
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
      *mapped_referrer_policy = web::ReferrerPolicyNever;
      return false;
  }
}

std::string
IOSSerializedNavigationDriver::GetSanitizedPageStateForPickle(
    const SerializedNavigationEntry* navigation) const {
  return std::string();
}

void IOSSerializedNavigationDriver::Sanitize(
    SerializedNavigationEntry* navigation) const {
  web::Referrer referrer(
      navigation->referrer_url_,
      static_cast<web::ReferrerPolicy>(navigation->referrer_policy_));

  if (!navigation->virtual_url_.SchemeIsHTTPOrHTTPS() ||
      !referrer.url.SchemeIsHTTPOrHTTPS()) {
    referrer.url = GURL();
  } else {
    if (referrer.policy < 0 || referrer.policy > web::ReferrerPolicyLast) {
      NOTREACHED();
      referrer.policy = web::ReferrerPolicyNever;
    }
    bool is_downgrade = referrer.url.SchemeIsCryptographic() &&
                        !navigation->virtual_url_.SchemeIsCryptographic();
    switch (referrer.policy) {
      case web::ReferrerPolicyDefault:
        if (is_downgrade)
          referrer.url = GURL();
        break;
      case web::ReferrerPolicyNoReferrerWhenDowngrade:
        if (is_downgrade)
          referrer.url = GURL();
      case web::ReferrerPolicyAlways:
        break;
      case web::ReferrerPolicyNever:
        referrer.url = GURL();
        break;
      case web::ReferrerPolicyOrigin:
        referrer.url = referrer.url.GetOrigin();
        break;
      case web::ReferrerPolicyOriginWhenCrossOrigin:
        if (navigation->virtual_url_.GetOrigin() != referrer.url.GetOrigin())
          referrer.url = referrer.url.GetOrigin();
        break;
    }
  }

  // Reset the referrer if it has changed.
  if (navigation->referrer_url_ != referrer.url) {
    navigation->referrer_url_ = GURL();
    navigation->referrer_policy_ = GetDefaultReferrerPolicy();
  }
}

std::string IOSSerializedNavigationDriver::StripReferrerFromPageState(
      const std::string& page_state) const {
  return std::string();
}

}  // namespace sessions
