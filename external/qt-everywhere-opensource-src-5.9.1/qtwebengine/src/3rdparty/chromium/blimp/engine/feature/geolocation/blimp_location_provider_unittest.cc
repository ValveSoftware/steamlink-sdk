// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/blimp_location_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/engine/feature/geolocation/mock_blimp_location_provider_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::SaveArg;
using testing::_;

namespace blimp {
namespace engine {

class BlimpLocationProviderTest : public testing::Test {
 public:
  BlimpLocationProviderTest()
      : mock_callback_(base::Bind(&BlimpLocationProviderTest::OnLocationUpdate,
                                  base::Unretained(this))),
        delegate_(base::WrapUnique(new MockBlimpLocationProviderDelegate)),
        location_provider_(base::MakeUnique<BlimpLocationProvider>(
            delegate_->GetWeakPtr(),
            base::ThreadTaskRunnerHandle::Get())) {}

  void SetUp() override { on_location_update_called_ = false; }

  void TearDown() override {
    location_provider_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnLocationUpdate(const device::LocationProvider* provider,
                        const device::Geoposition& geoposition) {
    on_location_update_called_ = true;
  }

 protected:
  const base::MessageLoop loop_;
  device::LocationProvider::LocationProviderUpdateCallback mock_callback_;
  std::unique_ptr<MockBlimpLocationProviderDelegate> delegate_;
  std::unique_ptr<BlimpLocationProvider> location_provider_;
  bool on_location_update_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpLocationProviderTest);
};

TEST_F(BlimpLocationProviderTest, StartProviderRunsCorrectly) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::LOW_ACCURACY))
      .Times(1);

  // BlimpLocationProvider implicitly stops on teardown, if it was started.
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST))
      .Times(1);

  EXPECT_TRUE(location_provider_->StartProvider(true));
  EXPECT_TRUE(location_provider_->StartProvider(false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, StartProviderHandlesNullDelegate) {
  EXPECT_CALL(*delegate_, RequestAccuracy(_)).Times(0);

  delegate_.reset();
  location_provider_->StartProvider(true);
  location_provider_->StartProvider(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, StopProviderRunsCorrectly) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST))
      .Times(1);

  location_provider_->StartProvider(true);
  location_provider_->StopProvider();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, StopProviderHandlesNullDelegate) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);

  location_provider_->StartProvider(true);
  base::RunLoop().RunUntilIdle();
  delegate_.reset();
  location_provider_->StopProvider();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, LocationProviderDeleted) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST))
      .Times(1);

  location_provider_->StartProvider(true);
  location_provider_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, OnPermissionGranted) {
  EXPECT_CALL(*delegate_, OnPermissionGranted()).Times(1);
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST))
      .Times(1);

  location_provider_->StartProvider(true);
  location_provider_->OnPermissionGranted();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, OnPermissionGrantedHandlesNullDelegate) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);

  location_provider_->StartProvider(true);
  base::RunLoop().RunUntilIdle();
  delegate_.reset();
  location_provider_->OnPermissionGranted();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, SetUpdateCallbackPropagatesCallback) {
  EXPECT_CALL(*delegate_, SetUpdateCallback(_));

  location_provider_->SetUpdateCallback(mock_callback_);
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlimpLocationProviderTest, NoCallbackWhenProviderDeleted) {
  base::Callback<void(const device::Geoposition&)> callback;
  EXPECT_CALL(*delegate_, SetUpdateCallback(_)).WillOnce(SaveArg<0>(&callback));
  location_provider_->SetUpdateCallback(mock_callback_);
  base::RunLoop().RunUntilIdle();

  location_provider_.reset();
  device::Geoposition position;
  callback.Run(position);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(on_location_update_called_);
}

TEST_F(BlimpLocationProviderTest, CallbackCalledProperly) {
  base::Callback<void(const device::Geoposition&)> callback;
  EXPECT_CALL(*delegate_, SetUpdateCallback(_)).WillOnce(SaveArg<0>(&callback));
  location_provider_->SetUpdateCallback(mock_callback_);
  base::RunLoop().RunUntilIdle();

  device::Geoposition position;
  callback.Run(position);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(on_location_update_called_);
}

TEST_F(BlimpLocationProviderTest, SetUpdateCallbackHandlesNullDelegate) {
  EXPECT_CALL(*delegate_, SetUpdateCallback(_)).Times(0);

  delegate_.reset();
  location_provider_->SetUpdateCallback(mock_callback_);
  base::RunLoop().RunUntilIdle();
}

}  // namespace engine
}  // namespace blimp
