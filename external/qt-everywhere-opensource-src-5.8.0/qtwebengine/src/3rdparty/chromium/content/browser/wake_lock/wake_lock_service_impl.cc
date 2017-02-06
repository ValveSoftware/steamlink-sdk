// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_impl.h"

#include <utility>

#include "content/browser/wake_lock/wake_lock_service_context.h"

namespace content {

WakeLockServiceImpl::WakeLockServiceImpl(
    base::WeakPtr<WakeLockServiceContext> context,
    int render_process_id,
    int render_frame_id,
    mojo::InterfaceRequest<blink::mojom::WakeLockService> request)
    : context_(context),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      binding_(this, std::move(request)) {}

WakeLockServiceImpl::~WakeLockServiceImpl() {}

void WakeLockServiceImpl::RequestWakeLock() {
  if (context_)
    context_->RequestWakeLock(render_process_id_, render_frame_id_);
}

void WakeLockServiceImpl::CancelWakeLock() {
  if (context_)
    context_->CancelWakeLock(render_process_id_, render_frame_id_);
}

}  // namespace content
