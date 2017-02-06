// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "content/browser/android/content_view_statics.h"
#include "content/common/android/address_parser.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "jni/ContentViewStatics_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;

namespace {

// TODO(pliard): http://crbug.com/235909. Move WebKit shared timer toggling
// functionality out of ContentViewStatistics and not be build on top of
// blink::Platform::SuspendSharedTimer.
// TODO(pliard): http://crbug.com/235912. Add unit tests for WebKit shared timer
// toggling.

// This tracks the renderer processes that received a suspend request. It's
// important on resume to only resume the renderer processes that were actually
// suspended as opposed to all the current renderer processes because the
// suspend calls are refcounted within BlinkPlatformImpl and it expects a
// perfectly matched number of resume calls.
// Note that this class is only accessed from the UI thread.
class SuspendedProcessWatcher : public content::RenderProcessHostObserver {
 public:

  // If the process crashes, stop watching the corresponding RenderProcessHost
  // and ensure it doesn't get over-resumed.
  void RenderProcessExited(content::RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override {
    StopWatching(host);
  }

  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override {
    StopWatching(host);
  }

  // Suspends timers in all current render processes.
  void SuspendWebKitSharedTimers() {
    DCHECK(suspended_processes_.empty());

    for (content::RenderProcessHost::iterator i(
            content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      host->AddObserver(this);
      host->Send(new ViewMsg_SetWebKitSharedTimersSuspended(true));
      suspended_processes_.push_back(host->GetID());
    }
  }

  // Resumes timers in processes that were previously stopped.
  void ResumeWebkitSharedTimers() {
    for (std::vector<int>::const_iterator it = suspended_processes_.begin();
         it != suspended_processes_.end(); ++it) {
      content::RenderProcessHost* host =
          content::RenderProcessHost::FromID(*it);
      DCHECK(host);
      host->RemoveObserver(this);
      host->Send(new ViewMsg_SetWebKitSharedTimersSuspended(false));
    }
    suspended_processes_.clear();
  }

 private:
  void StopWatching(content::RenderProcessHost* host) {
    std::vector<int>::iterator pos = std::find(suspended_processes_.begin(),
                                               suspended_processes_.end(),
                                               host->GetID());
    DCHECK(pos != suspended_processes_.end());
    host->RemoveObserver(this);
    suspended_processes_.erase(pos);
  }

  std::vector<int /* RenderProcessHost id */> suspended_processes_;
};

base::LazyInstance<SuspendedProcessWatcher> g_suspended_processes_watcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Returns the first substring consisting of the address of a physical location.
static ScopedJavaLocalRef<jstring> FindAddress(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& addr) {
  base::string16 content_16 = ConvertJavaStringToUTF16(env, addr);
  base::string16 result_16;
  if (content::address_parser::FindAddress(content_16, &result_16))
    return ConvertUTF16ToJavaString(env, result_16);
  return ScopedJavaLocalRef<jstring>();
}

static void SetWebKitSharedTimersSuspended(JNIEnv* env,
                                           const JavaParamRef<jclass>& obj,
                                           jboolean suspend) {
  if (suspend) {
    g_suspended_processes_watcher.Pointer()->SuspendWebKitSharedTimers();
  } else {
    g_suspended_processes_watcher.Pointer()->ResumeWebkitSharedTimers();
  }
}

namespace content {

bool RegisterWebViewStatics(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
