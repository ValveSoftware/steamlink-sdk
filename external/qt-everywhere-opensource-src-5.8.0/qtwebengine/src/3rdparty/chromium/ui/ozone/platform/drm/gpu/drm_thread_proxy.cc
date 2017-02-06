// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"

namespace ui {

DrmThreadProxy::DrmThreadProxy() {}

DrmThreadProxy::~DrmThreadProxy() {}

void DrmThreadProxy::BindThreadIntoMessagingProxy(
    InterThreadMessagingProxy* messaging_proxy) {
  messaging_proxy->SetDrmThread(&drm_thread_);
}

std::unique_ptr<DrmWindowProxy> DrmThreadProxy::CreateDrmWindowProxy(
    gfx::AcceleratedWidget widget) {
  return base::WrapUnique(new DrmWindowProxy(widget, &drm_thread_));
}

scoped_refptr<GbmBuffer> DrmThreadProxy::CreateBuffer(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  scoped_refptr<GbmBuffer> buffer;
  PostSyncTask(
      drm_thread_.task_runner(),
      base::Bind(&DrmThread::CreateBuffer, base::Unretained(&drm_thread_),
                 widget, size, format, usage, &buffer));
  return buffer;
}

scoped_refptr<GbmBuffer> DrmThreadProxy::CreateBufferFromFds(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    std::vector<base::ScopedFD>&& fds,
    std::vector<int> strides,
    std::vector<int> offsets) {
  scoped_refptr<GbmBuffer> buffer;
  PostSyncTask(
      drm_thread_.task_runner(),
      base::Bind(&DrmThread::CreateBufferFromFds,
                 base::Unretained(&drm_thread_), widget, size, format,
                 base::Passed(std::move(fds)), strides, offsets, &buffer));
  return buffer;
}

void DrmThreadProxy::GetScanoutFormats(
    gfx::AcceleratedWidget widget,
    std::vector<gfx::BufferFormat>* scanout_formats) {
  PostSyncTask(
      drm_thread_.task_runner(),
      base::Bind(&DrmThread::GetScanoutFormats, base::Unretained(&drm_thread_),
                 widget, scanout_formats));
}

}  // namespace ui
