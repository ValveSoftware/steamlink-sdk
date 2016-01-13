// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/in_process_gpu_thread.h"

#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"

namespace content {

InProcessGpuThread::InProcessGpuThread(const std::string& channel_id)
    : base::Thread("Chrome_InProcGpuThread"),
      channel_id_(channel_id),
      gpu_process_(NULL) {
}

InProcessGpuThread::~InProcessGpuThread() {
  Stop();
}

void InProcessGpuThread::Init() {
  gpu_process_ = new GpuProcess();
  // The process object takes ownership of the thread object, so do not
  // save and delete the pointer.
  gpu_process_->set_main_thread(new GpuChildThread(channel_id_));
}

void InProcessGpuThread::CleanUp() {
  SetThreadWasQuitProperly(true);
  delete gpu_process_;
}

base::Thread* CreateInProcessGpuThread(const std::string& channel_id) {
  return new InProcessGpuThread(channel_id);
}

}  // namespace content
