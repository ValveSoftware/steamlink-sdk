// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_H_
#define BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/command_line_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif  // defined(OS_ANDROID)

namespace base {
class SupportsUserData;
}

namespace blimp {
namespace client {
class CompositorDependencies;

// BlimpClientContext is the core class for the Blimp client. It provides hooks
// for creating BlimpContents and other features that are per
// BrowserContext/Profile.
class BlimpClientContext : public KeyedService {
 public:
#if defined(OS_ANDROID)
  // Returns a Java object of the type BlimpClientContext for the given
  // BlimpClientContext.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      BlimpClientContext* blimp_client_context);
#endif  // defined(OS_ANDROID)

  // Creates a BlimpClientContext. The implementation of this function
  // depends on whether the core or dummy implementation of Blimp has been
  // linked in.
  // The |io_thread_task_runner| must be the task runner to use for IO
  // operations.
  // The |file_thread_task_runner| must be the task runner to use for file
  // operations.
  // |compositor_dependencies| is a support object that provides all embedder
  // dependencies required by internal compositors.
  static BlimpClientContext* Create(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      std::unique_ptr<CompositorDependencies> compositor_dependencies,
      PrefService* local_state);

  // Register Blimp prefs. The implementation of this function depends on
  // whether the core or dummy implementation of Blimp has been linked in.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Apply blimp command-line switches to the corresponding preferences of the
  // blimp's own switch map.
  static void ApplyBlimpSwitches(CommandLinePrefStore* store);

  // The delegate provides all the required functionality from the embedder.
  // The context must be initialized with a |delegate| before it can be used.
  virtual void SetDelegate(BlimpClientContextDelegate* delegate) = 0;

  // Creates a new BlimpContents that will be shown in |window|.
  // TODO(mlliu): Currently we want to have a single BlimpContents. If there is
  // an existing contents, return nullptr (http://crbug.com/642558).
  virtual std::unique_ptr<BlimpContents> CreateBlimpContents(
      gfx::NativeWindow window) = 0;

  // Start authentication flow and connection to engine.
  virtual void Connect() = 0;
  virtual void ConnectWithAssignment(const Assignment& assignment) = 0;

 protected:
  BlimpClientContext() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpClientContext);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_H_
