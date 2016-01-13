// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
#define CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_

#include <jni.h>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

// Register all content JNI functions now, rather than waiting for the process
// of fully loading the native library to complete.
CONTENT_EXPORT bool EnsureJniRegistered(JNIEnv* env);

// Do the intialization of content needed immediately after the native library
// has loaded.
// This is designed to be used as a hook function to be passed to
// base::android::SetLibraryLoadedHook
CONTENT_EXPORT bool LibraryLoaded(JNIEnv* env,
                                  jclass clazz,
                                  jobjectArray init_command_line);

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
