// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/named_platform_channel_pair.h"

#include <sddl.h>
#include <windows.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#include "mojo/edk/embedder/platform_handle.h"

namespace mojo {
namespace edk {

namespace {

const char kMojoNamedPlatformChannelPipeSwitch[] =
    "mojo-named-platform-channel-pipe";

std::wstring GeneratePipeName() {
  return base::StringPrintf(L"\\\\.\\pipe\\mojo.%u.%u.%I64u",
                            GetCurrentProcessId(), GetCurrentThreadId(),
                            base::RandUint64());

}

}  // namespace

NamedPlatformChannelPair::NamedPlatformChannelPair() {
  pipe_name_ = GeneratePipeName();

  PSECURITY_DESCRIPTOR security_desc = nullptr;
  ULONG security_desc_len = 0;
  // Create a DACL to grant:
  // GA = Generic All
  // access to:
  // SY = LOCAL_SYSTEM
  // BA = BUILTIN_ADMINISTRATORS
  // OW = OWNER_RIGHTS
  PCHECK(ConvertStringSecurityDescriptorToSecurityDescriptor(
      L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;OW)",
      SDDL_REVISION_1, &security_desc, &security_desc_len));
  std::unique_ptr<void, decltype(::LocalFree)*> p(security_desc, ::LocalFree);
  SECURITY_ATTRIBUTES security_attributes = {
      sizeof(SECURITY_ATTRIBUTES), security_desc, FALSE };

  const DWORD kOpenMode =
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
  const DWORD kPipeMode =
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS;
  PlatformHandle handle(
      CreateNamedPipeW(pipe_name_.c_str(), kOpenMode, kPipeMode,
                       1,           // Max instances.
                       4096,        // Out buffer size.
                       4096,        // In buffer size.
                       5000,        // Timeout in milliseconds.
                       &security_attributes));
  handle.needs_connection = true;
  server_handle_.reset(handle);
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
  std::wstring client_handle_string =
      command_line.GetSwitchValueNative(kMojoNamedPlatformChannelPipeSwitch);

  if (client_handle_string.empty())
    return ScopedPlatformHandle();

  // Note: This may block.
  BOOL ok = WaitNamedPipeW(client_handle_string.c_str(),
                           NMPWAIT_USE_DEFAULT_WAIT);
  if (!ok)
    return ScopedPlatformHandle();

  // In order to support passing the pipe name on the command line, the pipe
  // handle is lazily created from the pipe name when requested.
  const DWORD kDesiredAccess = GENERIC_READ | GENERIC_WRITE;
  // The SECURITY_ANONYMOUS flag means that the server side cannot impersonate
  // the client.
  const DWORD kFlags =
      SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS | FILE_FLAG_OVERLAPPED;
  ScopedPlatformHandle handle(
      PlatformHandle(CreateFileW(client_handle_string.c_str(), kDesiredAccess,
                                 0,  // No sharing.
                                 nullptr, OPEN_EXISTING, kFlags,
                                 nullptr)));  // No template file.
  PCHECK(handle.is_valid());
  return handle;
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
  command_line->AppendSwitchNative(
      kMojoNamedPlatformChannelPipeSwitch, pipe_name_);
}

}  // namespace edk
}  // namespace mojo
