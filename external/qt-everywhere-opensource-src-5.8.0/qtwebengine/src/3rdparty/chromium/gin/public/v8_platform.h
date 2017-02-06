// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_V8_PLATFORM_H_
#define GIN_PUBLIC_V8_PLATFORM_H_

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "gin/gin_export.h"
#include "v8/include/v8-platform.h"

namespace gin {

// A v8::Platform implementation to use with gin.
class GIN_EXPORT V8Platform : public NON_EXPORTED_BASE(v8::Platform) {
 public:
  static V8Platform* Get();

  // v8::Platform implementation.
  size_t NumberOfAvailableBackgroundThreads() override;
  void CallOnBackgroundThread(
      v8::Task* task,
      v8::Platform::ExpectedRuntime expected_runtime) override;
  void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) override;
  void CallDelayedOnForegroundThread(v8::Isolate* isolate,
                                     v8::Task* task,
                                     double delay_in_seconds) override;
  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  v8::IdleTask* task) override;
  bool IdleTasksEnabled(v8::Isolate* isolate) override;
  double MonotonicallyIncreasingTime() override;
  const uint8_t* GetCategoryGroupEnabled(const char* name) override;
  const char* GetCategoryGroupName(
      const uint8_t* category_enabled_flag) override;
  uint64_t AddTraceEvent(char phase,
                         const uint8_t* category_enabled_flag,
                         const char* name,
                         const char* scope,
                         uint64_t id,
                         uint64_t bind_id,
                         int32_t num_args,
                         const char** arg_names,
                         const uint8_t* arg_types,
                         const uint64_t* arg_values,
                         unsigned int flags) override;
  void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                const char* name,
                                uint64_t handle) override;

 private:
  friend struct base::DefaultLazyInstanceTraits<V8Platform>;

  V8Platform();
  ~V8Platform() override;

  DISALLOW_COPY_AND_ASSIGN(V8Platform);
};

}  // namespace gin

#endif  // GIN_PUBLIC_V8_PLATFORM_H_
