// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/switches.h"

namespace switches {

// Used to specify the type of child process (switch values from
// |ChildProcess::Type|).
const char kChildProcessType[] = "child-process-type";

// Force dynamically loaded apps / services to be loaded irrespective of cache
// instructions.
const char kDisableCache[] = "disable-cache";

// Load apps in separate processes.
// TODO(vtl): Work in progress; doesn't work. Flip this to "disable" (or maybe
// change it to "single-process") when it works.
const char kEnableMultiprocess[] = "enable-multiprocess";

// Map mojo: URLs to a shared library of similar name at this origin. See
// mojo_url_resolver.cc for details.
const char kOrigin[] = "origin";

// Enables the mojo spy, which acts as a man-in-the-middle inspector for
// message pipes and other activities. This is work in progress.
const char kSpy[] = "spy";

}  // namespace switches
