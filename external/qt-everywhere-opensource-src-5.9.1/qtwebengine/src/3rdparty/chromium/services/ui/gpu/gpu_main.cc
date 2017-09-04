// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_main.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "services/ui/gpu/gpu_service_internal.h"

namespace {

#if defined(OS_WIN)
std::unique_ptr<base::MessagePump> CreateMessagePumpWin() {
  base::MessagePumpForGpu::InitFactory();
  return base::MessageLoop::CreateMessagePumpForType(
      base::MessageLoop::TYPE_UI);
}
#endif  // defined(OS_WIN)

#if defined(USE_X11)
std::unique_ptr<base::MessagePump> CreateMessagePumpX11() {
  // TODO(sad): This should create a TYPE_UI message pump, and create a
  // PlatformEventSource when gpu process split happens.
  return base::MessageLoop::CreateMessagePumpForType(
      base::MessageLoop::TYPE_DEFAULT);
}
#endif  // defined(USE_X11)

#if defined(OS_MACOSX)
std::unique_ptr<base::MessagePump> CreateMessagePumpMac() {
  return base::MakeUnique<base::MessagePumpCFRunLoop>();
}
#endif  // defined(OS_MACOSX)

}  // namespace

namespace ui {

GpuMain::GpuMain()
    : gpu_thread_("GpuThread"), io_thread_("GpuIOThread"), weak_factory_(this) {
  base::Thread::Options thread_options;

#if defined(OS_WIN)
  thread_options.message_pump_factory = base::Bind(&CreateMessagePumpWin);
#elif defined(USE_X11)
  thread_options.message_pump_factory = base::Bind(&CreateMessagePumpX11);
#elif defined(USE_OZONE)
  thread_options.message_loop_type = base::MessageLoop::TYPE_UI;
#elif defined(OS_LINUX)
  thread_options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
#elif defined(OS_MACOSX)
  thread_options.message_pump_factory = base::Bind(&CreateMessagePumpMac);
#else
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
#endif

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(gpu_thread_.StartWithOptions(thread_options));

  // TODO(sad): We do not need the IO thread once gpu has a separate process. It
  // should be possible to use |main_task_runner_| for doing IO tasks.
  thread_options = base::Thread::Options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // TODO(reveman): Remove this in favor of setting it explicitly for each type
  // of process.
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(io_thread_.StartWithOptions(thread_options));
}

GpuMain::~GpuMain() {
  // Unretained() is OK here since the thread/task runner is owned by |this|.
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::TearDownOnGpuThread, base::Unretained(this)));
  gpu_thread_.Stop();
  io_thread_.Stop();
}

void GpuMain::OnStart() {
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::InitOnGpuThread, weak_factory_.GetWeakPtr()));
}

void GpuMain::Create(mojom::GpuServiceInternalRequest request) {
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::CreateOnGpuThread, weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(request))));
}

void GpuMain::InitOnGpuThread() {
  gpu_init_.reset(new gpu::GpuInit());
  gpu_init_->set_sandbox_helper(this);
  bool success = gpu_init_->InitializeAndStartSandbox(
      *base::CommandLine::ForCurrentProcess());
  if (success) {
    if (gpu::GetNativeGpuMemoryBufferType() != gfx::EMPTY_BUFFER) {
      gpu_memory_buffer_factory_ =
          gpu::GpuMemoryBufferFactory::CreateNativeType();
    }
    gpu_service_internal_.reset(new GpuServiceInternal(
        gpu_init_->gpu_info(), gpu_init_->TakeWatchdogThread(),
        gpu_memory_buffer_factory_.get(), io_thread_.task_runner()));
  }
}

void GpuMain::TearDownOnGpuThread() {
  gpu_service_internal_.reset();
  gpu_memory_buffer_factory_.reset();
  gpu_init_.reset();
}

void GpuMain::CreateOnGpuThread(mojom::GpuServiceInternalRequest request) {
  if (gpu_service_internal_)
    gpu_service_internal_->Add(std::move(request));
}

void GpuMain::PreSandboxStartup() {
  // TODO(sad): https://crbug.com/645602
}

bool GpuMain::EnsureSandboxInitialized(
    gpu::GpuWatchdogThread* watchdog_thread) {
  // TODO(sad): https://crbug.com/645602
  return true;
}

}  // namespace ui
