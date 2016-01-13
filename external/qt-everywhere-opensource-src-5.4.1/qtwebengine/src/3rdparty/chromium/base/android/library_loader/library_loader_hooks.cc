// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/library_loader/library_loader_hooks.h"

#include "base/android/jni_string.h"
#include "base/at_exit.h"
#include "base/metrics/histogram.h"
#include "jni/LibraryLoader_jni.h"

namespace base {
namespace android {

namespace {

base::AtExitManager* g_at_exit_manager = NULL;
const char* g_library_version_number = "";
LibraryLoadedHook* g_registration_callback = NULL;

} // namespace

void SetLibraryLoadedHook(LibraryLoadedHook* func) {
  g_registration_callback = func;
}

static jboolean LibraryLoaded(JNIEnv* env, jclass clazz,
                          jobjectArray init_command_line) {
  if(g_registration_callback == NULL) {
    return true;
  }
  return g_registration_callback(env, clazz, init_command_line);
}

static void RecordChromiumAndroidLinkerHistogram(
    JNIEnv* env,
    jclass clazz,
    jboolean loaded_at_fixed_address_failed,
    jboolean is_low_memory_device) {
  UMA_HISTOGRAM_BOOLEAN("ChromiumAndroidLinker.LoadedAtFixedAddressFailed",
                        loaded_at_fixed_address_failed);
  UMA_HISTOGRAM_BOOLEAN("ChromiumAndroidLinker.IsLowMemoryDevice",
                        is_low_memory_device);
}

void LibraryLoaderExitHook() {
  if (g_at_exit_manager) {
    delete g_at_exit_manager;
    g_at_exit_manager = NULL;
  }
}

bool RegisterLibraryLoaderEntryHook(JNIEnv* env) {
  // We need the AtExitManager to be created at the very beginning.
  g_at_exit_manager = new base::AtExitManager();

  return RegisterNativesImpl(env);
}

void SetVersionNumber(const char* version_number) {
  g_library_version_number = strdup(version_number);
}

jstring GetVersionNumber(JNIEnv* env, jclass clazz) {
  return ConvertUTF8ToJavaString(env, g_library_version_number).Release();
}

static void RecordNativeLibraryHack(JNIEnv*, jclass, jboolean usedHack) {
  UMA_HISTOGRAM_BOOLEAN("LibraryLoader.NativeLibraryHack", usedHack);
}

}  // namespace android
}  // namespace base
