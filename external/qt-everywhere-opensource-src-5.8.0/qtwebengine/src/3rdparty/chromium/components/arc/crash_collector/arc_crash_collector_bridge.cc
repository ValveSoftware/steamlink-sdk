// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/crash_collector/arc_crash_collector_bridge.h"

#include <sysexits.h>
#include <unistd.h>

#include <utility>

#include "base/logging.h"
#include "base/process/launch.h"
#include "mojo/edk/embedder/embedder.h"

namespace {
const char kCrashReporterPath[] = "/sbin/crash_reporter";
}

namespace arc {

ArcCrashCollectorBridge::ArcCrashCollectorBridge(ArcBridgeService* bridge)
    : ArcService(bridge), binding_(this) {
  arc_bridge_service()->crash_collector()->AddObserver(this);
}

ArcCrashCollectorBridge::~ArcCrashCollectorBridge() {
  arc_bridge_service()->crash_collector()->RemoveObserver(this);
}

void ArcCrashCollectorBridge::OnInstanceReady() {
  mojom::CrashCollectorHostPtr host_ptr;
  binding_.Bind(mojo::GetProxy(&host_ptr));
  arc_bridge_service()->crash_collector()->instance()->Init(
      std::move(host_ptr));
}

void ArcCrashCollectorBridge::DumpCrash(const mojo::String& type,
                                        mojo::ScopedHandle pipe) {
  mojo::edk::ScopedPlatformHandle handle;
  mojo::edk::PassWrappedPlatformHandle(pipe.get().value(), &handle);

  base::FileHandleMappingVector fd_map = {
      std::make_pair(handle.get().handle, STDIN_FILENO)};

  base::LaunchOptions options;
  options.fds_to_remap = &fd_map;

  auto process =
      base::LaunchProcess({kCrashReporterPath, "--arc_java_crash=" + type.get(),
                           "--arc_device=" + device_, "--arc_board=" + board_,
                           "--arc_cpu_abi=" + cpu_abi_},
                          options);

  int exit_code;
  if (!process.WaitForExit(&exit_code)) {
    LOG(ERROR) << "Failed to wait for " << kCrashReporterPath;
  } else if (exit_code != EX_OK) {
    LOG(ERROR) << kCrashReporterPath << " failed with exit code " << exit_code;
  }
}

void ArcCrashCollectorBridge::SetBuildProperties(const mojo::String& device,
                                                 const mojo::String& board,
                                                 const mojo::String& cpu_abi) {
  device_ = device.get();
  board_ = board.get();
  cpu_abi_ = cpu_abi.get();
}

}  // namespace arc
