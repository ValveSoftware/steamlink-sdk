// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/child_process_launcher_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "jni/ChildProcessLauncher_jni.h"
#include "media/base/android/media_player_android.h"
#include "ui/gl/android/scoped_java_surface.h"

using base::android::AttachCurrentThread;
using base::android::ToJavaArrayOfStrings;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::StartChildProcessCallback;

namespace content {

namespace {

// Pass a java surface object to the MediaPlayerAndroid object
// identified by render process handle, render frame ID and player ID.
static void SetSurfacePeer(
    const base::android::JavaRef<jobject>& surface,
    base::ProcessHandle render_process_handle,
    int render_frame_id,
    int player_id) {
  int render_process_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      render_process_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }
  if (!render_process_id) {
    DVLOG(1) << "Cannot find render process for render_process_handle "
             << render_process_handle;
    return;
  }

  RenderFrameHostImpl* frame =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!frame) {
    DVLOG(1) << "Cannot find frame for render_frame_id " << render_frame_id;
    return;
  }

  RenderViewHostImpl* view =
      static_cast<RenderViewHostImpl*>(frame->GetRenderViewHost());
  BrowserMediaPlayerManager* player_manager =
      view->media_web_contents_observer()->GetMediaPlayerManager(frame);
  if (!player_manager) {
    DVLOG(1) << "Cannot find the media player manager for frame " << frame;
    return;
  }

  media::MediaPlayerAndroid* player = player_manager->GetPlayer(player_id);
  if (!player) {
    DVLOG(1) << "Cannot find media player for player_id " << player_id;
    return;
  }

  if (player != player_manager->GetFullscreenPlayer()) {
    gfx::ScopedJavaSurface scoped_surface(surface);
    player->SetVideoSurface(scoped_surface.Pass());
  }
}

}  // anonymous namespace

// Called from ChildProcessLauncher.java when the ChildProcess was
// started.
// |client_context| is the pointer to StartChildProcessCallback which was
// passed in from StartChildProcess.
// |handle| is the processID of the child process as originated in Java, 0 if
// the ChildProcess could not be created.
static void OnChildProcessStarted(JNIEnv*,
                                  jclass,
                                  jlong client_context,
                                  jint handle) {
  StartChildProcessCallback* callback =
      reinterpret_cast<StartChildProcessCallback*>(client_context);
  if (handle)
    callback->Run(static_cast<base::ProcessHandle>(handle));
  delete callback;
}

void StartChildProcess(
    const CommandLine::StringVector& argv,
    int child_process_id,
    const std::vector<content::FileDescriptorInfo>& files_to_register,
    const StartChildProcessCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv = ToJavaArrayOfStrings(env, argv);

  size_t file_count = files_to_register.size();
  DCHECK(file_count > 0);

  ScopedJavaLocalRef<jintArray> j_file_ids(env, env->NewIntArray(file_count));
  base::android::CheckException(env);
  jint* file_ids = env->GetIntArrayElements(j_file_ids.obj(), NULL);
  base::android::CheckException(env);
  ScopedJavaLocalRef<jintArray> j_file_fds(env, env->NewIntArray(file_count));
  base::android::CheckException(env);
  jint* file_fds = env->GetIntArrayElements(j_file_fds.obj(), NULL);
  base::android::CheckException(env);
  ScopedJavaLocalRef<jbooleanArray> j_file_auto_close(
      env, env->NewBooleanArray(file_count));
  base::android::CheckException(env);
  jboolean* file_auto_close =
      env->GetBooleanArrayElements(j_file_auto_close.obj(), NULL);
  base::android::CheckException(env);
  for (size_t i = 0; i < file_count; ++i) {
    const content::FileDescriptorInfo& fd_info = files_to_register[i];
    file_ids[i] = fd_info.id;
    file_fds[i] = fd_info.fd.fd;
    file_auto_close[i] = fd_info.fd.auto_close;
  }
  env->ReleaseIntArrayElements(j_file_ids.obj(), file_ids, 0);
  env->ReleaseIntArrayElements(j_file_fds.obj(), file_fds, 0);
  env->ReleaseBooleanArrayElements(j_file_auto_close.obj(), file_auto_close, 0);

  Java_ChildProcessLauncher_start(env,
      base::android::GetApplicationContext(),
      j_argv.obj(),
      child_process_id,
      j_file_ids.obj(),
      j_file_fds.obj(),
      j_file_auto_close.obj(),
      reinterpret_cast<intptr_t>(new StartChildProcessCallback(callback)));
}

void StopChildProcess(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_stop(env, static_cast<jint>(handle));
}

bool IsChildProcessOomProtected(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_isOomProtected(env,
      static_cast<jint>(handle));
}

void SetChildProcessInForeground(base::ProcessHandle handle,
    bool in_foreground) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_setInForeground(env,
      static_cast<jint>(handle), static_cast<jboolean>(in_foreground));
}

void EstablishSurfacePeer(
    JNIEnv* env, jclass clazz,
    jint pid, jobject surface, jint primary_id, jint secondary_id) {
  ScopedJavaGlobalRef<jobject> jsurface;
  jsurface.Reset(env, surface);
  if (jsurface.is_null())
    return;

  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &SetSurfacePeer, jsurface, pid, primary_id, secondary_id));
}

void RegisterViewSurface(int surface_id, jobject j_surface) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_registerViewSurface(env, surface_id, j_surface);
}

void UnregisterViewSurface(int surface_id) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_unregisterViewSurface(env, surface_id);
}

void RegisterChildProcessSurfaceTexture(int surface_texture_id,
                                        int child_process_id,
                                        jobject j_surface_texture) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_registerSurfaceTexture(
      env, surface_texture_id, child_process_id, j_surface_texture);
}

void UnregisterChildProcessSurfaceTexture(int surface_texture_id,
                                          int child_process_id) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_unregisterSurfaceTexture(
      env, surface_texture_id, child_process_id);
}

jboolean IsSingleProcess(JNIEnv* env, jclass clazz) {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
}

bool RegisterChildProcessLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
