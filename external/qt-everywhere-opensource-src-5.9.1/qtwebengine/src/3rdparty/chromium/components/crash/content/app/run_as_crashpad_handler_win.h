// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_
#define COMPONENTS_CRASH_CONTENT_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_

namespace base {
class CommandLine;
}

namespace crash_reporter {

// Helper for running an embedded copy of crashpad_handler. Searches for and
// removes --switches::kProcessType=xyz arguments in the command line, and all
// options starting with '/' (for "/prefetch:N"), and then runs
// crashpad::HandlerMain with the remaining arguments.
int RunAsCrashpadHandler(const base::CommandLine& command_line);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_
