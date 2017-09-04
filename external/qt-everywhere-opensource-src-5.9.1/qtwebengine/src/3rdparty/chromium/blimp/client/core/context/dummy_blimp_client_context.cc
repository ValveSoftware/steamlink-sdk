// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/context/dummy_blimp_client_context.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/context/android/dummy_blimp_client_context_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/blimp_client_context_impl.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    std::unique_ptr<CompositorDependencies> compositor_dependencies,
    PrefService* local_state) {
#if defined(OS_ANDROID)
  return new DummyBlimpClientContextAndroid();
#else
  return new DummyBlimpClientContext();
#endif  // defined(OS_ANDROID)
}

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/blimp_client_context_impl.cc should be linked in to
// any binary using BlimpClientContext::RegisterPrefs.
// static
void BlimpClientContext::RegisterPrefs(PrefRegistrySimple* registry) {}

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/blimp_client_context_impl.cc should be linked in to
// any binary using BlimpClientContext::ApplyBlimpSwitches.
// static
void BlimpClientContext::ApplyBlimpSwitches(CommandLinePrefStore* store) {}

DummyBlimpClientContext::DummyBlimpClientContext() : BlimpClientContext() {
  UMA_HISTOGRAM_BOOLEAN("Blimp.Supported", false);
}

DummyBlimpClientContext::~DummyBlimpClientContext() {}

void DummyBlimpClientContext::SetDelegate(
    BlimpClientContextDelegate* delegate) {}

std::unique_ptr<BlimpContents> DummyBlimpClientContext::CreateBlimpContents(
    gfx::NativeWindow window) {
  return nullptr;
}

void DummyBlimpClientContext::Connect() {
  NOTREACHED();
}

void DummyBlimpClientContext::ConnectWithAssignment(
    const Assignment& assignment) {
  NOTREACHED();
}

}  // namespace client
}  // namespace blimp
