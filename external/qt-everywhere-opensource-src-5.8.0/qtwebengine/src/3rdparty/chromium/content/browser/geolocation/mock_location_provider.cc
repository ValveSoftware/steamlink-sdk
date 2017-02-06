// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a mock location provider and the factory functions for
// various ways of creating it.

#include "content/browser/geolocation/mock_location_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

MockLocationProvider::MockLocationProvider()
    : state_(STOPPED),
      is_permission_granted_(false),
      provider_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

MockLocationProvider::~MockLocationProvider() {}

void MockLocationProvider::HandlePositionChanged(const Geoposition& position) {
  if (provider_task_runner_->BelongsToCurrentThread()) {
    // The location arbitrator unit tests rely on this method running
    // synchronously.
    position_ = position;
    NotifyCallback(position_);
  } else {
    provider_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MockLocationProvider::HandlePositionChanged,
                              base::Unretained(this), position));
  }
}

bool MockLocationProvider::StartProvider(bool high_accuracy) {
  state_ = high_accuracy ? HIGH_ACCURACY : LOW_ACCURACY;
  return true;
}

void MockLocationProvider::StopProvider() {
  state_ = STOPPED;
}

void MockLocationProvider::GetPosition(Geoposition* position) {
  *position = position_;
}

void MockLocationProvider::OnPermissionGranted() {
  is_permission_granted_ = true;
}

// Mock location provider that automatically calls back its client at most
// once, when StartProvider or OnPermissionGranted is called. Use
// |requires_permission_to_start| to select which event triggers the callback.
class AutoMockLocationProvider : public MockLocationProvider {
 public:
  AutoMockLocationProvider(bool has_valid_location,
                           bool requires_permission_to_start)
      : requires_permission_to_start_(requires_permission_to_start),
        listeners_updated_(false) {
    if (has_valid_location) {
      position_.accuracy = 3;
      position_.latitude = 4.3;
      position_.longitude = -7.8;
      // Webkit compares the timestamp to wall clock time, so we need it to be
      // contemporary.
      position_.timestamp = base::Time::Now();
    } else {
      position_.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    }
  }
  bool StartProvider(bool high_accuracy) override {
    MockLocationProvider::StartProvider(high_accuracy);
    if (!requires_permission_to_start_) {
      UpdateListenersIfNeeded();
    }
    return true;
  }

  void OnPermissionGranted() override {
    MockLocationProvider::OnPermissionGranted();
    if (requires_permission_to_start_) {
      UpdateListenersIfNeeded();
    }
  }

  void UpdateListenersIfNeeded() {
    if (!listeners_updated_) {
      listeners_updated_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&MockLocationProvider::HandlePositionChanged,
                                base::Unretained(this), position_));
    }
  }

 private:
  const bool requires_permission_to_start_;
  bool listeners_updated_;

  DISALLOW_COPY_AND_ASSIGN(AutoMockLocationProvider);
};

LocationProvider* NewMockLocationProvider() {
  return new MockLocationProvider;
}

LocationProvider* NewAutoSuccessMockLocationProvider() {
  return new AutoMockLocationProvider(true, false);
}

LocationProvider* NewAutoFailMockLocationProvider() {
  return new AutoMockLocationProvider(false, false);
}

LocationProvider* NewAutoSuccessMockNetworkLocationProvider() {
  return new AutoMockLocationProvider(true, true);
}

}  // namespace content
