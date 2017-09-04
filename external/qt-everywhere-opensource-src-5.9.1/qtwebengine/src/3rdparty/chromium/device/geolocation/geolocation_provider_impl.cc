// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geolocation_provider_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/location_arbitrator.h"

namespace device {

namespace {
base::LazyInstance<std::unique_ptr<GeolocationDelegate>>::Leaky g_delegate =
    LAZY_INSTANCE_INITIALIZER;
}  // anonymous namespace

// static
GeolocationProvider* GeolocationProvider::GetInstance() {
  return GeolocationProviderImpl::GetInstance();
}

// static
void GeolocationProvider::SetGeolocationDelegate(
    GeolocationDelegate* delegate) {
  DCHECK(!g_delegate.Get());
  g_delegate.Get().reset(delegate);
}

std::unique_ptr<GeolocationProvider::Subscription>
GeolocationProviderImpl::AddLocationUpdateCallback(
    const LocationUpdateCallback& callback,
    bool enable_high_accuracy) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<GeolocationProvider::Subscription> subscription;
  if (enable_high_accuracy) {
    subscription = high_accuracy_callbacks_.Add(callback);
  } else {
    subscription = low_accuracy_callbacks_.Add(callback);
  }

  OnClientsChanged();
  if (position_.Validate() ||
      position_.error_code != Geoposition::ERROR_CODE_NONE) {
    callback.Run(position_);
  }

  return subscription;
}

void GeolocationProviderImpl::UserDidOptIntoLocationServices() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  bool was_permission_granted = user_did_opt_into_location_services_;
  user_did_opt_into_location_services_ = true;
  if (IsRunning() && !was_permission_granted)
    InformProvidersPermissionGranted();
}

void GeolocationProviderImpl::OverrideLocationForTesting(
    const Geoposition& position) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  ignore_location_updates_ = true;
  NotifyClients(position);
}

void GeolocationProviderImpl::OnLocationUpdate(const LocationProvider* provider,
                                               const Geoposition& position) {
  DCHECK(OnGeolocationThread());
  // Will be true only in testing.
  if (ignore_location_updates_)
    return;
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GeolocationProviderImpl::NotifyClients,
                            base::Unretained(this), position));
}

// static
GeolocationProviderImpl* GeolocationProviderImpl::GetInstance() {
  return base::Singleton<GeolocationProviderImpl>::get();
}

GeolocationProviderImpl::GeolocationProviderImpl()
    : base::Thread("Geolocation"),
      user_did_opt_into_location_services_(false),
      ignore_location_updates_(false),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  high_accuracy_callbacks_.set_removal_callback(base::Bind(
      &GeolocationProviderImpl::OnClientsChanged, base::Unretained(this)));
  low_accuracy_callbacks_.set_removal_callback(base::Bind(
      &GeolocationProviderImpl::OnClientsChanged, base::Unretained(this)));
}

GeolocationProviderImpl::~GeolocationProviderImpl() {
  Stop();
  DCHECK(!arbitrator_);
}

void GeolocationProviderImpl::SetArbitratorForTesting(
    std::unique_ptr<LocationProvider> arbitrator) {
  arbitrator_ = std::move(arbitrator);
}

bool GeolocationProviderImpl::OnGeolocationThread() const {
  return task_runner()->BelongsToCurrentThread();
}

void GeolocationProviderImpl::OnClientsChanged() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::Closure task;
  if (high_accuracy_callbacks_.empty() && low_accuracy_callbacks_.empty()) {
    DCHECK(IsRunning());
    if (!ignore_location_updates_) {
      // We have no more observers, so we clear the cached geoposition so that
      // when the next observer is added we will not provide a stale position.
      position_ = Geoposition();
    }
    task = base::Bind(&GeolocationProviderImpl::StopProviders,
                      base::Unretained(this));
  } else {
    if (!IsRunning()) {
      Start();
      if (user_did_opt_into_location_services_)
        InformProvidersPermissionGranted();
    }
    // Determine a set of options that satisfies all clients.
    bool enable_high_accuracy = !high_accuracy_callbacks_.empty();

    // Send the current options to the providers as they may have changed.
    task = base::Bind(&GeolocationProviderImpl::StartProviders,
                      base::Unretained(this), enable_high_accuracy);
  }

  task_runner()->PostTask(FROM_HERE, task);
}

void GeolocationProviderImpl::StopProviders() {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StopProvider();
}

void GeolocationProviderImpl::StartProviders(bool enable_high_accuracy) {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StartProvider(enable_high_accuracy);
}

void GeolocationProviderImpl::InformProvidersPermissionGranted() {
  DCHECK(IsRunning());
  if (!OnGeolocationThread()) {
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&GeolocationProviderImpl::InformProvidersPermissionGranted,
                   base::Unretained(this)));
    return;
  }
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->OnPermissionGranted();
}

void GeolocationProviderImpl::NotifyClients(const Geoposition& position) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(position.Validate() ||
         position.error_code != Geoposition::ERROR_CODE_NONE);
  position_ = position;
  high_accuracy_callbacks_.Notify(position_);
  low_accuracy_callbacks_.Notify(position_);
}

void GeolocationProviderImpl::Init() {
  DCHECK(OnGeolocationThread());

  if (!arbitrator_) {
    LocationProvider::LocationProviderUpdateCallback callback = base::Bind(
        &GeolocationProviderImpl::OnLocationUpdate, base::Unretained(this));
    // Use the embedder's |g_delegate| or fall back to the default one.
    if (!g_delegate.Get())
      g_delegate.Get().reset(new GeolocationDelegate);

    arbitrator_ = base::MakeUnique<LocationArbitrator>(
        base::WrapUnique(g_delegate.Get().get()));
    arbitrator_->SetUpdateCallback(callback);
  }
}

void GeolocationProviderImpl::CleanUp() {
  DCHECK(OnGeolocationThread());
  arbitrator_.reset();
}

}  // namespace device
