// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/blimp_location_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/geolocation/geoposition.h"

namespace blimp {
namespace engine {
namespace {

// Called on the delegate thread, this function posts the results
// of the geolocation update back to the main blimp thread.
void InvokeGeopositionCallback(
    scoped_refptr<base::TaskRunner> geolocation_task_runner,
    const base::Callback<void(const device::Geoposition&)>& callback,
    const device::Geoposition& geoposition) {
  geolocation_task_runner->PostTask(FROM_HERE,
                                    base::Bind(callback, geoposition));
}

}  // namespace

BlimpLocationProvider::BlimpLocationProvider(
    base::WeakPtr<BlimpLocationProvider::Delegate> delegate,
    scoped_refptr<base::SequencedTaskRunner> delegate_task_runner)
    : delegate_(delegate),
      delegate_task_runner_(delegate_task_runner),
      is_started_(false),
      weak_factory_(this) {}

BlimpLocationProvider::~BlimpLocationProvider() {
  if (is_started_) {
    StopProvider();
  }
}

bool BlimpLocationProvider::StartProvider(bool high_accuracy) {
  is_started_ = true;
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &BlimpLocationProvider::Delegate::RequestAccuracy, delegate_,
          (high_accuracy ? GeolocationSetInterestLevelMessage::HIGH_ACCURACY
                         : GeolocationSetInterestLevelMessage::LOW_ACCURACY)));
  return is_started_;
}

void BlimpLocationProvider::StopProvider() {
  DCHECK(is_started_);
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BlimpLocationProvider::Delegate::RequestAccuracy, delegate_,
                 GeolocationSetInterestLevelMessage::NO_INTEREST));
  is_started_ = false;
}

const device::Geoposition& BlimpLocationProvider::GetPosition() {
  return cached_position_;
}

void BlimpLocationProvider::OnPermissionGranted() {
  DCHECK(is_started_);
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BlimpLocationProvider::Delegate::OnPermissionGranted,
                 delegate_));
}

void BlimpLocationProvider::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  // We post a SetUpdateCallback call to the delegate thread.
  // InvokeGeopositionCallback runs on the delegate thread on geoposition
  // update and then uses the task runner passed to it to post the results back
  // to the blimp thread.
  location_update_callback_ = callback;
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BlimpLocationProvider::Delegate::SetUpdateCallback, delegate_,
                 base::Bind(&InvokeGeopositionCallback,
                            base::ThreadTaskRunnerHandle::Get(),
                            base::Bind(&BlimpLocationProvider::OnLocationUpdate,
                                       weak_factory_.GetWeakPtr()))));
}

void BlimpLocationProvider::OnLocationUpdate(
    const device::Geoposition& geoposition) {
  location_update_callback_.Run(this, geoposition);
}

}  // namespace engine
}  // namespace blimp
