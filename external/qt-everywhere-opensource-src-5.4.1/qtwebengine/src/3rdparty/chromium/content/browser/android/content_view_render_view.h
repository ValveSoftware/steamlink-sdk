// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_

#include "base/android/jni_weak_ref.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/android/compositor_client.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Layer;
}

namespace content {
class Compositor;
class LayerTreeBuildHelper;

class ContentViewRenderView : public CompositorClient {
 public:
  // Registers the JNI methods for ContentViewRender.
  static bool RegisterContentViewRenderView(JNIEnv* env);

  ContentViewRenderView(JNIEnv* env,
                        jobject obj,
                        gfx::NativeWindow root_window);

  // Methods called from Java via JNI -----------------------------------------
  void Destroy(JNIEnv* env, jobject obj);
  void SetCurrentContentViewCore(JNIEnv* env, jobject obj,
                                 jlong native_content_view_core);
  void SetLayerTreeBuildHelper(
      JNIEnv* env, jobject obj, jlong native_build_helper);
  void SurfaceCreated(JNIEnv* env, jobject obj);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceChanged(JNIEnv* env, jobject obj,
                      jint format, jint width, jint height, jobject surface);
  jboolean Composite(JNIEnv* env, jobject obj);
  void SetOverlayVideoMode(JNIEnv* env, jobject obj, bool enabled);

  // CompositorClient implementation
  virtual void Layout() OVERRIDE;
  virtual void OnSwapBuffersCompleted(int pending_swap_buffers) OVERRIDE;

 private:
  virtual ~ContentViewRenderView();

  void InitCompositor();

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  scoped_ptr<LayerTreeBuildHelper> layer_tree_build_helper_;

  scoped_ptr<content::Compositor> compositor_;

  gfx::NativeWindow root_window_;
  int current_surface_format_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewRenderView);
};



}

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_RENDER_VIEW_H_
