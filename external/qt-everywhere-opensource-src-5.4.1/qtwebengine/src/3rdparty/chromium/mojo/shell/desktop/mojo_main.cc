// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/child_process.h"
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"
#include "mojo/shell/run.h"
#include "mojo/shell/switches.h"
#include "ui/gl/gl_surface.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  mojo::shell::InitializeLogging();

  // TODO(vtl): Unify parent and child process cases to the extent possible.
  if (scoped_ptr<mojo::shell::ChildProcess> child_process =
          mojo::shell::ChildProcess::Create(
              *base::CommandLine::ForCurrentProcess())) {
    child_process->Main();
  } else {
    gfx::GLSurface::InitializeOneOff();

    base::MessageLoop message_loop;
    mojo::shell::Context shell_context;

    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kOrigin)) {
      shell_context.mojo_url_resolver()->set_origin(
          command_line.GetSwitchValueASCII(switches::kOrigin));
    }

    std::vector<GURL> app_urls;
    base::CommandLine::StringVector args = command_line.GetArgs();
    for (base::CommandLine::StringVector::const_iterator it = args.begin();
         it != args.end();
         ++it)
      app_urls.push_back(GURL(*it));

    message_loop.PostTask(FROM_HERE,
                          base::Bind(mojo::shell::Run,
                                     &shell_context,
                                     app_urls));

    message_loop.Run();
  }

  return 0;
}
