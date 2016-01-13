// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/child_process.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/embedder/platform_channel_pair.h"
#include "mojo/shell/app_child_process.h"
#include "mojo/shell/switches.h"
#include "mojo/shell/test_child_process.h"

namespace mojo {
namespace shell {

ChildProcess::~ChildProcess() {
}

// static
scoped_ptr<ChildProcess> ChildProcess::Create(
    const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switches::kChildProcessType))
    return scoped_ptr<ChildProcess>();

  int type_as_int;
  CHECK(base::StringToInt(command_line.GetSwitchValueASCII(
            switches::kChildProcessType), &type_as_int));

  scoped_ptr<ChildProcess> rv;
  switch (type_as_int) {
    case TYPE_TEST:
      rv.reset(new TestChildProcess());
      break;
    case TYPE_APP:
      rv.reset(new AppChildProcess());
      break;
    default:
      CHECK(false) << "Invalid child process type";
      break;
  }

  if (rv) {
    rv->platform_channel_ =
        embedder::PlatformChannelPair::PassClientHandleFromParentProcess(
            command_line);
    CHECK(rv->platform_channel_.is_valid());
  }

  return rv.Pass();
}

ChildProcess::ChildProcess() {
}

}  // namespace shell
}  // namespace mojo
