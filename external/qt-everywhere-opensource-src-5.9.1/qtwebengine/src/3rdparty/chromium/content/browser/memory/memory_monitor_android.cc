// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_monitor_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/memory/memory_coordinator.h"
#include "jni/MemoryMonitorAndroid_jni.h"

namespace content {

namespace {

const size_t kMBShift = 20;

void RegisterComponentCallbacks() {
  Java_MemoryMonitorAndroid_registerComponentCallbacks(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
}

}

// An implementation of MemoryMonitorAndroid::Delegate using the Android APIs.
class MemoryMonitorAndroidDelegateImpl : public MemoryMonitorAndroid::Delegate {
 public:
  MemoryMonitorAndroidDelegateImpl() {}
  ~MemoryMonitorAndroidDelegateImpl() override {}

  using MemoryInfo = MemoryMonitorAndroid::MemoryInfo;
  void GetMemoryInfo(MemoryInfo* out) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryMonitorAndroidDelegateImpl);
};

void MemoryMonitorAndroidDelegateImpl::GetMemoryInfo(MemoryInfo* out) {
  DCHECK(out);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MemoryMonitorAndroid_getMemoryInfo(
      env, base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(out));
}

// Called by JNI to populate ActivityManager.MemoryInfo.
static void GetMemoryInfoCallback(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz,
    jlong avail_mem,
    jboolean low_memory,
    jlong threshold,
    jlong total_mem,
    jlong out_ptr) {
  DCHECK(out_ptr);
  MemoryMonitorAndroid::MemoryInfo* info =
      reinterpret_cast<MemoryMonitorAndroid::MemoryInfo*>(out_ptr);
  info->avail_mem = avail_mem;
  info->low_memory = low_memory;
  info->threshold = threshold;
  info->total_mem = total_mem;
}

// The maximum level of onTrimMemory (TRIM_MEMORY_COMPLETE).
const int kTrimMemoryLevelMax = 0x80;

// Called by JNI.
static void OnTrimMemory(JNIEnv* env,
                         const base::android::JavaParamRef<jclass>& jcaller,
                         jint level) {
  DCHECK(level >= 0 && level <= kTrimMemoryLevelMax);
  auto state = MemoryCoordinator::GetInstance()->GetCurrentMemoryState();
  switch (state) {
    case base::MemoryState::NORMAL:
      UMA_HISTOGRAM_ENUMERATION("Memory.Coordinator.TrimMemoryLevel.Normal",
                                level, kTrimMemoryLevelMax);
      break;
    case base::MemoryState::THROTTLED:
      UMA_HISTOGRAM_ENUMERATION("Memory.Coordinator.TrimMemoryLevel.Throttled",
                                level, kTrimMemoryLevelMax);
      break;
    case base::MemoryState::SUSPENDED:
      UMA_HISTOGRAM_ENUMERATION("Memory.Coordinator.TrimMemoryLevel.Suspended",
                                level, kTrimMemoryLevelMax);
      break;
    case base::MemoryState::UNKNOWN:
      NOTREACHED();
      break;
  }
}

// static
std::unique_ptr<MemoryMonitorAndroid> MemoryMonitorAndroid::Create() {
  auto delegate = base::WrapUnique(new MemoryMonitorAndroidDelegateImpl);
  return base::WrapUnique(new MemoryMonitorAndroid(std::move(delegate)));
}

// static
bool MemoryMonitorAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

MemoryMonitorAndroid::MemoryMonitorAndroid(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_.get());
  RegisterComponentCallbacks();
}

MemoryMonitorAndroid::~MemoryMonitorAndroid() {}

int MemoryMonitorAndroid::GetFreeMemoryUntilCriticalMB() {
  MemoryInfo info;
  GetMemoryInfo(&info);
  return (info.avail_mem - info.threshold) >> kMBShift;
}

void MemoryMonitorAndroid::GetMemoryInfo(MemoryInfo* out) {
  delegate_->GetMemoryInfo(out);
}

// Implementation of a factory function defined in memory_monitor.h.
std::unique_ptr<MemoryMonitor> CreateMemoryMonitor() {
  return MemoryMonitorAndroid::Create();
}

}  // namespace content
