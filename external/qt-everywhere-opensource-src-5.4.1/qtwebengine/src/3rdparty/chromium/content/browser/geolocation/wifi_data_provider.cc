// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/wifi_data_provider.h"

namespace content {

// static
WifiDataProvider* WifiDataProvider::instance_ = NULL;

// static
WifiDataProvider::ImplFactoryFunction WifiDataProvider::factory_function_ =
    DefaultFactoryFunction;

// static
void WifiDataProvider::SetFactory(ImplFactoryFunction factory_function_in) {
  factory_function_ = factory_function_in;
}

// static
void WifiDataProvider::ResetFactory() {
  factory_function_ = DefaultFactoryFunction;
}

// static
WifiDataProvider* WifiDataProvider::Register(WifiDataUpdateCallback* callback) {
  bool need_to_start_data_provider = false;
  if (!instance_) {
    instance_ = new WifiDataProvider();
    need_to_start_data_provider = true;
  }
  DCHECK(instance_);
  instance_->AddCallback(callback);
  // Start the provider after adding the callback, to avoid any race in
  // it running early.
  if (need_to_start_data_provider)
    instance_->StartDataProvider();
  return instance_;
}

// static
bool WifiDataProvider::Unregister(WifiDataUpdateCallback* callback) {
  DCHECK(instance_);
  DCHECK(instance_->has_callbacks());
  if (!instance_->RemoveCallback(callback)) {
    return false;
  }
  if (!instance_->has_callbacks()) {
    // Must stop the data provider (and any implementation threads) before
    // destroying to avoid any race conditions in access to the provider in
    // the destructor chain.
    instance_->StopDataProvider();
    delete instance_;
    instance_ = NULL;
  }
  return true;
}

WifiDataProviderImplBase::WifiDataProviderImplBase()
    : container_(NULL),
      client_loop_(base::MessageLoop::current()) {
  DCHECK(client_loop_);
}

WifiDataProviderImplBase::~WifiDataProviderImplBase() {
}

void WifiDataProviderImplBase::SetContainer(WifiDataProvider* container) {
  container_ = container;
}

void WifiDataProviderImplBase::AddCallback(WifiDataUpdateCallback* callback) {
  callbacks_.insert(callback);
}

bool WifiDataProviderImplBase::RemoveCallback(
    WifiDataUpdateCallback* callback) {
  return callbacks_.erase(callback) == 1;
}

bool WifiDataProviderImplBase::has_callbacks() const {
  return !callbacks_.empty();
}

void WifiDataProviderImplBase::RunCallbacks() {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &WifiDataProviderImplBase::DoRunCallbacks,
      this));
}

bool WifiDataProviderImplBase::CalledOnClientThread() const {
  return base::MessageLoop::current() == this->client_loop_;
}

base::MessageLoop* WifiDataProviderImplBase::client_loop() const {
  return client_loop_;
}

void WifiDataProviderImplBase::DoRunCallbacks() {
  // It's possible that all the callbacks (and the container) went away
  // whilst this task was pending. This is fine; the loop will be a no-op.
  CallbackSet::const_iterator iter = callbacks_.begin();
  while (iter != callbacks_.end()) {
    WifiDataUpdateCallback* callback = *iter;
    ++iter;  // Advance iter before running, in case callback unregisters.
    callback->Run(container_);
  }
}

WifiDataProvider::WifiDataProvider() {
  DCHECK(factory_function_);
  impl_ = (*factory_function_)();
  DCHECK(impl_.get());
  impl_->SetContainer(this);
}

WifiDataProvider::~WifiDataProvider() {
  DCHECK(impl_.get());
  impl_->SetContainer(NULL);
}

bool WifiDataProvider::GetData(WifiData* data) {
  return impl_->GetData(data);
}

void WifiDataProvider::AddCallback(WifiDataUpdateCallback* callback) {
  impl_->AddCallback(callback);
}

bool WifiDataProvider::RemoveCallback(WifiDataUpdateCallback* callback) {
  return impl_->RemoveCallback(callback);
}

bool WifiDataProvider::has_callbacks() const {
  return impl_->has_callbacks();
}

void WifiDataProvider::StartDataProvider() {
  impl_->StartDataProvider();
}

void WifiDataProvider::StopDataProvider() {
  impl_->StopDataProvider();
}

}  // namespace content
