// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_gpu_test_helper.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ipc/message_filter.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

namespace {

const int kGpuProcessHostId = 1;

void DispatchToGpuPlatformSupportHostTask(IPC::Message* msg) {
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnMessageReceived(*msg);
  delete msg;
}

void DispatchToGpuPlatformSupportTask(IPC::Message* msg) {
  ui::OzonePlatform::GetInstance()->GetGpuPlatformSupport()->OnMessageReceived(
      *msg);
  delete msg;
}

void DispatchToGpuPlatformSupportTaskOnIO(
    const scoped_refptr<base::SingleThreadTaskRunner>& gpu_task_runner,
    IPC::Message* msg) {
  IPC::MessageFilter* filter = ui::OzonePlatform::GetInstance()
                                   ->GetGpuPlatformSupport()
                                   ->GetMessageFilter();
  if (filter && filter->OnMessageReceived(*msg)) {
    delete msg;
    return;
  }

  gpu_task_runner->PostTask(FROM_HERE,
                            base::Bind(DispatchToGpuPlatformSupportTask, msg));
}

}  // namespace

class FakeGpuProcess : public IPC::Sender {
 public:
  FakeGpuProcess(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : ui_task_runner_(ui_task_runner) {}
  ~FakeGpuProcess() override {}

  void Init() {
    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupport()
        ->OnChannelEstablished(this);
  }

  void InitOnIO() {
    IPC::MessageFilter* filter = ui::OzonePlatform::GetInstance()
                                     ->GetGpuPlatformSupport()
                                     ->GetMessageFilter();

    if (filter)
      filter->OnFilterAdded(this);
  }

  bool Send(IPC::Message* msg) override {
    ui_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DispatchToGpuPlatformSupportHostTask, msg));
    return true;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

class FakeGpuProcessHost {
 public:
  FakeGpuProcessHost(
      const scoped_refptr<base::SingleThreadTaskRunner>& gpu_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& gpu_io_task_runner)
      : gpu_task_runner_(gpu_task_runner),
        gpu_io_task_runner_(gpu_io_task_runner) {}
  ~FakeGpuProcessHost() {}

  void Init() {
    base::Callback<void(IPC::Message*)> sender =
        base::Bind(&DispatchToGpuPlatformSupportTaskOnIO, gpu_task_runner_);

    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupportHost()
        ->OnChannelEstablished(kGpuProcessHostId, gpu_io_task_runner_, sender);
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_io_task_runner_;
};

OzoneGpuTestHelper::OzoneGpuTestHelper() {
}

OzoneGpuTestHelper::~OzoneGpuTestHelper() {
}

bool OzoneGpuTestHelper::Initialize(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& gpu_task_runner) {
  io_helper_thread_.reset(new base::Thread("IOHelperThread"));
  if (!io_helper_thread_->StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0)))
    return false;

  fake_gpu_process_.reset(new FakeGpuProcess(ui_task_runner));
  io_helper_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&FakeGpuProcess::InitOnIO,
                            base::Unretained(fake_gpu_process_.get())));
  fake_gpu_process_->Init();

  fake_gpu_process_host_.reset(new FakeGpuProcessHost(
      gpu_task_runner, io_helper_thread_->task_runner()));
  fake_gpu_process_host_->Init();

  return true;
}

}  // namespace ui
