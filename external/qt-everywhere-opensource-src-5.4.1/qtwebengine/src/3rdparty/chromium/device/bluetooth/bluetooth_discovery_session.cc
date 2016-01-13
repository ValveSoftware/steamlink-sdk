// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_discovery_session.h"

#include "base/bind.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothDiscoverySession::BluetoothDiscoverySession(
    scoped_refptr<BluetoothAdapter> adapter)
    : active_(true), adapter_(adapter), weak_ptr_factory_(this) {
  DCHECK(adapter_.get());
}

BluetoothDiscoverySession::BluetoothDiscoverySession()
    : active_(false), weak_ptr_factory_(this) {}

BluetoothDiscoverySession::~BluetoothDiscoverySession() {
  // |adapter_| may be NULL if this instance was initialized as a mock.
  if (!adapter_.get()) {
    DCHECK(!active_);
    return;
  }
  Stop(base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));
  MarkAsInactive();
}

bool BluetoothDiscoverySession::IsActive() const {
  return active_;
}

void BluetoothDiscoverySession::Stop(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!active_) {
    LOG(WARNING) << "Discovery session not active. Cannot stop.";
    error_callback.Run();
    return;
  }
  VLOG(1) << "Stopping device discovery session.";
  DCHECK(adapter_.get());
  adapter_->RemoveDiscoverySession(
      base::Bind(&BluetoothDiscoverySession::OnStop,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      error_callback);
}

void BluetoothDiscoverySession::OnStop(const base::Closure& callback) {
  MarkAsInactive();
  callback.Run();
}

void BluetoothDiscoverySession::MarkAsInactive() {
  if (!active_)
    return;
  active_ = false;
  DCHECK(adapter_.get());
  adapter_->DiscoverySessionBecameInactive(this);
}

}  // namespace device
