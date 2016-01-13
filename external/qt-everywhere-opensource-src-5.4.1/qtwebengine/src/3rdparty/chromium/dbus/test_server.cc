// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "dbus/bus.h"
#include "dbus/test_service.h"

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();

  base::Thread* dbus_thread = new base::Thread("D-Bus Thread");
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  CHECK(dbus_thread->StartWithOptions(thread_options));

  dbus::TestService::Options options;
  options.dbus_task_runner = dbus_thread->message_loop_proxy();
  dbus::TestService* test_service = new dbus::TestService(options);
  CHECK(test_service->StartService());
  CHECK(test_service->WaitUntilServiceIsStarted());
  CHECK(test_service->HasDBusThread());
  base::PlatformThread::Join(dbus_thread->thread_handle());
}
