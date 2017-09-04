// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_

namespace subresource_filter {

enum class ActivationList {
  NONE,
  SOCIAL_ENG_ADS_INTERSTITIAL,
  PHISHING_INTERSTITIAL,
  LAST = PHISHING_INTERSTITIAL,
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_ACTIVATION_LIST_H_
