// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"

#include <stddef.h>

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;

namespace {
// TODO(ortuno): Once we have a chooser for scanning and a way to control that
// chooser from tests we should delete this constant.
// https://crbug.com/436280
enum { kTestingScanDuration = 0 };  // No need to wait when testing.
enum { kScanDuration = 10 };
}  // namespace

namespace content {

BluetoothAdapterFactoryWrapper::BluetoothAdapterFactoryWrapper()
    : scan_duration_(base::TimeDelta::FromSecondsD(kScanDuration)),
      testing_(false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BluetoothAdapterFactoryWrapper::~BluetoothAdapterFactoryWrapper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // All observers should have been removed already.
  DCHECK(adapter_observers_.empty());
  // Clear adapter.
  set_adapter(scoped_refptr<device::BluetoothAdapter>());
}

bool BluetoothAdapterFactoryWrapper::IsBluetoothAdapterAvailable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BluetoothAdapterFactory::IsBluetoothAdapterAvailable() || testing_;
}

void BluetoothAdapterFactoryWrapper::AcquireAdapter(
    device::BluetoothAdapter::Observer* observer,
    const AcquireAdapterCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!GetAdapter(observer));

  AddAdapterObserver(observer);
  if (adapter_.get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, base::Unretained(adapter_.get())));
    return;
  }

  DCHECK(BluetoothAdapterFactory::IsBluetoothAdapterAvailable());
  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothAdapterFactoryWrapper::OnGetAdapter,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BluetoothAdapterFactoryWrapper::ReleaseAdapter(
    device::BluetoothAdapter::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!HasAdapter(observer)) {
    return;
  }
  RemoveAdapterObserver(observer);
  if (adapter_observers_.empty())
    set_adapter(scoped_refptr<device::BluetoothAdapter>());
}

BluetoothAdapter* BluetoothAdapterFactoryWrapper::GetAdapter(
    device::BluetoothAdapter::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (HasAdapter(observer)) {
    return adapter_.get();
  }
  return nullptr;
}

void BluetoothAdapterFactoryWrapper::SetBluetoothAdapterForTesting(
    scoped_refptr<device::BluetoothAdapter> mock_adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scan_duration_ = base::TimeDelta::FromSecondsD(kTestingScanDuration);
  testing_ = true;
  set_adapter(std::move(mock_adapter));
}

void BluetoothAdapterFactoryWrapper::OnGetAdapter(
    const AcquireAdapterCallback& continuation,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  set_adapter(adapter);
  continuation.Run(adapter_.get());
}

bool BluetoothAdapterFactoryWrapper::HasAdapter(
    device::BluetoothAdapter::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ContainsKey(adapter_observers_, observer);
}

void BluetoothAdapterFactoryWrapper::AddAdapterObserver(
    device::BluetoothAdapter::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = adapter_observers_.insert(observer);
  DCHECK(iter.second);
  if (adapter_) {
    adapter_->AddObserver(observer);
  }
}

void BluetoothAdapterFactoryWrapper::RemoveAdapterObserver(
    device::BluetoothAdapter::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t removed = adapter_observers_.erase(observer);
  DCHECK(removed);
  if (adapter_) {
    adapter_->RemoveObserver(observer);
  }
}

void BluetoothAdapterFactoryWrapper::set_adapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (adapter_.get()) {
    for (device::BluetoothAdapter::Observer* observer : adapter_observers_) {
      adapter_->RemoveObserver(observer);
    }
  }
  adapter_ = adapter;
  if (adapter_.get()) {
    for (device::BluetoothAdapter::Observer* observer : adapter_observers_) {
      adapter_->AddObserver(observer);
    }
  }
}

}  // namespace content
