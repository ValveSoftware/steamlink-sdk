// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_interface_provider_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

BlinkInterfaceProviderImpl::BlinkInterfaceProviderImpl(
    base::WeakPtr<service_manager::InterfaceProvider> remote_interfaces)
    : remote_interfaces_(remote_interfaces),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

BlinkInterfaceProviderImpl::~BlinkInterfaceProviderImpl() = default;

void BlinkInterfaceProviderImpl::getInterface(
    const char* name,
    mojo::ScopedMessagePipeHandle handle) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BlinkInterfaceProviderImpl::getInterface,
                              weak_ptr_, name, base::Passed(&handle)));
    return;
  }

  if (!remote_interfaces_)
    return;

  remote_interfaces_->GetInterface(name, std::move(handle));
}

}  // namespace content
