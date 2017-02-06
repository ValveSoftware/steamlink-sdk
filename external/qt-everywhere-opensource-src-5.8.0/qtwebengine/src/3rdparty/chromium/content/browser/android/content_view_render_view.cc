// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_render_view.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "cc/layers/layer.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_layer_renderer.h"
#include "jni/ContentViewRenderView_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/size.h"

using base::android::ScopedJavaLocalRef;

namespace content {

// static
bool ContentViewRenderView::RegisterContentViewRenderView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ContentViewRenderView::ContentViewRenderView(JNIEnv* env,
                                             jobject obj,
                                             gfx::NativeWindow root_window)
    : root_window_(root_window), current_surface_format_(0) {
  java_obj_.Reset(env, obj);
}

ContentViewRenderView::~ContentViewRenderView() {
}

// static
static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  jlong native_root_window) {
  gfx::NativeWindow root_window =
      reinterpret_cast<gfx::NativeWindow>(native_root_window);
  ContentViewRenderView* content_view_render_view =
      new ContentViewRenderView(env, obj, root_window);
  return reinterpret_cast<intptr_t>(content_view_render_view);
}

void ContentViewRenderView::Destroy(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  delete this;
}

void ContentViewRenderView::SetCurrentContentViewCore(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong native_content_view_core) {
  InitCompositor();
  ContentViewCoreImpl* content_view_core =
      reinterpret_cast<ContentViewCoreImpl*>(native_content_view_core);
  compositor_->SetRootLayer(content_view_core ? content_view_core->GetLayer()
                                              : scoped_refptr<cc::Layer>());
}

void ContentViewRenderView::SurfaceCreated(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  current_surface_format_ = 0;
  InitCompositor();
}

void ContentViewRenderView::SurfaceDestroyed(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  compositor_->SetSurface(NULL);
  current_surface_format_ = 0;
}

void ContentViewRenderView::SurfaceChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint format,
    jint width,
    jint height,
    const JavaParamRef<jobject>& surface) {
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    compositor_->SetSurface(surface);
  }
  compositor_->SetWindowBounds(gfx::Size(width, height));
}

void ContentViewRenderView::SetOverlayVideoMode(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    bool enabled) {
  compositor_->SetHasTransparentBackground(enabled);
  compositor_->SetNeedsComposite();
}

void ContentViewRenderView::UpdateLayerTreeHost() {
  // TODO(wkorman): Rename Layout to UpdateLayerTreeHost in all Android
  // Compositor related classes.
}

void ContentViewRenderView::OnSwapBuffersCompleted(int pending_swap_buffers) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentViewRenderView_onSwapBuffersCompleted(env, java_obj_.obj());
}

void ContentViewRenderView::InitCompositor() {
  if (!compositor_)
    compositor_.reset(Compositor::Create(this, root_window_));
}

}  // namespace content
