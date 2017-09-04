// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/geolocation/geolocation_feature.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/test_common.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"
#include "device/geolocation/mock_location_provider.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;
using testing::Invoke;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace blimp {
namespace client {

const double kLatitude = -42.0;
const double kLongitude = 17.3;
const double kAltitude = 123.4;
const double kAccuracy = 73.7;

MATCHER(EqualsDefaultGeoposition, "") {
  return arg.feature_case() == BlimpMessage::kGeolocation &&
         arg.geolocation().type_case() == GeolocationMessage::kCoordinates &&
         arg.geolocation().coordinates().latitude() == kLatitude &&
         arg.geolocation().coordinates().longitude() == kLongitude &&
         arg.geolocation().coordinates().altitude() == kAltitude &&
         arg.geolocation().coordinates().accuracy() == kAccuracy;
}

MATCHER_P4(EqualGeoposition, lat, lon, alt, acc, "") {
  return arg.feature_case() == BlimpMessage::kGeolocation &&
         arg.geolocation().type_case() == GeolocationMessage::kCoordinates &&
         arg.geolocation().coordinates().latitude() == lat &&
         arg.geolocation().coordinates().longitude() == lon &&
         arg.geolocation().coordinates().altitude() == alt &&
         arg.geolocation().coordinates().accuracy() == acc;
}

MATCHER_P(EqualsError, error_code, "") {
  return arg.feature_case() == BlimpMessage::kGeolocation &&
         arg.geolocation().type_case() == GeolocationMessage::kError &&
         arg.geolocation().error().error_code() == error_code;
}

class GeolocationFeatureTest : public testing::Test {
 public:
  GeolocationFeatureTest() {}

  void SetUp() override {
    auto location_provider =
        base::MakeUnique<StrictMock<device::MockLocationProvider>>();
    location_provider_ = location_provider.get();
    EXPECT_CALL(*location_provider_, SetUpdateCallback(_))
        .WillOnce(SaveArg<0>(&callback_));
    feature_ =
        base::MakeUnique<GeolocationFeature>(std::move(location_provider));

    auto out_processor =
        base::MakeUnique<StrictMock<MockBlimpMessageProcessor>>();
    out_processor_ = out_processor.get();
    feature_->set_outgoing_message_processor(std::move(out_processor));

    position_.latitude = kLatitude;
    position_.longitude = kLongitude;
    position_.altitude = kAltitude;
    position_.accuracy = kAccuracy;
  }

 protected:
  void SendMockSetInterestLevelMessage(
      GeolocationSetInterestLevelMessage::Level level) {
    GeolocationMessage* geolocation_message;
    std::unique_ptr<BlimpMessage> message =
        CreateBlimpMessage(&geolocation_message);

    GeolocationSetInterestLevelMessage* interest_message =
        geolocation_message->mutable_set_interest_level();
    interest_message->set_level(level);

    net::TestCompletionCallback cb;
    feature_->ProcessMessage(std::move(message), cb.callback());
    EXPECT_EQ(net::OK, cb.WaitForResult());
  }

  void ReportProcessMessageSuccess(const BlimpMessage& blimp_message,
                                   const net::CompletionCallback& callback) {
    callback.Run(net::OK);
  }

  // These are raw pointers to classes that are owned by the
  // GeolocationFeature.
  StrictMock<MockBlimpMessageProcessor>* out_processor_;
  StrictMock<device::MockLocationProvider>* location_provider_;

  std::unique_ptr<GeolocationFeature> feature_;
  device::LocationProvider::LocationProviderUpdateCallback callback_;
  device::Geoposition position_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationFeatureTest);
};

TEST_F(GeolocationFeatureTest, UpdateInterestLevelReceived) {
  InSequence s;

  EXPECT_CALL(*location_provider_, StartProvider(true));
  EXPECT_CALL(*location_provider_, StopProvider());
  EXPECT_CALL(*location_provider_, StartProvider(false));

  SendMockSetInterestLevelMessage(
      GeolocationSetInterestLevelMessage::HIGH_ACCURACY);
  SendMockSetInterestLevelMessage(
      GeolocationSetInterestLevelMessage::NO_INTEREST);
  SendMockSetInterestLevelMessage(
      GeolocationSetInterestLevelMessage::LOW_ACCURACY);
}

TEST_F(GeolocationFeatureTest, UnexpectedMessageReceived) {
  GeolocationMessage* geolocation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&geolocation_message);

  GeolocationCoordinatesMessage* coordinates_message =
      geolocation_message->mutable_coordinates();
  coordinates_message->set_latitude(1.0);

  net::TestCompletionCallback cb;
  feature_->ProcessMessage(std::move(message), cb.callback());

  EXPECT_EQ(net::ERR_UNEXPECTED, cb.WaitForResult());
}

