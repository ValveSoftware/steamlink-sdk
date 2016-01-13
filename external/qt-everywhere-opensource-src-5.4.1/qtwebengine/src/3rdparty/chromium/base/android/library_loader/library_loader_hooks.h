// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LIBRARY_LOADER_HOOKS_H_
#define BASE_ANDROID_LIBRARY_LOADER_HOOKS_H_

#include <jni.h>

#include "base/base_export.h"

namespace base {
namespace android {

// Registers the callbacks that allows the entry point of the library to be
// exposed to the calling java code.  This handles only registering the
// the callbacks needed by the loader. Any application specific JNI bindings
// should happen once the native library has fully loaded, either in the library
// loaded hook function or later.
BASE_EXPORT bool RegisterLibraryLoaderEntryHook(JNIEnv* env);

// Typedef for hook function to be called (indirectly from Java) once the
// libraries are loaded. The hook function should register the JNI bindings
// required to start the application. It should return true for success and
// false for failure.
// Note: this can't use base::Callback because there is no way of initializing
// the default callback without using static objects, which we forbid.
typedef bool LibraryLoadedHook(JNIEnv* env,
                               jclass clazz,
                               jobjectArray init_command_line);

// Set the hook function to be called (from Java) once the libraries are loaded.
// SetLibraryLoadedHook may only be called from JNI_OnLoad. The hook function
// should register the JNI bindings required to start the application.

BASE_EXPORT void SetLibraryLoadedHook(LibraryLoadedHook* func);

// Pass the version name to the loader. This used to check that the library
// version matches the version expected by Java before completing JNI
// registration.
// Note: argument must remain valid at least until library loading is complete.
BASE_EXPORT void SetVersionNumber(const char* version_number);

// Call on exit to delete the AtExitManager which OnLibraryLoadedOnUIThread
// created.
BASE_EXPORT void LibraryLoaderExitHook();

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_LIBRARY_LOADER_HOOKS_H_
