// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/named_platform_channel_pair.h"

#include <windows.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/platform_handle.h"

namespace mojo {
namespace edk {

namespace {

const char kMojoNamedPlatformChannelPipeSwitch[] =
    "mojo-named-platform-channel-pipe";

std::wstring GeneratePipeName() {
  return base::StringPrintf(L"%u.%u.%I64u", GetCurrentProcessId(),
                            GetCurrentThreadId(), base::RandUint64());
}

}  // namespace

NamedPlatformChannelPair::NamedPlatformChannelPair(
    const NamedPlatformChannelPair::Options& options)
    : pipe_handle_(GeneratePipeName()) {
  CreateServerHandleOptions server_handle_options;
  server_handle_options.security_descriptor = options.security_descriptor;
  server_handle_options.enforce_uniqueness = true;
  server_handle_ = CreateServerHandle(pipe_handle_, server_handle_options);
  PCHECK(server_handle_.is_valid());
}

NamedPlatformChannelPair::~NamedPlatformChannelPair() {}

ScopedPlatformHandle NamedPlatformChannelPair::PassServerHandle() {
  return std::move(server_handle_);
}

// static
ScopedPlatformHandle
NamedPlatformChannelPair::PassClientHandleFromParentProcess(
    const base::CommandLine& command_line) {
  // In order to support passing the pipe name on the command line, the pipe
  // handle is lazily created from the pipe name when requested.
  NamedPlatformHandle handle(
      command_line.GetSwitchValueNative(kMojoNamedPlatformChannelPipeSwitch));

  if (!handle.is_valid())
    return ScopedPlatformHandle();

  return CreateClientHandle(handle);
}

void NamedPlatformChannelPair::PrepareToPassClientHandleToChildProcess(
    base::CommandLine* command_line) const {
  DCHECK(command_line);

  // Log a warning if the command line already has the switch, but "clobber" it
  // anyway, since it's reasonably likely that all the switches were just copied
  // from the parent.
  LOG_IF(WARNING,
         command_line->HasSwitch(kMojoNamedPlatformChannelPipeSwitch))
      << "Child command line already has switch --"
      << kMojoNamedPlatformChannelPipeSwitch << "="
      << command_line->GetSwitchValueNative(
          kMojoNamedPlatformChannelPipeSwitch);
  // (Any existing switch won't actually be removed from the command line, but
  // the last one appended takes precedence.)
  command_line->AppendSwitchNative(kMojoNamedPlatformChannelPipeSwitch,
                                   pipe_handle_.name);
}

}  // namespace edk
}  // namespace mojo
