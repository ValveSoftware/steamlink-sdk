// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_provider_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/geolocation/location_arbitrator_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

GeolocationProvider* GeolocationProvider::GetInstance() {
  return GeolocationProviderImpl::GetInstance();
}

scoped_ptr<GeolocationProvider::Subscription>
GeolocationProviderImpl::AddLocationUpdateCallback(
    const LocationUpdateCallback& callback, bool use_high_accuracy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<GeolocationProvider::Subscription> subscription;
  if (use_high_accuracy) {
    subscription = high_accuracy_callbacks_.Add(callback);
  } else {
    subscription = low_accuracy_callbacks_.Add(callback);
  }

  OnClientsChanged();
  if (position_.Validate() ||
      position_.error_code != Geoposition::ERROR_CODE_NONE) {
    callback.Run(position_);
  }

  return subscription.Pass();
}

void GeolocationProviderImpl::UserDidOptIntoLocationServices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool was_permission_granted = user_did_opt_into_location_services_;
  user_did_opt_into_location_services_ = true;
  if (IsRunning() && !was_permission_granted)
    InformProvidersPermissionGranted();
}

void GeolocationProviderImpl::OverrideLocationForTesting(
    const Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ignore_location_updates_ = true;
  NotifyClients(position);
}

void GeolocationProviderImpl::OnLocationUpdate(const Geoposition& position) {
  DCHECK(OnGeolocationThread());
  // Will be true only in testing.
  if (ignore_location_updates_)
    return;
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&GeolocationProviderImpl::NotifyClients,
                                     base::Unretained(this), position));
}

GeolocationProviderImpl* GeolocationProviderImpl::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<GeolocationProviderImpl>::get();
}

GeolocationProviderImpl::GeolocationProviderImpl()
    : base::Thread("Geolocation"),
      user_did_opt_into_location_services_(false),
      ignore_location_updates_(false),
      arbitrator_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  high_accuracy_callbacks_.set_removal_callback(
      base::Bind(&GeolocationProviderImpl::OnClientsChanged,
                 base::Unretained(this)));
  low_accuracy_callbacks_.set_removal_callback(
      base::Bind(&GeolocationProviderImpl::OnClientsChanged,
                 base::Unretained(this)));
}

GeolocationProviderImpl::~GeolocationProviderImpl() {
  Stop();
  DCHECK(!arbitrator_);
}

bool GeolocationProviderImpl::OnGeolocationThread() const {
  return base::MessageLoop::current() == message_loop();
}

void GeolocationProviderImpl::OnClientsChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
    bool use_high_accuracy = !high_accuracy_callbacks_.empty();

    // Send the current options to the providers as they may have changed.
    task = base::Bind(&GeolocationProviderImpl::StartProviders,
                      base::Unretained(this),
                      use_high_accuracy);
  }

  message_loop()->PostTask(FROM_HERE, task);
}

void GeolocationProviderImpl::StopProviders() {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StopProviders();
}

void GeolocationProviderImpl::StartProviders(bool use_high_accuracy) {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StartProviders(use_high_accuracy);
}

void GeolocationProviderImpl::InformProvidersPermissionGranted() {
  DCHECK(IsRunning());
  if (!OnGeolocationThread()) {
    message_loop()->PostTask(
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(position.Validate() ||
         position.error_code != Geoposition::ERROR_CODE_NONE);
  position_ = position;
  high_accuracy_callbacks_.Notify(position_);
  low_accuracy_callbacks_.Notify(position_);
}

void GeolocationProviderImpl::Init() {
  DCHECK(OnGeolocationThread());
  DCHECK(!arbitrator_);
  arbitrator_ = CreateArbitrator();
}

void GeolocationProviderImpl::CleanUp() {
  DCHECK(OnGeolocationThread());
  delete arbitrator_;
  arbitrator_ = NULL;
}

LocationArbitrator* GeolocationProviderImpl::CreateArbitrator() {
  LocationArbitratorImpl::LocationUpdateCallback callback = base::Bind(
      &GeolocationProviderImpl::OnLocationUpdate, base::Unretained(this));
  return new LocationArbitratorImpl(callback);
}

}  // namespace content
