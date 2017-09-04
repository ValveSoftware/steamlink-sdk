
// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(BackgroundSyncTypeConverterTest, TestBlinkToMojoNetworkStateConversions) {
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAny,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                blink::mojom::BackgroundSyncNetworkState::ANY));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                blink::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR));
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline,
            ConvertTo<blink::WebSyncRegistration::NetworkState>(
                blink::mojom::BackgroundSyncNetworkState::ONLINE));
}

TEST(BackgroundSyncTypeConverterTest, TestMojoToBlinkNetworkStateConversions) {
  ASSERT_EQ(blink::mojom::BackgroundSyncNetworkState::ANY,
            ConvertTo<blink::mojom::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateAny));
  ASSERT_EQ(blink::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR,
            ConvertTo<blink::mojom::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateAvoidCellular));
  ASSERT_EQ(blink::mojom::BackgroundSyncNetworkState::ONLINE,
            ConvertTo<blink::mojom::BackgroundSyncNetworkState>(
                blink::WebSyncRegistration::NetworkStateOnline));
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultBlinkToMojoConversion) {
  blink::WebSyncRegistration in;
  blink::mojom::SyncRegistrationPtr out =
      ConvertTo<blink::mojom::SyncRegistrationPtr>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->id);
  ASSERT_EQ("", out->tag);
  ASSERT_EQ(blink::mojom::BackgroundSyncNetworkState::ONLINE,
            out->network_state);
}

TEST(BackgroundSyncTypeConverterTest, TestFullBlinkToMojoConversion) {
  blink::WebSyncRegistration in(
      7, "BlinkToMojo", blink::WebSyncRegistration::NetworkStateAvoidCellular);
  blink::mojom::SyncRegistrationPtr out =
      ConvertTo<blink::mojom::SyncRegistrationPtr>(in);

  ASSERT_EQ(7, out->id);
  ASSERT_EQ("BlinkToMojo", out->tag);
  ASSERT_EQ(blink::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR,
            out->network_state);
}

TEST(BackgroundSyncTypeConverterTest, TestDefaultMojoToBlinkConversion) {
  blink::mojom::SyncRegistrationPtr in(blink::mojom::SyncRegistration::New());
  std::unique_ptr<blink::WebSyncRegistration> out =
      ConvertTo<std::unique_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(blink::WebSyncRegistration::UNREGISTERED_SYNC_ID, out->id);
  ASSERT_EQ("",out->tag);
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateOnline, out->networkState);
}

TEST(BackgroundSyncTypeConverterTest, TestFullMojoToBlinkConversion) {
  blink::mojom::SyncRegistrationPtr in(blink::mojom::SyncRegistration::New());
  in->id = 41;
  in->tag = mojo::String("MojoToBlink");
  in->network_state = blink::mojom::BackgroundSyncNetworkState::AVOID_CELLULAR;
  std::unique_ptr<blink::WebSyncRegistration> out =
      ConvertTo<std::unique_ptr<blink::WebSyncRegistration>>(in);

  ASSERT_EQ(41, out->id);
  ASSERT_EQ("MojoToBlink", out->tag.utf8());
  ASSERT_EQ(blink::WebSyncRegistration::NetworkStateAvoidCellular,
            out->networkState);
}

}  // anonymous namespace
}  // namespace mojo