TEST_F(GeolocationFeatureTest, RequestRefreshReceived) {
  EXPECT_CALL(*location_provider_, OnPermissionGranted());

  GeolocationMessage* geolocation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&geolocation_message);
  geolocation_message->mutable_request_refresh();

  net::TestCompletionCallback cb;
  feature_->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

TEST_F(GeolocationFeatureTest, LocationUpdateSendsCorrectMessage) {
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualsDefaultGeoposition(), _));
  callback_.Run(location_provider_, position_);
}

TEST_F(GeolocationFeatureTest, ErrorUpdateSendsCorrectMessage) {
  ON_CALL(*out_processor_, MockableProcessMessage(_, _))
      .WillByDefault(Invoke(
          this, &GeolocationFeatureTest_ErrorUpdateSendsCorrectMessage_Test::
                    ReportProcessMessageSuccess));
  EXPECT_CALL(
      *out_processor_,
      MockableProcessMessage(
          EqualsError(GeolocationErrorMessage::POSITION_UNAVAILABLE), _));
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsError(GeolocationErrorMessage::PERMISSION_DENIED), _));
  EXPECT_CALL(
      *out_processor_,
      MockableProcessMessage(EqualsError(GeolocationErrorMessage::TIMEOUT), _));

  device::Geoposition err_position;
  err_position.error_code =
      device::Geoposition::ErrorCode::ERROR_CODE_POSITION_UNAVAILABLE;
  callback_.Run(location_provider_, err_position);

  err_position.error_code =
      device::Geoposition::ErrorCode::ERROR_CODE_PERMISSION_DENIED;
  callback_.Run(location_provider_, err_position);

  err_position.error_code = device::Geoposition::ErrorCode::ERROR_CODE_TIMEOUT;
  callback_.Run(location_provider_, err_position);
}

TEST_F(GeolocationFeatureTest, NoRepeatSendsWithMessagePending) {
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualsDefaultGeoposition(), _));
  callback_.Run(location_provider_, position_);
  callback_.Run(location_provider_, position_);
  callback_.Run(location_provider_, position_);
}

TEST_F(GeolocationFeatureTest, MessageSendsAfterAcknowledgement) {
  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualsDefaultGeoposition(), _))
      .WillOnce(Invoke(
          this, &GeolocationFeatureTest_MessageSendsAfterAcknowledgement_Test::
                    ReportProcessMessageSuccess));

  device::Geoposition position;
  position.latitude = 1.0;
  position.longitude = 1.0;
  position.altitude = 1.0;
  position.accuracy = 1.0;
  position.timestamp = base::Time::Now();

  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(EqualGeoposition(1.0, 1.0, 1.0, 1.0), _));
  callback_.Run(location_provider_, position_);
  callback_.Run(location_provider_, position);
}

TEST_F(GeolocationFeatureTest, ProcessMessageHandlesNullCallback) {
  EXPECT_CALL(*location_provider_, OnPermissionGranted());

  GeolocationMessage* geolocation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&geolocation_message);
  geolocation_message->mutable_request_refresh();

  feature_->ProcessMessage(std::move(message), net::CompletionCallback());
}

}  // namespace client
}  // namespace blimp
