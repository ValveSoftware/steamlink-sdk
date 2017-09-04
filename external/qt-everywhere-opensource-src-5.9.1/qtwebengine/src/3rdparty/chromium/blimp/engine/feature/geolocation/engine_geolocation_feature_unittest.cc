// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/engine_geolocation_feature.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/geolocation.pb.h"
#include "blimp/net/test_common.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"
#include "net/base/test_completion_callback.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace engine {

void SendMockLocationMessage(BlimpMessageProcessor* processor) {
  GeolocationMessage* geolocation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&geolocation_message);

  GeolocationCoordinatesMessage* coordinates_message =
      geolocation_message->mutable_coordinates();
  coordinates_message->set_latitude(-42.0);
  coordinates_message->set_longitude(17.3);
  coordinates_message->set_altitude(123.4);
  coordinates_message->set_accuracy(73.7);

  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
  base::RunLoop().RunUntilIdle();
}

void SendMockErrorMessage(BlimpMessageProcessor* processor,
                          GeolocationErrorMessage::ErrorCode error,
                          std::string error_message) {
  GeolocationMessage* geolocation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&geolocation_message);
  GeolocationErrorMessage* geolocation_error_message =
      geolocation_message->mutable_error();
  geolocation_error_message->set_error_code(error);
  geolocation_error_message->set_error_message(error_message);

  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
  base::RunLoop().RunUntilIdle();
}

void SendMalformedMessage(BlimpMessageProcessor* processor) {
  GeolocationMessage* geolocation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&geolocation_message);

  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

std::unique_ptr<BlimpMessage> CreateUnexpectedMessage() {
  GeolocationMessage* geolocation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&geolocation_message);
  GeolocationSetInterestLevelMessage* geolocation_set_interest_level =
      geolocation_message->mutable_set_interest_level();
  geolocation_set_interest_level->set_level(
      GeolocationSetInterestLevelMessage::NO_INTEREST);
  return message;
}

MATCHER_P(EqualsUpdatedRequestLevel, level, "") {
  return arg.feature_case() == BlimpMessage::kGeolocation &&
         arg.geolocation().type_case() ==
             GeolocationMessage::kSetInterestLevel &&
         arg.geolocation().set_interest_level().level() == level;
}

MATCHER(EqualsRequestRefresh, "") {
  return arg.feature_case() == BlimpMessage::kGeolocation &&
         arg.geolocation().type_case() == GeolocationMessage::kRequestRefresh;
}

class EngineGeolocationFeatureTest : public testing::Test {
 public:
  EngineGeolocationFeatureTest()
      : out_processor_(nullptr),
        location_provider_(feature_.CreateGeolocationDelegate()
                               ->OverrideSystemLocationProvider()),
        mock_callback_(
            base::Bind(&EngineGeolocationFeatureTest::OnLocationUpdate,
                       base::Unretained(this))) {}

  void SetUp() override {
    out_processor_ = new MockBlimpMessageProcessor();
    feature_.set_outgoing_message_processor(base::WrapUnique(out_processor_));
    location_provider_->SetUpdateCallback(mock_callback_);
    base::RunLoop().RunUntilIdle();
  }

  void OnLocationUpdate(const device::LocationProvider* provider,
                        const device::Geoposition& geoposition) {
    received_position = geoposition;
  }

 protected:
  // This is a raw pointer to a class that is owned by the GeolocationFeature.
  MockBlimpMessageProcessor* out_processor_;

  // Processes tasks that were posted as a result of processing of incoming
  // messages.
  base::MessageLoop message_loop_;

  EngineGeolocationFeature feature_;
  std::unique_ptr<device::LocationProvider> location_provider_;
  device::LocationProvider::LocationProviderUpdateCallback mock_callback_;
  device::Geoposition received_position;
};

TEST_F(EngineGeolocationFeatureTest, LocationReceived) {
  SendMockLocationMessage(&feature_);
  EXPECT_EQ(device::Geoposition::ERROR_CODE_NONE,
            received_position.error_code);
  EXPECT_EQ(-42.0, received_position.latitude);
  EXPECT_EQ(17.3, received_position.longitude);
  EXPECT_EQ(123.4, received_position.altitude);
  EXPECT_EQ(73.7, received_position.accuracy);
}

TEST_F(EngineGeolocationFeatureTest, ErrorRecieved) {
  SendMockErrorMessage(&feature_, GeolocationErrorMessage::PERMISSION_DENIED,
                       "PERMISSION_DENIED");
  EXPECT_EQ(device::Geoposition::ERROR_CODE_PERMISSION_DENIED,
            received_position.error_code);
  EXPECT_EQ("PERMISSION_DENIED", received_position.error_message);

  SendMockErrorMessage(&feature_, GeolocationErrorMessage::POSITION_UNAVAILABLE,
                       "POSITION_UNAVAILABLE");
  EXPECT_EQ(device::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE,
            received_position.error_code);
  EXPECT_EQ("POSITION_UNAVAILABLE", received_position.error_message);
}

TEST_F(EngineGeolocationFeatureTest, UpdateRequestLevel) {
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsUpdatedRequestLevel(
                      GeolocationSetInterestLevelMessage::HIGH_ACCURACY),
                  _))
      .Times(1);
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsUpdatedRequestLevel(
                      GeolocationSetInterestLevelMessage::LOW_ACCURACY),
                  _))
      .Times(1);
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsUpdatedRequestLevel(
                      GeolocationSetInterestLevelMessage::NO_INTEREST),
                  _))
      .Times(1);

  location_provider_->StartProvider(true);
  location_provider_->StartProvider(false);
  location_provider_->StopProvider();
  base::RunLoop().RunUntilIdle();
}

TEST_F(EngineGeolocationFeatureTest, UnexpectedMessageReceived) {
  std::unique_ptr<BlimpMessage> message = CreateUnexpectedMessage();
  net::TestCompletionCallback cb;
  feature_.ProcessMessage(std::move(message), cb.callback());

  EXPECT_EQ(net::ERR_UNEXPECTED, cb.WaitForResult());
}

TEST_F(EngineGeolocationFeatureTest,
       OnPermissionGrantedTriggersRefreshRequest) {
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsUpdatedRequestLevel(
                      GeolocationSetInterestLevelMessage::HIGH_ACCURACY),
                  _))
      .Times(1);

  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualsRequestRefresh(), _))
      .Times(1);

  location_provider_->StartProvider(true);
  location_provider_->OnPermissionGranted();
  base::RunLoop().RunUntilIdle();
}

}  // namespace engine
}  // namespace blimp
