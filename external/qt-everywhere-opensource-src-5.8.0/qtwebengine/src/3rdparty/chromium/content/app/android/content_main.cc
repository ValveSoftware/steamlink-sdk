// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/content_main.h"

#include <memory>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/content_switches.h"
#include "jni/ContentMain_jni.h"

using base::LazyInstance;

namespace content {

namespace {
LazyInstance<std::unique_ptr<ContentMainRunner>> g_content_runner =
    LAZY_INSTANCE_INITIALIZER;

LazyInstance<std::unique_ptr<ContentMainDelegate>> g_content_main_delegate =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

static jint Start(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  TRACE_EVENT0("startup", "content::Start");

  // On Android we can have multiple requests to start the browser in process
  // simultaneously. If we get an asynchonous request followed by a synchronous
  // request then we have to call this a second time to finish starting the
  // browser synchronously.
  if (!g_content_runner.Get().get()) {
    ContentMainParams params(g_content_main_delegate.Get().get());
    g_content_runner.Get().reset(ContentMainRunner::Create());
    g_content_runner.Get()->Initialize(params);
  }
  return g_content_runner.Get()->Run();
}

void SetContentMainDelegate(ContentMainDelegate* delegate) {
  DCHECK(!g_content_main_delegate.Get().get());
  g_content_main_delegate.Get().reset(delegate);
}

bool RegisterContentMain(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
