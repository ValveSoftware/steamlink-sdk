// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "gin/public/isolate_holder.h"
#include "mojo/apps/js/mojo_runner_delegate.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define MOJO_APPS_JS_EXPORT __declspec(dllexport)
#else
#define CDECL
#define MOJO_APPS_JS_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace apps {

void Start(MojoHandle pipe, const std::string& module) {
  base::MessageLoop loop;

  gin::IsolateHolder instance(gin::IsolateHolder::kStrictMode);
  MojoRunnerDelegate delegate;
  gin::ShellRunner runner(&delegate, instance.isolate());
  delegate.Start(&runner, pipe, module);

  base::MessageLoop::current()->Run();
}

}  // namespace apps
}  // namespace mojo

extern "C" MOJO_APPS_JS_EXPORT MojoResult CDECL MojoMain(MojoHandle pipe) {
  mojo::GLES2Initializer gles2;
  mojo::apps::Start(pipe, "mojo/apps/js/main");
  return MOJO_RESULT_OK;
}
