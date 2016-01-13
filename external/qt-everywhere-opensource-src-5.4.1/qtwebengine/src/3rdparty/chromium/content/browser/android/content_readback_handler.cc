// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_readback_handler.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "content/browser/android/content_view_core_impl.h"
#include "jni/ContentReadbackHandler_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/android/window_android.h"
#include "ui/base/android/window_android_compositor.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/rect.h"

namespace content {

namespace {

typedef base::Callback<void(bool, const SkBitmap&)> ResultCallback;

void OnFinishCopyOutputRequest(
    const ResultCallback& result_callback,
    scoped_ptr<cc::CopyOutputResult> copy_output_result) {
  if (!copy_output_result->HasBitmap()) {
    result_callback.Run(false, SkBitmap());
    return;
  }

  scoped_ptr<SkBitmap> bitmap = copy_output_result->TakeBitmap();
  result_callback.Run(true, *bitmap.Pass());
}

}  // anonymous namespace

// static
bool ContentReadbackHandler::RegisterContentReadbackHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ContentReadbackHandler::ContentReadbackHandler(JNIEnv* env, jobject obj)
    : weak_factory_(this) {
  java_obj_.Reset(env, obj);
}

void ContentReadbackHandler::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void ContentReadbackHandler::GetContentBitmap(JNIEnv* env,
                                              jobject obj,
                                              jint readback_id,
                                              jfloat scale,
                                              jobject config,
                                              jfloat x,
                                              jfloat y,
                                              jfloat width,
                                              jfloat height,
                                              jobject content_view_core) {
  ContentViewCore* view =
      ContentViewCore::GetNativeContentViewCore(env, content_view_core);
  DCHECK(view);

  ResultCallback result_callback =
      base::Bind(&ContentReadbackHandler::OnFinishReadback,
                 weak_factory_.GetWeakPtr(),
                 readback_id);

  view->GetScaledContentBitmap(
      scale, config, gfx::Rect(x, y, width, height), result_callback);
}

void ContentReadbackHandler::GetCompositorBitmap(JNIEnv* env,
                                                 jobject obj,
                                                 jint readback_id,
                                                 jlong native_window_android) {
  ui::WindowAndroid* window_android =
      reinterpret_cast<ui::WindowAndroid*>(native_window_android);
  DCHECK(window_android);

  ResultCallback result_callback =
      base::Bind(&ContentReadbackHandler::OnFinishReadback,
                 weak_factory_.GetWeakPtr(),
                 readback_id);

  base::Callback<void(scoped_ptr<cc::CopyOutputResult>)> copy_output_callback =
      base::Bind(&OnFinishCopyOutputRequest,
                 result_callback);

  ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();

  if (!compositor) {
    copy_output_callback.Run(cc::CopyOutputResult::CreateEmptyResult());
    return;
  }

  compositor->RequestCopyOfOutputOnRootLayer(
      cc::CopyOutputRequest::CreateBitmapRequest(copy_output_callback));
}

ContentReadbackHandler::~ContentReadbackHandler() {}

void ContentReadbackHandler::OnFinishReadback(int readback_id,
                                              bool success,
                                              const SkBitmap& bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (success)
    java_bitmap = gfx::ConvertToJavaBitmap(&bitmap);

  Java_ContentReadbackHandler_notifyGetBitmapFinished(
      env, java_obj_.obj(), readback_id, success, java_bitmap.obj());
}

// static
static jlong Init(JNIEnv* env, jobject obj) {
  ContentReadbackHandler* content_readback_handler =
      new ContentReadbackHandler(env, obj);
  return reinterpret_cast<intptr_t>(content_readback_handler);
}

}  // namespace content
