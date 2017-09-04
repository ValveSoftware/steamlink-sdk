// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/crash_collector/arc_crash_collector_bridge.h"

#include <sysexits.h>
#include <unistd.h>

#include <utility>

#include "base/logging.h"
#include "base/process/launch.h"
#include "components/arc/arc_bridge_service.h"
#include "mojo/edk/embedder/embedder.h"

namespace {

const char kCrashReporterPath[] = "/sbin/crash_reporter";

// Runs crash_reporter to save the crash info provided via the pipe.
void RunCrashReporter(const std::string& crash_type,
                      const std::string& device,
                      const std::string& board,
                      const std::string& cpu_abi,
                      mojo::edk::ScopedPlatformHandle pipe) {
  base::FileHandleMappingVector fd_map = {
      std::make_pair(pipe.get().handle, STDIN_FILENO)};

  base::LaunchOptions options;
  options.fds_to_remap = &fd_map;

  auto process =
      base::LaunchProcess({kCrashReporterPath, "--arc_java_crash=" + crash_type,
                           "--arc_device=" + device, "--arc_board=" + board,
                           "--arc_cpu_abi=" + cpu_abi},
                          options);

  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    LOG(ERROR) << "Failed to wait for " << kCrashReporterPath;
  } else if (exit_code != EX_OK) {
    LOG(ERROR) << kCrashReporterPath << " failed with exit code " << exit_code;
  }
}

}  // namespace

namespace arc {

ArcCrashCollectorBridge::ArcCrashCollectorBridge(
    ArcBridgeService* bridge,
    scoped_refptr<base::TaskRunner> blocking_task_runner)
    : ArcService(bridge),
      blocking_task_runner_(blocking_task_runner),
      binding_(this) {
  arc_bridge_service()->crash_collector()->AddObserver(this);
}

ArcCrashCollectorBridge::~ArcCrashCollectorBridge() {
  arc_bridge_service()->crash_collector()->RemoveObserver(this);
}

void ArcCrashCollectorBridge::OnInstanceReady() {
  mojom::CrashCollectorHostPtr host_ptr;
  binding_.Bind(mojo::GetProxy(&host_ptr));
  auto* instance =
      arc_bridge_service()->crash_collector()->GetInstanceForMethod("Init");
  DCHECK(instance);
  instance->Init(std::move(host_ptr));
}

void ArcCrashCollectorBridge::DumpCrash(const std::string& type,
                                        mojo::ScopedHandle pipe) {
  mojo::edk::ScopedPlatformHandle pipe_handle;
  mojo::edk::PassWrappedPlatformHandle(pipe.release().value(), &pipe_handle);

  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RunCrashReporter, type, device_, board_, cpu_abi_,
                            base::Passed(std::move(pipe_handle))));
}

void ArcCrashCollectorBridge::SetBuildProperties(const std::string& device,
                                                 const std::string& board,
                                                 const std::string& cpu_abi) {
  device_ = device;
  board_ = board;
  cpu_abi_ = cpu_abi;
}

}  // namespace arc
