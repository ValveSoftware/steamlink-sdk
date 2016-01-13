// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/logging.h"
#include "content/browser/power_save_blocker_impl.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "jni/PowerSaveBlocker_jni.h"
#include "ui/base/android/view_android.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using gfx::NativeView;

namespace content {

class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  explicit Delegate(NativeView view_android) {
    j_view_android_ = JavaObjectWeakGlobalRef(
        AttachCurrentThread(), view_android->GetJavaObject().obj());
  }

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() {}

  JavaObjectWeakGlobalRef j_view_android_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

void PowerSaveBlockerImpl::Delegate::ApplyBlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_object = j_view_android_.get(env);
  if (j_object.obj())
    Java_PowerSaveBlocker_applyBlock(env, j_object.obj());
}

void PowerSaveBlockerImpl::Delegate::RemoveBlock() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_object = j_view_android_.get(env);
  if (j_object.obj())
    Java_PowerSaveBlocker_removeBlock(env, j_object.obj());
}

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           const std::string& reason) {
  // Don't support kPowerSaveBlockPreventAppSuspension
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
  if (delegate_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Delegate::RemoveBlock, delegate_));
  }
}

void PowerSaveBlockerImpl::InitDisplaySleepBlocker(NativeView view_android) {
  if (!view_android)
    return;

  delegate_ = new Delegate(view_android);
  // This may be called on any thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Delegate::ApplyBlock, delegate_));
}

bool RegisterPowerSaveBlocker(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
