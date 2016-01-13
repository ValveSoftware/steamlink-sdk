// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "mojo/services/native_viewport/native_viewport_export.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/gfx/size.h"

namespace gpu {
class GLInProcessContext;
}

struct ANativeWindow;

namespace mojo {
namespace services {

class MOJO_NATIVE_VIEWPORT_EXPORT NativeViewportAndroid
    : public NativeViewport {
 public:
  static MOJO_NATIVE_VIEWPORT_EXPORT bool Register(JNIEnv* env);

  explicit NativeViewportAndroid(shell::Context* context,
                                 NativeViewportDelegate* delegate);
  virtual ~NativeViewportAndroid();

  void Destroy(JNIEnv* env, jobject obj);
  void SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceSetSize(JNIEnv* env, jobject obj, jint width, jint height);
  bool TouchEvent(JNIEnv* env, jobject obj, jint pointer_id, jint action,
                  jfloat x, jfloat y, jlong time_ms);

 private:
  // Overridden from NativeViewport:
  virtual void Init(const gfx::Rect& bounds) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;

  void ReleaseWindow();

  NativeViewportDelegate* delegate_;
  shell::Context* context_;
  ANativeWindow* window_;
  gfx::Rect bounds_;
  ui::SequentialIDGenerator id_generator_;

  base::WeakPtrFactory<NativeViewportAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportAndroid);
};


}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_
