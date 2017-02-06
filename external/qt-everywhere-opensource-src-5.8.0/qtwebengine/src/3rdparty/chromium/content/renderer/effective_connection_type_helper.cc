// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/effective_connection_type_helper.h"

namespace content {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(
    net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
    blink::WebEffectiveConnectionType::TypeUnknown);
STATIC_ASSERT_ENUM(
    net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
    blink::WebEffectiveConnectionType::TypeOffline);
STATIC_ASSERT_ENUM(
    net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
    blink::WebEffectiveConnectionType::TypeSlow2G);
STATIC_ASSERT_ENUM(net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_2G,
                   blink::WebEffectiveConnectionType::Type2G);
STATIC_ASSERT_ENUM(net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_3G,
                   blink::WebEffectiveConnectionType::Type3G);
STATIC_ASSERT_ENUM(net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_4G,
                   blink::WebEffectiveConnectionType::Type4G);
STATIC_ASSERT_ENUM(
    net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_BROADBAND,
    blink::WebEffectiveConnectionType::TypeBroadband);

blink::WebEffectiveConnectionType
EffectiveConnectionTypeToWebEffectiveConnectionType(
    net::NetworkQualityEstimator::EffectiveConnectionType net_type) {
  return static_cast<blink::WebEffectiveConnectionType>(net_type);
}

}  // namespace content
