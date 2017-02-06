// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/wake_lock/wake_lock_service.mojom.h"

namespace content {

class WakeLockServiceContext;

class WakeLockServiceImpl : public blink::mojom::WakeLockService {
 public:
  WakeLockServiceImpl(
      base::WeakPtr<WakeLockServiceContext> context,
      int render_process_id,
      int render_frame_id,
      mojo::InterfaceRequest<blink::mojom::WakeLockService> request);
  ~WakeLockServiceImpl() override;

  // WakeLockSevice implementation.
  void RequestWakeLock() override;
  void CancelWakeLock() override;

 private:
  base::WeakPtr<WakeLockServiceContext> context_;
  const int render_process_id_;
  const int render_frame_id_;
  mojo::StrongBinding<blink::mojom::WakeLockService> binding_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
