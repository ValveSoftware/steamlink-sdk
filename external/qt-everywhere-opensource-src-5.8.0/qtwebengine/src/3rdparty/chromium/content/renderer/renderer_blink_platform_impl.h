// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
#define CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "content/child/blink_platform_impl.h"
#include "content/common/content_export.h"
#include "content/renderer/origin_trials/web_trial_token_validator_impl.h"
#include "content/renderer/top_level_blame_context.h"
#include "content/renderer/webpublicsuffixlist_impl.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBFactory.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace cc {
class ContextProvider;
}

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
class WebCanvasCaptureHandler;
class WebDeviceMotionData;
class WebDeviceOrientationData;
class WebGraphicsContext3DProvider;
class WebMediaPlayer;
class WebMediaRecorderHandler;
class WebMediaStream;
class WebSecurityOrigin;
class WebServiceWorkerCacheStorage;
}

namespace scheduler {
class RendererScheduler;
class WebThreadImplForRendererScheduler;
}

namespace shell {
class InterfaceProvider;
}

namespace content {
class BlinkServiceRegistryImpl;
class DeviceLightEventPump;
class DeviceMotionEventPump;
class DeviceOrientationEventPump;
class LocalStorageCachedAreas;
class PlatformEventObserverBase;
class QuotaMessageFilter;
class RendererClipboardDelegate;
class RenderView;
class ThreadSafeSender;
class WebClipboardImpl;
class WebDatabaseObserverImpl;
class WebFileSystemImpl;

class CONTENT_EXPORT RendererBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  RendererBlinkPlatformImpl(
      scheduler::RendererScheduler* renderer_scheduler,
      base::WeakPtr<shell::InterfaceProvider> remote_interfaces);
  ~RendererBlinkPlatformImpl() override;

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  void set_plugin_refresh_allowed(bool plugin_refresh_allowed) {
    plugin_refresh_allowed_ = plugin_refresh_allowed;
  }
  // Platform methods:
  blink::WebClipboard* clipboard() override;
  blink::WebMimeRegistry* mimeRegistry() override;
  blink::WebFileUtilities* fileUtilities() override;
  blink::WebSandboxSupport* sandboxSupport() override;
  blink::WebCookieJar* cookieJar() override;
  blink::WebThemeEngine* themeEngine() override;
  blink::WebSpeechSynthesizer* createSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client) override;
  virtual bool sandboxEnabled();
  unsigned long long visitedLinkHash(const char* canonicalURL,
                                     size_t length) override;
  bool isLinkVisited(unsigned long long linkHash) override;
  void createMessageChannel(blink::WebMessagePortChannel** channel1,
                            blink::WebMessagePortChannel** channel2) override;
  blink::WebPrescientNetworking* prescientNetworking() override;
  void cacheMetadata(const blink::WebURL&,
                     int64_t,
                     const char*,
                     size_t) override;
  void cacheMetadataInCacheStorage(
      const blink::WebURL&,
      int64_t,
      const char*,
      size_t,
      const blink::WebSecurityOrigin& cacheStorageOrigin,
      const blink::WebString& cacheStorageCacheName) override;
  blink::WebString defaultLocale() override;
  void suddenTerminationChanged(bool enabled) override;
  blink::WebStorageNamespace* createLocalStorageNamespace() override;
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
  blink::WebString databaseCreateOriginIdentifier(
      const blink::WebSecurityOrigin& origin) override;

  blink::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index,
      const blink::WebString& challenge,
      const blink::WebURL& url,
      const blink::WebURL& top_origin) override;
  void getPluginList(bool refresh,
                     blink::WebPluginListBuilder* builder) override;
  blink::WebPublicSuffixList* publicSuffixList() override;
  blink::WebScrollbarBehavior* scrollbarBehavior() override;
  blink::WebIDBFactory* idbFactory() override;
  blink::WebServiceWorkerCacheStorage* cacheStorage(
      const blink::WebSecurityOrigin& security_origin) override;
  blink::WebFileSystem* fileSystem() override;
  blink::WebString fileSystemCreateOriginIdentifier(
      const blink::WebSecurityOrigin& origin) override;

  bool canAccelerate2dCanvas() override;
  bool isThreadedCompositingEnabled() override;
  bool isThreadedAnimationEnabled() override;
  double audioHardwareSampleRate() override;
  size_t audioHardwareBufferSize() override;
  unsigned audioHardwareOutputChannels() override;
  blink::WebDatabaseObserver* databaseObserver() override;

  blink::WebAudioDevice* createAudioDevice(
      size_t buffer_size,
      unsigned input_channels,
      unsigned channels,
      double sample_rate,
      blink::WebAudioDevice::RenderCallback* callback,
      const blink::WebString& input_device_id,
      const blink::WebSecurityOrigin& security_origin) override;

  bool loadAudioResource(blink::WebAudioBus* destination_bus,
                         const char* audio_file_data,
                         size_t data_size) override;

  blink::WebMIDIAccessor* createMIDIAccessor(
      blink::WebMIDIAccessorClient* client) override;

  blink::WebBlobRegistry* blobRegistry() override;
  void sampleGamepads(blink::WebGamepads&) override;
  blink::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client) override;
  blink::WebRTCCertificateGenerator* createRTCCertificateGenerator() override;
  blink::WebMediaRecorderHandler* createMediaRecorderHandler() override;
  blink::WebMediaStreamCenter* createMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client) override;
  blink::WebCanvasCaptureHandler* createCanvasCaptureHandler(
      const blink::WebSize& size,
      double frame_rate,
      blink::WebMediaStreamTrack* track) override;
  void createHTMLVideoElementCapturer(
      blink::WebMediaStream* web_media_stream,
      blink::WebMediaPlayer* web_media_player) override;
  void createHTMLAudioElementCapturer(
      blink::WebMediaStream* web_media_stream,
      blink::WebMediaPlayer* web_media_player) override;
  blink::WebImageCaptureFrameGrabber* createImageCaptureFrameGrabber() override;
  blink::WebGraphicsContext3DProvider* createOffscreenGraphicsContext3DProvider(
      const blink::Platform::ContextAttributes& attributes,
      const blink::WebURL& top_document_web_url,
      blink::WebGraphicsContext3DProvider* share_provider,
      blink::Platform::GraphicsInfo* gl_info) override;
  blink::WebGraphicsContext3DProvider*
  createSharedOffscreenGraphicsContext3DProvider() override;
  blink::WebCompositorSupport* compositorSupport() override;
  blink::WebString convertIDNToUnicode(const blink::WebString& host) override;
  blink::ServiceRegistry* serviceRegistry() override;
  void startListening(blink::WebPlatformEventType,
                      blink::WebPlatformEventListener*) override;
  void stopListening(blink::WebPlatformEventType) override;
  void queryStorageUsageAndQuota(const blink::WebURL& storage_partition,
                                 blink::WebStorageQuotaType,
                                 blink::WebStorageQuotaCallbacks) override;
  blink::WebThread* currentThread() override;
  blink::BlameContext* topLevelBlameContext() override;
  void recordRappor(const char* metric,
                    const blink::WebString& sample) override;
  void recordRapporURL(const char* metric, const blink::WebURL& url) override;

  blink::WebTrialTokenValidator* trialTokenValidator() override;
  void workerContextCreated(const v8::Local<v8::Context>& worker) override;

  // Set the PlatformEventObserverBase in |platform_event_observers_| associated
  // with |type| to |observer|. If there was already an observer associated to
  // the given |type|, it will be replaced.
  // Note that |observer| will be owned by this object after the call.
  void SetPlatformEventObserverForTesting(
      blink::WebPlatformEventType type,
      std::unique_ptr<PlatformEventObserverBase> observer);

  // Disables the WebSandboxSupport implementation for testing.
  // Tests that do not set up a full sandbox environment should call
  // SetSandboxEnabledForTesting(false) _before_ creating any instances
  // of this class, to ensure that we don't attempt to use sandbox-related
  // file descriptors or other resources.
  //
  // Returns the previous |enable| value.
  static bool SetSandboxEnabledForTesting(bool enable);

  //  Set a double to return when setDeviceLightListener is invoked.
  static void SetMockDeviceLightDataForTesting(double data);
  // Set WebDeviceMotionData to return when setDeviceMotionListener is invoked.
  static void SetMockDeviceMotionDataForTesting(
      const blink::WebDeviceMotionData& data);
  // Set WebDeviceOrientationData to return when setDeviceOrientationListener
  // is invoked.
  static void SetMockDeviceOrientationDataForTesting(
      const blink::WebDeviceOrientationData& data);

  WebDatabaseObserverImpl* web_database_observer_impl() {
    return web_database_observer_impl_.get();
  }

  blink::WebURLLoader* createURLLoader() override;

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  // Factory that takes a type and return PlatformEventObserverBase that matches
  // it.
  static PlatformEventObserverBase* CreatePlatformEventObserverFromType(
      blink::WebPlatformEventType type);

  // Use the data previously set via SetMockDevice...DataForTesting() and send
  // them to the registered listener.
  void SendFakeDeviceEventDataForTesting(blink::WebPlatformEventType type);

  std::unique_ptr<blink::WebThread> main_thread_;

  std::unique_ptr<RendererClipboardDelegate> clipboard_delegate_;
  std::unique_ptr<WebClipboardImpl> clipboard_;

  class FileUtilities;
  std::unique_ptr<FileUtilities> file_utilities_;

  class MimeRegistry;
  std::unique_ptr<MimeRegistry> mime_registry_;

