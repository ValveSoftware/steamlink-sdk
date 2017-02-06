// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
#define CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "content/child/webfallbackthemeengine_impl.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "ui/base/layout.h"

#if defined(USE_DEFAULT_RENDER_THEME)
#include "content/child/webthemeengine_impl_default.h"
#elif defined(OS_WIN)
#include "content/child/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#elif defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#endif

namespace base {
class MessageLoop;
class WaitableEvent;
}

namespace scheduler {
class WebThreadBase;
}

namespace content {
class BackgroundSyncProvider;
class FlingCurveConfiguration;
class NotificationDispatcher;
class PermissionDispatcher;
class PushDispatcher;
class ThreadSafeSender;
class TraceLogObserverAdapter;
class WebCryptoImpl;

class CONTENT_EXPORT BlinkPlatformImpl
    : NON_EXPORTED_BASE(public blink::Platform) {
 public:
  BlinkPlatformImpl();
  explicit BlinkPlatformImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~BlinkPlatformImpl() override;

  // Platform methods (partial implementation):
  blink::WebThemeEngine* themeEngine() override;
  blink::WebFallbackThemeEngine* fallbackThemeEngine() override;
  blink::Platform::FileHandle databaseOpenFile(
      const blink::WebString& vfs_file_name,
      int desired_flags) override;
  int databaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;
  long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name) override;
  long long databaseGetFileSize(const blink::WebString& vfs_file_name) override;
  long long databaseGetSpaceAvailableForOrigin(
      const blink::WebSecurityOrigin& origin) override;
  bool databaseSetFileSize(const blink::WebString& vfs_file_name,
                           long long size) override;
  blink::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index,
      const blink::WebString& challenge,
      const blink::WebURL& url,
      const blink::WebURL& top_origin) override;
  size_t actualMemoryUsageMB() override;
  size_t numberOfProcessors() override;

  size_t maxDecodedImageBytes() override;
  uint32_t getUniqueIdForProcess() override;
  blink::WebSocketHandle* createWebSocketHandle() override;
  blink::WebString userAgent() override;
  blink::WebData parseDataURL(const blink::WebURL& url,
                              blink::WebString& mimetype,
                              blink::WebString& charset) override;
  blink::WebURLError cancelledError(const blink::WebURL& url) const override;
  bool parseMultipartHeadersFromBody(const char* bytes,
                                     size_t size,
                                     blink::WebURLResponse* response,
                                     size_t* end) const override;
  blink::WebThread* createThread(const char* name) override;
  blink::WebThread* currentThread() override;
  void recordAction(const blink::UserMetricsAction&) override;
  void addTraceLogEnabledStateObserver(
      blink::Platform::TraceLogEnabledStateObserver* observer) override;
  void removeTraceLogEnabledStateObserver(
      blink::Platform::TraceLogEnabledStateObserver* observer) override;

  blink::WebData loadResource(const char* name) override;
  blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name) override;
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      int numeric_value);
  blink::WebString queryLocalizedString(blink::WebLocalizedString::Name name,
                                        const blink::WebString& value) override;
  blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  void suddenTerminationChanged(bool enabled) override {}
  blink::WebThread* compositorThread() const override;
  blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;
  void didStartWorkerThread() override;
  void willStopWorkerThread() override;
  bool allowScriptExtensionForServiceWorker(
      const blink::WebURL& script_url) override;
  blink::WebCrypto* crypto() override;
  blink::WebNotificationManager* notificationManager() override;
  blink::WebPushProvider* pushProvider() override;
  blink::WebPermissionClient* permissionClient() override;
  blink::WebSyncProvider* backgroundSyncProvider() override;

  blink::WebString domCodeStringFromEnum(int dom_code) override;
  int domEnumFromCodeString(const blink::WebString& codeString) override;
  blink::WebString domKeyStringFromEnum(int dom_key) override;
  int domKeyEnumFromString(const blink::WebString& key_string) override;

  // This class does *not* own the compositor thread. It is the responsibility
  // of the caller to ensure that the compositor thread is cleared before it is
  // destructed.
  void SetCompositorThread(scheduler::WebThreadBase* compositor_thread);

 private:
  void InternalInit();
  void WaitUntilWebThreadTLSUpdate(scheduler::WebThreadBase* thread);
  void UpdateWebThreadTLS(blink::WebThread* thread, base::WaitableEvent* event);

  bool IsMainThread() const;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  WebThemeEngineImpl native_theme_engine_;
  WebFallbackThemeEngineImpl fallback_theme_engine_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  webcrypto::WebCryptoImpl web_crypto_;
  base::ScopedPtrHashMap<blink::Platform::TraceLogEnabledStateObserver*,
                         std::unique_ptr<TraceLogObserverAdapter>>
      trace_log_observers_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;
  scoped_refptr<PushDispatcher> push_dispatcher_;
  std::unique_ptr<PermissionDispatcher> permission_client_;
  std::unique_ptr<BackgroundSyncProvider> main_thread_sync_provider_;

  scheduler::WebThreadBase* compositor_thread_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
