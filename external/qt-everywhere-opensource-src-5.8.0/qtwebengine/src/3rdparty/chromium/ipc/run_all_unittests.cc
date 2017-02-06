// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/scoped_ipc_support.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/test/test_file_util.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mach_port_broker.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_ANDROID)
  base::InitAndroidMultiProcessTestHelper(main);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::RegisterContentUriTestUtils(env);
#endif
  base::TestSuite test_suite(argc, argv);
  mojo::edk::Init();
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  // Leak this because its destructor calls mojo::edk::ShutdownIPCSupport which
  // really does nothing in the new EDK but does depend on the current message
  // loop, which is destructed inside base::LaunchUnitTests.
  new mojo::edk::test::ScopedIPCSupport(test_io_thread.task_runner());

#if defined(OS_MACOSX) && !defined(OS_IOS)
  base::MachPortBroker mach_broker("mojo_test");
  CHECK(mach_broker.Init());
  mojo::edk::SetMachPortProvider(&mach_broker);
#endif

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
