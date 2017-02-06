// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_
#define CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_

#include <string>

#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {
struct UserMetricsAction;
}

namespace shell {
class InterfaceProvider;
class InterfaceRegistry;
}

namespace content {

  class MojoShellConnection;

// An abstract base class that contains logic shared between most child
// processes of the embedder.
class CONTENT_EXPORT ChildThread : public IPC::Sender {
 public:
  // Returns the one child thread for this process.  Note that this can only be
  // accessed when running on the child thread itself.
  static ChildThread* Get();

  ~ChildThread() override {}

#if defined(OS_WIN)
  // Request that the given font be loaded by the browser so it's cached by the
  // OS. Please see ChildProcessHost::PreCacheFont for details.
  virtual void PreCacheFont(const LOGFONT& log_font) = 0;

  // Release cached font.
  virtual void ReleaseCachedFonts() = 0;
#endif

  // Sends over a base::UserMetricsAction to be recorded by user metrics as
  // an action. Once a new user metric is added, run
  //   tools/metrics/actions/extract_actions.py
  // to add the metric to actions.xml, then update the <owner>s and
  // <description> sections. Make sure to include the actions.xml file when you
  // upload your code for review!
  //
  // WARNING: When using base::UserMetricsAction, base::UserMetricsAction
  // and a string literal parameter must be on the same line, e.g.
  //   RenderThread::Get()->RecordAction(
  //       base::UserMetricsAction("my extremely long action name"));
  // because otherwise our processing scripts won't pick up on new actions.
  virtual void RecordAction(const base::UserMetricsAction& action) = 0;

  // Sends over a string to be recorded by user metrics as a computed action.
  // When you use this you need to also update the rules for extracting known
  // actions in chrome/tools/extract_actions.py.
  virtual void RecordComputedAction(const std::string& action) = 0;

  // Returns the MojoShellConnection for the thread (from which a
  // shell::Connector can be obtained).
  virtual MojoShellConnection* GetMojoShellConnection() = 0;

  // Returns the InterfaceRegistry that this process uses to expose interfaces
  // to the browser.
  virtual shell::InterfaceRegistry* GetInterfaceRegistry() = 0;

  // Returns the InterfaceProvider that this process can use to bind
  // interfaces exposed to it by the browser.
  virtual shell::InterfaceProvider* GetRemoteInterfaces() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_
