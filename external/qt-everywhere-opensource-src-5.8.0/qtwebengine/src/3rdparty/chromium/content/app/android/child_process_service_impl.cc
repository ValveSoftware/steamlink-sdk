// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/child_process_service_impl.h"

#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "base/android/jni_array.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/global_descriptors.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/content_descriptors.h"
#include "gpu/ipc/common/android/surface_texture_manager.h"
#include "gpu/ipc/common/android/surface_texture_peer.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ipc/ipc_descriptors.h"
#include "jni/ChildProcessServiceImpl_jni.h"
#include "ui/gl/android/scoped_java_surface.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaIntArrayToIntVector;

namespace content {

namespace {

// TODO(sievers): Use two different implementations of this depending on if
// we're in a renderer or gpu process.
class SurfaceTextureManagerImpl : public gpu::SurfaceTextureManager,
                                  public gpu::SurfaceTexturePeer,
                                  public gpu::GpuSurfaceLookup {
 public:
  // |service impl| is the instance of
  // org.chromium.content.app.ChildProcessServiceImpl.
  explicit SurfaceTextureManagerImpl(
      const base::android::JavaRef<jobject>& service_impl)
      : service_impl_(service_impl) {
    SurfaceTexturePeer::InitInstance(this);
    gpu::GpuSurfaceLookup::InitInstance(this);
  }
  ~SurfaceTextureManagerImpl() override {
    SurfaceTexturePeer::InitInstance(NULL);
    gpu::GpuSurfaceLookup::InitInstance(NULL);
  }

  // Overridden from SurfaceTextureManager:
  void RegisterSurfaceTexture(int surface_texture_id,
                              int client_id,
                              gl::SurfaceTexture* surface_texture) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ChildProcessServiceImpl_createSurfaceTextureSurface(
        env,
        service_impl_.obj(),
        surface_texture_id,
        client_id,
        surface_texture->j_surface_texture().obj());
  }
  void UnregisterSurfaceTexture(int surface_texture_id,
                                int client_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ChildProcessServiceImpl_destroySurfaceTextureSurface(
        env, service_impl_.obj(), surface_texture_id, client_id);
  }
  gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    gl::ScopedJavaSurface surface(
        Java_ChildProcessServiceImpl_getSurfaceTextureSurface(
            env, service_impl_.obj(), surface_texture_id));

    if (surface.j_surface().is_null())
      return NULL;

    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    ANativeWindow* native_window =
        ANativeWindow_fromSurface(env, surface.j_surface().obj());

    return native_window;
  }

  // Overridden from SurfaceTexturePeer:
  void EstablishSurfaceTexturePeer(
      base::ProcessHandle pid,
      scoped_refptr<gl::SurfaceTexture> surface_texture,
      int primary_id,
      int secondary_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    content::Java_ChildProcessServiceImpl_establishSurfaceTexturePeer(
        env,
        service_impl_.obj(),
        pid,
        surface_texture->j_surface_texture().obj(),
        primary_id,
        secondary_id);
  }

  // Overridden from GpuSurfaceLookup:
  gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    gl::ScopedJavaSurface surface(
        content::Java_ChildProcessServiceImpl_getViewSurface(
            env, service_impl_.obj(), surface_id));

    if (surface.j_surface().is_null())
      return NULL;

    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    ANativeWindow* native_window =
        ANativeWindow_fromSurface(env, surface.j_surface().obj());

    return native_window;
  }

  // Overridden from GpuSurfaceLookup:
  gl::ScopedJavaSurface AcquireJavaSurface(int surface_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    return gl::ScopedJavaSurface(
        content::Java_ChildProcessServiceImpl_getViewSurface(
            env, service_impl_.obj(), surface_id));
  }

 private:
  // The instance of org.chromium.content.app.ChildProcessServiceImpl.
  base::android::ScopedJavaGlobalRef<jobject> service_impl_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureManagerImpl);
};

// Chrome actually uses the renderer code path for all of its child
// processes such as renderers, plugins, etc.
void InternalInitChildProcessImpl(JNIEnv* env,
                                  const JavaParamRef<jobject>& service_impl,
                                  jint cpu_count,
                                  jlong cpu_features) {
  // Set the CPU properties.
  android_setCpu(cpu_count, cpu_features);
  gpu::SurfaceTextureManager::SetInstance(
      new SurfaceTextureManagerImpl(service_impl));

  base::android::MemoryPressureListenerAndroid::RegisterSystemCallback(env);
}

}  // namespace <anonymous>

void RegisterGlobalFileDescriptor(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  jint id,
                                  jint fd,
                                  jlong offset,
                                  jlong size) {
  base::MemoryMappedFile::Region region = {offset, size};
  base::GlobalDescriptors::GetInstance()->Set(id, fd, region);
}

void InitChildProcessImpl(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jobject>& service_impl,
                          jint cpu_count,
                          jlong cpu_features) {
  InternalInitChildProcessImpl(env, service_impl, cpu_count, cpu_features);
}

void ExitChildProcess(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  VLOG(0) << "ChildProcessServiceImpl: Exiting child process.";
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterChildProcessServiceImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShutdownMainThread(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ChildThreadImpl::ShutdownThread();
}

}  // namespace content