#if !defined(OS_ANDROID) && !defined(OS_WIN)
  class SandboxSupport;
  std::unique_ptr<SandboxSupport> sandbox_support_;
#endif

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  std::unique_ptr<blink::WebIDBFactory> web_idb_factory_;

  std::unique_ptr<blink::WebBlobRegistry> blob_registry_;

  WebPublicSuffixListImpl public_suffix_list_;

  std::unique_ptr<DeviceLightEventPump> device_light_event_pump_;
  std::unique_ptr<DeviceMotionEventPump> device_motion_event_pump_;
  std::unique_ptr<DeviceOrientationEventPump> device_orientation_event_pump_;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  std::unique_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  cc_blink::WebCompositorSupportImpl compositor_support_;

  std::unique_ptr<blink::WebScrollbarBehavior> web_scrollbar_behavior_;

  IDMap<PlatformEventObserverBase, IDMapOwnPointer> platform_event_observers_;

  scheduler::RendererScheduler* renderer_scheduler_;  // NOT OWNED
  TopLevelBlameContext top_level_blame_context_;

  WebTrialTokenValidatorImpl trial_token_validator_;

  std::unique_ptr<LocalStorageCachedAreas> local_storage_cached_areas_;

  std::unique_ptr<BlinkServiceRegistryImpl> blink_service_registry_;

  DISALLOW_COPY_AND_ASSIGN(RendererBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
