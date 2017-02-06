// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/lib/callback_binder.h"

namespace shell {
namespace internal {

GenericCallbackBinder::GenericCallbackBinder(
    const BindCallback& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : callback_(callback), task_runner_(task_runner) {}

GenericCallbackBinder::~GenericCallbackBinder() {}

void GenericCallbackBinder::BindInterface(
    Connection* connection,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  if (task_runner_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GenericCallbackBinder::RunCallbackOnTaskRunner, callback_,
                    base::Passed(&handle)));
    return;
  }
  callback_.Run(std::move(handle));
}

// static
void GenericCallbackBinder::RunCallbackOnTaskRunner(
    const BindCallback& callback,
    mojo::ScopedMessagePipeHandle client_handle) {
  callback.Run(std::move(client_handle));
}

}  // namespace internal
}  // namespace shell
