// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CALLBACK_BINDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CALLBACK_BINDER_H_

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_binder.h"

namespace service_manager {
namespace internal {

template <typename Interface>
class CallbackBinder : public InterfaceBinder {
 public:
  // Method that binds a request for Interface.
  using BindCallback =
      base::Callback<void(mojo::InterfaceRequest<Interface>)>;

  explicit CallbackBinder(
      const BindCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}
  ~CallbackBinder() override {}

 private:
  // InterfaceBinder:
  void BindInterface(
      const Identity& remote_identity,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle handle) override {
    mojo::InterfaceRequest<Interface> request =
        mojo::MakeRequest<Interface>(std::move(handle));
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&CallbackBinder::RunCallbackOnTaskRunner, callback_,
                     base::Passed(&request)));
      return;
    }
    callback_.Run(std::move(request));
  }

  static void RunCallbackOnTaskRunner(
      const BindCallback& callback,
      mojo::InterfaceRequest<Interface> client) {
    callback.Run(std::move(client));
  }

  const BindCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(CallbackBinder);
};

class GenericCallbackBinder : public InterfaceBinder {
 public:
  using BindCallback = base::Callback<void(mojo::ScopedMessagePipeHandle)>;

  explicit GenericCallbackBinder(
      const BindCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~GenericCallbackBinder() override;

 private:
  // InterfaceBinder:
  void BindInterface(const service_manager::Identity& remote_identity,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle) override;

  static void RunCallbackOnTaskRunner(
      const BindCallback& callback,
      mojo::ScopedMessagePipeHandle client_handle);

  const BindCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(GenericCallbackBinder);
};

}  // namespace internal
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CALLBACK_BINDER_H_
