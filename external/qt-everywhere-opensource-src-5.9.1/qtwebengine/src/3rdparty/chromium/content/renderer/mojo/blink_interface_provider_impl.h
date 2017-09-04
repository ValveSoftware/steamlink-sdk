// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BLINK_INTERFACE_PROVIDER_IMPL_H_
#define CONTENT_RENDERER_MOJO_BLINK_INTERFACE_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/public/platform/InterfaceProvider.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class InterfaceProvider;
}

namespace content {

// An implementation of blink::InterfaceProvider that forwards to a
// service_manager::InterfaceProvider.
class BlinkInterfaceProviderImpl : public blink::InterfaceProvider {
 public:
  explicit BlinkInterfaceProviderImpl(
      base::WeakPtr<service_manager::InterfaceProvider> remote_interfaces);
  ~BlinkInterfaceProviderImpl();

  // blink::InterfaceProvider override.
  void getInterface(const char* name,
                    mojo::ScopedMessagePipeHandle handle) override;

 private:
  const base::WeakPtr<service_manager::InterfaceProvider> remote_interfaces_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Should only be accessed by Web Worker threads that are using the
  // blink::Platform-level interface provider.
  base::WeakPtr<BlinkInterfaceProviderImpl> weak_ptr_;
  base::WeakPtrFactory<BlinkInterfaceProviderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlinkInterfaceProviderImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BLINK_INTERFACE_PROVIDER_IMPL_H_
