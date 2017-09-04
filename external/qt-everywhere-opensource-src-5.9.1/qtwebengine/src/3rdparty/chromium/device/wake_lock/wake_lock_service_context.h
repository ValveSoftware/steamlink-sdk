// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
#define DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_

#include <memory>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "device/wake_lock/wake_lock_service_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

class PowerSaveBlocker;

class WakeLockServiceContext {
 public:
  WakeLockServiceContext(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      base::Callback<gfx::NativeView()> native_view_getter);
  ~WakeLockServiceContext();

  // Creates a WakeLockServiceImpl that is strongly bound to |request|.
  void CreateService(mojo::InterfaceRequest<mojom::WakeLockService> request);

  // Requests wake lock.
  void RequestWakeLock();

  // Cancels pending wake lock request.
  void CancelWakeLock();

  // Used by tests.
  bool HasWakeLockForTests() const;

 private:
  void CreateWakeLock();
  void RemoveWakeLock();
  void UpdateWakeLock();

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  int num_lock_requests_;

  // The actual power save blocker for screen.
  std::unique_ptr<PowerSaveBlocker> wake_lock_;
  base::Callback<gfx::NativeView()> native_view_getter_;

  base::WeakPtrFactory<WakeLockServiceContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceContext);
};

}  // namespace device

#endif  // DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
