// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webkitplatformsupport_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/blink_glue.h"
#include "content/child/database_util.h"
#include "content/child/fileapi/webfilesystem_impl.h"
#include "content/child/indexed_db/webidbfactory_impl.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/quota_message_filter.h"
#include "content/child/simple_webmimeregistry_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_database_observer_impl.h"
#include "content/child/webblobregistry_impl.h"
#include "content/child/webfileutilities_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/file_utilities_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/mime_registry_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/battery_status/battery_status_dispatcher.h"
#include "content/renderer/battery_status/fake_battery_status_dispatcher.h"
#include "content/renderer/device_sensors/device_motion_event_pump.h"
#include "content/renderer/device_sensors/device_orientation_event_pump.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/gamepad_shared_memory_reader.h"
#include "content/renderer/media/audio_decoder.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/renderer_webaudiodevice_impl.h"
#include "content/renderer/media/renderer_webmidiaccessor_impl.h"
#include "content/renderer/media/webcontentdecryptionmodule_impl.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_clipboard_client.h"
#include "content/renderer/screen_orientation/mock_screen_orientation_controller.h"
#include "content/renderer/webclipboard_impl.h"
#include "content/renderer/webgraphicscontext3d_provider_impl.h"
#include "content/renderer/webpublicsuffixlist_impl.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_hardware_config.h"
#include "media/filters/stream_parser_factory.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionListener.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationListener.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebPluginListBuilder.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "ui/gfx/color_profile.h"
#include "url/gurl.h"
#include "webkit/common/gpu/context_provider_web_context.h"
#include "webkit/common/quota/quota_types.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/audio_decoder_android.h"
#endif

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#include "content/common/mac/font_loader.h"
#include "content/renderer/webscrollbarbehavior_impl_mac.h"
#include "third_party/WebKit/public/platform/mac/WebSandboxSupport.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "third_party/WebKit/public/platform/linux/WebFallbackFont.h"
#include "third_party/WebKit/public/platform/linux/WebSandboxSupport.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#endif
#endif

#if defined(OS_WIN)
#include "content/common/child_process_messages.h"
#include "third_party/WebKit/public/platform/win/WebSandboxSupport.h"
#endif

#if defined(USE_AURA)
#include "content/renderer/webscrollbarbehavior_impl_gtkoraura.h"
#elif !defined(OS_MACOSX)
#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"
#define WebScrollbarBehaviorImpl blink::WebScrollbarBehavior
#endif

using blink::Platform;
using blink::WebAudioDevice;
using blink::WebBlobRegistry;
using blink::WebDatabaseObserver;
using blink::WebFileInfo;
using blink::WebFileSystem;
using blink::WebGamepad;
using blink::WebGamepads;
using blink::WebIDBFactory;
using blink::WebMIDIAccessor;
using blink::WebMediaStreamCenter;
using blink::WebMediaStreamCenterClient;
using blink::WebMimeRegistry;
using blink::WebRTCPeerConnectionHandler;
using blink::WebRTCPeerConnectionHandlerClient;
using blink::WebStorageNamespace;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

namespace {

static bool g_sandbox_enabled = true;
base::LazyInstance<blink::WebDeviceMotionData>::Leaky
    g_test_device_motion_data = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<blink::WebDeviceOrientationData>::Leaky
    g_test_device_orientation_data = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<MockScreenOrientationController>::Leaky
    g_test_screen_orientation_controller = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<FakeBatteryStatusDispatcher>::Leaky
    g_test_battery_status_dispatcher = LAZY_INSTANCE_INITIALIZER;

} // namespace

//------------------------------------------------------------------------------

class RendererWebKitPlatformSupportImpl::MimeRegistry
    : public SimpleWebMimeRegistryImpl {
 public:
  virtual blink::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const blink::WebString& mime_type,
      const blink::WebString& codecs,
      const blink::WebString& key_system);
  virtual bool supportsMediaSourceMIMEType(const blink::WebString& mime_type,
                                           const blink::WebString& codecs);
  virtual bool supportsEncryptedMediaMIMEType(const WebString& key_system,
                                              const WebString& mime_type,
                                              const WebString& codecs) OVERRIDE;
  virtual blink::WebString mimeTypeForExtension(
      const blink::WebString& file_extension);
  virtual blink::WebString mimeTypeFromFile(
      const blink::WebString& file_path);
};

class RendererWebKitPlatformSupportImpl::FileUtilities
    : public WebFileUtilitiesImpl {
 public:
  explicit FileUtilities(ThreadSafeSender* sender)
      : thread_safe_sender_(sender) {}
  virtual bool getFileInfo(const WebString& path, WebFileInfo& result);
 private:
  bool SendSyncMessageFromAnyThread(IPC::SyncMessage* msg) const;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

#if defined(OS_ANDROID)
// WebKit doesn't use WebSandboxSupport on android so we don't need to
// implement anything here.
class RendererWebKitPlatformSupportImpl::SandboxSupport {
};
#else
class RendererWebKitPlatformSupportImpl::SandboxSupport
    : public blink::WebSandboxSupport {
 public:
  virtual ~SandboxSupport() {}

#if defined(OS_WIN)
  virtual bool ensureFontLoaded(HFONT);
#elif defined(OS_MACOSX)
  virtual bool loadFont(
      NSFont* src_font,
      CGFontRef* container,
      uint32* font_id);
#elif defined(OS_POSIX)
  virtual void getFallbackFontForCharacter(
      blink::WebUChar32 character,
      const char* preferred_locale,
      blink::WebFallbackFont* fallbackFont);
  virtual void getRenderStyleForStrike(
      const char* family, int sizeAndStyle, blink::WebFontRenderStyle* out);

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here.
  base::Lock unicode_font_families_mutex_;
  std::map<int32_t, blink::WebFallbackFont> unicode_font_families_;
#endif
};
#endif  // defined(OS_ANDROID)

//------------------------------------------------------------------------------

RendererWebKitPlatformSupportImpl::RendererWebKitPlatformSupportImpl()
    : clipboard_client_(new RendererClipboardClient),
      clipboard_(new WebClipboardImpl(clipboard_client_.get())),
      mime_registry_(new RendererWebKitPlatformSupportImpl::MimeRegistry),
      sudden_termination_disables_(0),
      plugin_refresh_allowed_(true),
      child_thread_loop_(base::MessageLoopProxy::current()),
      web_scrollbar_behavior_(new WebScrollbarBehaviorImpl),
      gamepad_provider_(NULL) {
  if (g_sandbox_enabled && sandboxEnabled()) {
    sandbox_support_.reset(
        new RendererWebKitPlatformSupportImpl::SandboxSupport);
  } else {
    DVLOG(1) << "Disabling sandbox support for testing.";
  }

  // ChildThread may not exist in some tests.
  if (ChildThread::current()) {
    sync_message_filter_ = ChildThread::current()->sync_message_filter();
    thread_safe_sender_ = ChildThread::current()->thread_safe_sender();
    quota_message_filter_ = ChildThread::current()->quota_message_filter();
    blob_registry_.reset(new WebBlobRegistryImpl(thread_safe_sender_));
    web_idb_factory_.reset(new WebIDBFactoryImpl(thread_safe_sender_));
    web_database_observer_impl_.reset(
        new WebDatabaseObserverImpl(sync_message_filter_));
  }
}

RendererWebKitPlatformSupportImpl::~RendererWebKitPlatformSupportImpl() {
  WebFileSystemImpl::DeleteThreadSpecificInstance();
}

//------------------------------------------------------------------------------

blink::WebClipboard* RendererWebKitPlatformSupportImpl::clipboard() {
  blink::WebClipboard* clipboard =
      GetContentClient()->renderer()->OverrideWebClipboard();
  if (clipboard)
    return clipboard;
  return clipboard_.get();
}

blink::WebMimeRegistry* RendererWebKitPlatformSupportImpl::mimeRegistry() {
  return mime_registry_.get();
}

blink::WebFileUtilities*
RendererWebKitPlatformSupportImpl::fileUtilities() {
  if (!file_utilities_) {
    file_utilities_.reset(new FileUtilities(thread_safe_sender_.get()));
    file_utilities_->set_sandbox_enabled(sandboxEnabled());
  }
  return file_utilities_.get();
}

blink::WebSandboxSupport* RendererWebKitPlatformSupportImpl::sandboxSupport() {
#if defined(OS_ANDROID)
  // WebKit doesn't use WebSandboxSupport on android.
  return NULL;
#else
  return sandbox_support_.get();
#endif
}

blink::WebCookieJar* RendererWebKitPlatformSupportImpl::cookieJar() {
  NOTREACHED() << "Use WebFrameClient::cookieJar() instead!";
  return NULL;
}

blink::WebThemeEngine* RendererWebKitPlatformSupportImpl::themeEngine() {
  blink::WebThemeEngine* theme_engine =
      GetContentClient()->renderer()->OverrideThemeEngine();
  if (theme_engine)
    return theme_engine;
  return BlinkPlatformImpl::themeEngine();
}

bool RendererWebKitPlatformSupportImpl::sandboxEnabled() {
  // As explained in Platform.h, this function is used to decide
  // whether to allow file system operations to come out of WebKit or not.
  // Even if the sandbox is disabled, there's no reason why the code should
  // act any differently...unless we're in single process mode.  In which
  // case, we have no other choice.  Platform.h discourages using
  // this switch unless absolutely necessary, so hopefully we won't end up
  // with too many code paths being different in single-process mode.
  return !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
}

unsigned long long RendererWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return GetContentClient()->renderer()->VisitedLinkHash(canonical_url, length);
}

bool RendererWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  return GetContentClient()->renderer()->IsLinkVisited(link_hash);
}

void RendererWebKitPlatformSupportImpl::createMessageChannel(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  WebMessagePortChannelImpl::CreatePair(
      child_thread_loop_.get(), channel1, channel2);
}

blink::WebPrescientNetworking*
RendererWebKitPlatformSupportImpl::prescientNetworking() {
  return GetContentClient()->renderer()->GetPrescientNetworking();
}

bool
RendererWebKitPlatformSupportImpl::CheckPreparsedJsCachingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnablePreparsedJsCaching);
    checked = true;
  }
  return result;
}

void RendererWebKitPlatformSupportImpl::cacheMetadata(
    const blink::WebURL& url,
    double response_time,
    const char* data,
    size_t size) {
  if (!CheckPreparsedJsCachingEnabled())
    return;

  // Let the browser know we generated cacheable metadata for this resource. The
  // browser may cache it and return it on subsequent responses to speed
  // the processing of this resource.
  std::vector<char> copy(data, data + size);
  RenderThread::Get()->Send(
      new ViewHostMsg_DidGenerateCacheableMetadata(url, response_time, copy));
}

WebString RendererWebKitPlatformSupportImpl::defaultLocale() {
  return base::ASCIIToUTF16(RenderThread::Get()->GetLocale());
}

void RendererWebKitPlatformSupportImpl::suddenTerminationChanged(bool enabled) {
  if (enabled) {
    // We should not get more enables than disables, but we want it to be a
    // non-fatal error if it does happen.
    DCHECK_GT(sudden_termination_disables_, 0);
    sudden_termination_disables_ = std::max(sudden_termination_disables_ - 1,
                                            0);
    if (sudden_termination_disables_ != 0)
      return;
  } else {
    sudden_termination_disables_++;
    if (sudden_termination_disables_ != 1)
      return;
  }

  RenderThread* thread = RenderThread::Get();
  if (thread)  // NULL in unittests.
    thread->Send(new ViewHostMsg_SuddenTerminationChanged(enabled));
}

WebStorageNamespace*
RendererWebKitPlatformSupportImpl::createLocalStorageNamespace() {
  return new WebStorageNamespaceImpl();
}


//------------------------------------------------------------------------------

WebIDBFactory* RendererWebKitPlatformSupportImpl::idbFactory() {
  return web_idb_factory_.get();
}

//------------------------------------------------------------------------------

WebFileSystem* RendererWebKitPlatformSupportImpl::fileSystem() {
  return WebFileSystemImpl::ThreadSpecificInstance(child_thread_loop_.get());
}

//------------------------------------------------------------------------------

WebMimeRegistry::SupportsType
RendererWebKitPlatformSupportImpl::MimeRegistry::supportsMediaMIMEType(
    const WebString& mime_type,
    const WebString& codecs,
    const WebString& key_system) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  // Not supporting the container is a flat-out no.
  if (!net::IsSupportedMediaMimeType(mime_type_ascii))
    return IsNotSupported;

  if (!key_system.isEmpty()) {
    // Check whether the key system is supported with the mime_type and codecs.

    // Chromium only supports ASCII parameters.
    if (!base::IsStringASCII(key_system))
      return IsNotSupported;

    std::string key_system_ascii =
        GetUnprefixedKeySystemName(base::UTF16ToASCII(key_system));
    std::vector<std::string> strict_codecs;
    bool strip_suffix = !net::IsStrictMediaMimeType(mime_type_ascii);
    net::ParseCodecString(ToASCIIOrEmpty(codecs), &strict_codecs, strip_suffix);

    if (!IsSupportedKeySystemWithMediaMimeType(
            mime_type_ascii, strict_codecs, key_system_ascii)) {
      return IsNotSupported;
    }

    // Continue processing the mime_type and codecs.
  }

  // Check list of strict codecs to see if it is supported.
  if (net::IsStrictMediaMimeType(mime_type_ascii)) {
    // Check if the codecs are a perfect match.
    std::vector<std::string> strict_codecs;
    net::ParseCodecString(ToASCIIOrEmpty(codecs), &strict_codecs, false);
    if (net::IsSupportedStrictMediaMimeType(mime_type_ascii, strict_codecs))
      return IsSupported;

    // We support the container, but no codecs were specified.
    if (codecs.isNull())
      return MayBeSupported;

    return IsNotSupported;
  }

  // If we don't recognize the codec, it's possible we support it.
  std::vector<std::string> parsed_codecs;
  net::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codecs, true);
  if (!net::AreSupportedMediaCodecs(parsed_codecs))
    return MayBeSupported;

  // Otherwise we have a perfect match.
  return IsSupported;
}

bool
RendererWebKitPlatformSupportImpl::MimeRegistry::supportsMediaSourceMIMEType(
    const blink::WebString& mime_type,
    const WebString& codecs) {
  const std::string mime_type_ascii = ToASCIIOrEmpty(mime_type);
  std::vector<std::string> parsed_codec_ids;
  net::ParseCodecString(ToASCIIOrEmpty(codecs), &parsed_codec_ids, false);
  if (mime_type_ascii.empty())
    return false;
  return media::StreamParserFactory::IsTypeSupported(
      mime_type_ascii, parsed_codec_ids);
}

bool
RendererWebKitPlatformSupportImpl::MimeRegistry::supportsEncryptedMediaMIMEType(
    const WebString& key_system,
    const WebString& mime_type,
    const WebString& codecs) {
  // Chromium only supports ASCII parameters.
  if (!base::IsStringASCII(key_system) || !base::IsStringASCII(mime_type) ||
      !base::IsStringASCII(codecs)) {
    return false;
  }

  if (key_system.isEmpty())
    return false;

  const std::string mime_type_ascii = base::UTF16ToASCII(mime_type);

  std::vector<std::string> codec_vector;
  bool strip_suffix = !net::IsStrictMediaMimeType(mime_type_ascii);
  net::ParseCodecString(base::UTF16ToASCII(codecs), &codec_vector,
                        strip_suffix);

  return IsSupportedKeySystemWithMediaMimeType(
      mime_type_ascii, codec_vector, base::UTF16ToASCII(key_system));
}

WebString
RendererWebKitPlatformSupportImpl::MimeRegistry::mimeTypeForExtension(
    const WebString& file_extension) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeForExtension(file_extension);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::Get()->Send(
      new MimeRegistryMsg_GetMimeTypeFromExtension(
          base::FilePath::FromUTF16Unsafe(file_extension).value(), &mime_type));
  return base::ASCIIToUTF16(mime_type);
}

WebString RendererWebKitPlatformSupportImpl::MimeRegistry::mimeTypeFromFile(
    const WebString& file_path) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeFromFile(file_path);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::Get()->Send(new MimeRegistryMsg_GetMimeTypeFromFile(
      base::FilePath::FromUTF16Unsafe(file_path),
      &mime_type));
  return base::ASCIIToUTF16(mime_type);
}

//------------------------------------------------------------------------------

bool RendererWebKitPlatformSupportImpl::FileUtilities::getFileInfo(
    const WebString& path,
    WebFileInfo& web_file_info) {
  base::File::Info file_info;
  base::File::Error status;
  if (!SendSyncMessageFromAnyThread(new FileUtilitiesMsg_GetFileInfo(
           base::FilePath::FromUTF16Unsafe(path), &file_info, &status)) ||
      status != base::File::FILE_OK) {
    return false;
  }
  FileInfoToWebFileInfo(file_info, &web_file_info);
  web_file_info.platformPath = path;
  return true;
}

bool RendererWebKitPlatformSupportImpl::FileUtilities::
SendSyncMessageFromAnyThread(IPC::SyncMessage* msg) const {
  base::TimeTicks begin = base::TimeTicks::Now();
  const bool success = thread_safe_sender_->Send(msg);
  base::TimeDelta delta = base::TimeTicks::Now() - begin;
  UMA_HISTOGRAM_TIMES("RendererSyncIPC.ElapsedTime", delta);
  return success;
}

//------------------------------------------------------------------------------

#if defined(OS_WIN)

bool RendererWebKitPlatformSupportImpl::SandboxSupport::ensureFontLoaded(
    HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  RenderThread::Get()->PreCacheFont(logfont);
  return true;
}

#elif defined(OS_MACOSX)

bool RendererWebKitPlatformSupportImpl::SandboxSupport::loadFont(
    NSFont* src_font, CGFontRef* out, uint32* font_id) {
  uint32 font_data_size;
  FontDescriptor src_font_descriptor(src_font);
  base::SharedMemoryHandle font_data;
  if (!RenderThread::Get()->Send(new ViewHostMsg_LoadFont(
        src_font_descriptor, &font_data_size, &font_data, font_id))) {
    *out = NULL;
    *font_id = 0;
    return false;
  }

  if (font_data_size == 0 || font_data == base::SharedMemory::NULLHandle() ||
      *font_id == 0) {
    LOG(ERROR) << "Bad response from ViewHostMsg_LoadFont() for " <<
        src_font_descriptor.font_name;
    *out = NULL;
    *font_id = 0;
    return false;
  }

  // TODO(jeremy): Need to call back into WebKit to make sure that the font
  // isn't already activated, based on the font id.  If it's already
  // activated, don't reactivate it here - crbug.com/72727 .

  return FontLoader::CGFontRefFromBuffer(font_data, font_data_size, out);
}

#elif defined(OS_ANDROID)

// WebKit doesn't use WebSandboxSupport on android so we don't need to
// implement anything here. This is cleaner to support than excluding the
// whole class for android.

#elif defined(OS_POSIX)

void
RendererWebKitPlatformSupportImpl::SandboxSupport::getFallbackFontForCharacter(
    blink::WebUChar32 character,
    const char* preferred_locale,
    blink::WebFallbackFont* fallbackFont) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const std::map<int32_t, blink::WebFallbackFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallbackFont->name = iter->second.name;
    fallbackFont->filename = iter->second.filename;
    fallbackFont->ttcIndex = iter->second.ttcIndex;
    fallbackFont->isBold = iter->second.isBold;
    fallbackFont->isItalic = iter->second.isItalic;
    return;
  }

  GetFallbackFontForCharacter(character, preferred_locale, fallbackFont);
  unicode_font_families_.insert(std::make_pair(character, *fallbackFont));
}

void
RendererWebKitPlatformSupportImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

//------------------------------------------------------------------------------

Platform::FileHandle
RendererWebKitPlatformSupportImpl::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags) {
  return DatabaseUtil::DatabaseOpenFile(
      vfs_file_name, desired_flags, sync_message_filter_.get());
}

int RendererWebKitPlatformSupportImpl::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  return DatabaseUtil::DatabaseDeleteFile(
      vfs_file_name, sync_dir, sync_message_filter_.get());
}

long RendererWebKitPlatformSupportImpl::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileAttributes(vfs_file_name,
                                                 sync_message_filter_.get());
}

long long RendererWebKitPlatformSupportImpl::databaseGetFileSize(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileSize(vfs_file_name,
                                           sync_message_filter_.get());
}

long long RendererWebKitPlatformSupportImpl::databaseGetSpaceAvailableForOrigin(
    const WebString& origin_identifier) {
  return DatabaseUtil::DatabaseGetSpaceAvailable(origin_identifier,
                                                 sync_message_filter_.get());
}

bool RendererWebKitPlatformSupportImpl::canAccelerate2dCanvas() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  GpuChannelHost* host = thread->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_CANVAS_2D);
  if (!host)
    return false;

  return host->gpu_info().SupportsAccelerated2dCanvas();
}

bool RendererWebKitPlatformSupportImpl::isThreadedCompositingEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  // thread can be NULL in tests.
  return thread && thread->compositor_message_loop_proxy().get();
}

double RendererWebKitPlatformSupportImpl::audioHardwareSampleRate() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputSampleRate();
}

size_t RendererWebKitPlatformSupportImpl::audioHardwareBufferSize() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputBufferSize();
}

unsigned RendererWebKitPlatformSupportImpl::audioHardwareOutputChannels() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputChannels();
}

WebDatabaseObserver* RendererWebKitPlatformSupportImpl::databaseObserver() {
  return web_database_observer_impl_.get();
}

WebAudioDevice*
RendererWebKitPlatformSupportImpl::createAudioDevice(
    size_t buffer_size,
    unsigned input_channels,
    unsigned channels,
    double sample_rate,
    WebAudioDevice::RenderCallback* callback,
    const blink::WebString& input_device_id) {
  // Use a mock for testing.
  blink::WebAudioDevice* mock_device =
      GetContentClient()->renderer()->OverrideCreateAudioDevice(sample_rate);
  if (mock_device)
    return mock_device;

  // The |channels| does not exactly identify the channel layout of the
  // device. The switch statement below assigns a best guess to the channel
  // layout based on number of channels.
  // TODO(crogers): WebKit should give the channel layout instead of the hard
  // channel count.
  media::ChannelLayout layout = media::CHANNEL_LAYOUT_UNSUPPORTED;
  switch (channels) {
    case 1:
      layout = media::CHANNEL_LAYOUT_MONO;
      break;
    case 2:
      layout = media::CHANNEL_LAYOUT_STEREO;
      break;
    case 3:
      layout = media::CHANNEL_LAYOUT_2_1;
      break;
    case 4:
      layout = media::CHANNEL_LAYOUT_4_0;
      break;
    case 5:
      layout = media::CHANNEL_LAYOUT_5_0;
      break;
    case 6:
      layout = media::CHANNEL_LAYOUT_5_1;
      break;
    case 7:
      layout = media::CHANNEL_LAYOUT_7_0;
      break;
    case 8:
      layout = media::CHANNEL_LAYOUT_7_1;
      break;
    default:
      layout = media::CHANNEL_LAYOUT_STEREO;
  }

  int session_id = 0;
  if (input_device_id.isNull() ||
      !base::StringToInt(base::UTF16ToUTF8(input_device_id), &session_id)) {
    if (input_channels > 0)
      DLOG(WARNING) << "createAudioDevice(): request for audio input ignored";

    input_channels = 0;
  }

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      layout, input_channels,
      static_cast<int>(sample_rate), 16, buffer_size,
      media::AudioParameters::NO_EFFECTS);

  return new RendererWebAudioDeviceImpl(params, callback, session_id);
}

#if defined(OS_ANDROID)
bool RendererWebKitPlatformSupportImpl::loadAudioResource(
    blink::WebAudioBus* destination_bus, const char* audio_file_data,
    size_t data_size) {
  return DecodeAudioFileData(destination_bus,
                             audio_file_data,
                             data_size,
                             thread_safe_sender_);
}
#else
bool RendererWebKitPlatformSupportImpl::loadAudioResource(
    blink::WebAudioBus* destination_bus, const char* audio_file_data,
    size_t data_size) {
  return DecodeAudioFileData(
      destination_bus, audio_file_data, data_size);
}
#endif  // defined(OS_ANDROID)

//------------------------------------------------------------------------------

blink::WebMIDIAccessor*
RendererWebKitPlatformSupportImpl::createMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  blink::WebMIDIAccessor* accessor =
      GetContentClient()->renderer()->OverrideCreateMIDIAccessor(client);
  if (accessor)
    return accessor;

  return new RendererWebMIDIAccessorImpl(client);
}

void RendererWebKitPlatformSupportImpl::getPluginList(
    bool refresh,
    blink::WebPluginListBuilder* builder) {
#if defined(ENABLE_PLUGINS)
  std::vector<WebPluginInfo> plugins;
  if (!plugin_refresh_allowed_)
    refresh = false;
  RenderThread::Get()->Send(
      new ViewHostMsg_GetPlugins(refresh, &plugins));
  for (size_t i = 0; i < plugins.size(); ++i) {
    const WebPluginInfo& plugin = plugins[i];

    builder->addPlugin(
        plugin.name, plugin.desc,
        plugin.path.BaseName().AsUTF16Unsafe());

    for (size_t j = 0; j < plugin.mime_types.size(); ++j) {
      const WebPluginMimeType& mime_type = plugin.mime_types[j];

      builder->addMediaTypeToLastPlugin(
          WebString::fromUTF8(mime_type.mime_type), mime_type.description);

      for (size_t k = 0; k < mime_type.file_extensions.size(); ++k) {
        builder->addFileExtensionToLastMediaType(
            WebString::fromUTF8(mime_type.file_extensions[k]));
      }
    }
  }
#endif
}

//------------------------------------------------------------------------------

blink::WebPublicSuffixList*
RendererWebKitPlatformSupportImpl::publicSuffixList() {
  return &public_suffix_list_;
}

//------------------------------------------------------------------------------

blink::WebString
RendererWebKitPlatformSupportImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const blink::WebString& challenge,
    const blink::WebURL& url) {
  std::string signed_public_key;
  RenderThread::Get()->Send(new ViewHostMsg_Keygen(
      static_cast<uint32>(key_size_index),
      challenge.utf8(),
      GURL(url),
      &signed_public_key));
  return WebString::fromUTF8(signed_public_key);
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::screenColorProfile(
    WebVector<char>* to_profile) {
#if defined(OS_WIN)
  // On Windows screen color profile is only available in the browser.
  std::vector<char> profile;
  // This Send() can be called from any impl-side thread. Use a thread
  // safe send to avoid crashing trying to access RenderThread::Get(),
  // which is not accessible from arbitrary threads.
  thread_safe_sender_->Send(
      new ViewHostMsg_GetMonitorColorProfile(&profile));
  *to_profile = profile;
#else
  // On other platforms, the primary monitor color profile can be read
  // directly.
  gfx::ColorProfile profile;
  *to_profile = profile.profile();
#endif
}

//------------------------------------------------------------------------------

blink::WebScrollbarBehavior*
    RendererWebKitPlatformSupportImpl::scrollbarBehavior() {
  return web_scrollbar_behavior_.get();
}

//------------------------------------------------------------------------------

WebBlobRegistry* RendererWebKitPlatformSupportImpl::blobRegistry() {
  // blob_registry_ can be NULL when running some tests.
  return blob_registry_.get();
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::sampleGamepads(WebGamepads& gamepads) {
  DCHECK(gamepad_provider_);
  gamepad_provider_->SampleGamepads(gamepads);
}

void RendererWebKitPlatformSupportImpl::setGamepadListener(
      blink::WebGamepadListener* listener) {
  DCHECK(gamepad_provider_);
  gamepad_provider_->SetGamepadListener(listener);
}

//------------------------------------------------------------------------------

WebRTCPeerConnectionHandler*
RendererWebKitPlatformSupportImpl::createRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return NULL;

#if defined(ENABLE_WEBRTC)
  WebRTCPeerConnectionHandler* peer_connection_handler =
      GetContentClient()->renderer()->OverrideCreateWebRTCPeerConnectionHandler(
          client);
  if (peer_connection_handler)
    return peer_connection_handler;

  PeerConnectionDependencyFactory* rtc_dependency_factory =
      render_thread->GetPeerConnectionDependencyFactory();
  return rtc_dependency_factory->CreateRTCPeerConnectionHandler(client);
#else
  return NULL;
#endif  // defined(ENABLE_WEBRTC)
}

//------------------------------------------------------------------------------

WebMediaStreamCenter*
RendererWebKitPlatformSupportImpl::createMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread);
  if (!render_thread)
    return NULL;
  return render_thread->CreateMediaStreamCenter(client);
}

// static
bool RendererWebKitPlatformSupportImpl::SetSandboxEnabledForTesting(
    bool enable) {
  bool was_enabled = g_sandbox_enabled;
  g_sandbox_enabled = enable;
  return was_enabled;
}

//------------------------------------------------------------------------------

blink::WebSpeechSynthesizer*
RendererWebKitPlatformSupportImpl::createSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return GetContentClient()->renderer()->OverrideSpeechSynthesizer(client);
}

//------------------------------------------------------------------------------

bool RendererWebKitPlatformSupportImpl::processMemorySizesInBytes(
    size_t* private_bytes, size_t* shared_bytes) {
  content::RenderThread::Get()->Send(
      new ViewHostMsg_GetProcessMemorySizes(private_bytes, shared_bytes));
  return true;
}

//------------------------------------------------------------------------------

blink::WebGraphicsContext3D*
RendererWebKitPlatformSupportImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  return createOffscreenGraphicsContext3D(attributes, NULL);
}

blink::WebGraphicsContext3D*
RendererWebKitPlatformSupportImpl::createOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    blink::WebGraphicsContext3D* share_context) {
  if (!RenderThreadImpl::current())
    return NULL;

#if defined(OS_ANDROID)
  if (SynchronousCompositorFactory* factory =
          SynchronousCompositorFactory::GetInstance()) {
    return factory->CreateOffscreenGraphicsContext3D(attributes);
  }
#endif

  scoped_refptr<GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync(
          CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE));

  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
  bool lose_context_when_out_of_memory = false;
  return WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
      gpu_channel_host.get(),
      attributes,
      lose_context_when_out_of_memory,
      GURL(attributes.topDocumentURL),
      limits,
      static_cast<WebGraphicsContext3DCommandBufferImpl*>(share_context));
}

//------------------------------------------------------------------------------

blink::WebGraphicsContext3DProvider* RendererWebKitPlatformSupportImpl::
    createSharedOffscreenGraphicsContext3DProvider() {
  scoped_refptr<webkit::gpu::ContextProviderWebContext> provider =
      RenderThreadImpl::current()->SharedMainThreadContextProvider();
  if (!provider)
    return NULL;
  return new WebGraphicsContext3DProviderImpl(provider);
}

//------------------------------------------------------------------------------

blink::WebCompositorSupport*
RendererWebKitPlatformSupportImpl::compositorSupport() {
  return &compositor_support_;
}

//------------------------------------------------------------------------------

blink::WebString RendererWebKitPlatformSupportImpl::convertIDNToUnicode(
    const blink::WebString& host,
    const blink::WebString& languages) {
  return net::IDNToUnicode(host.utf8(), languages.utf8());
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::setDeviceMotionListener(
    blink::WebDeviceMotionListener* listener) {
  if (g_test_device_motion_data == 0) {
    if (!device_motion_event_pump_) {
      device_motion_event_pump_.reset(new DeviceMotionEventPump);
      device_motion_event_pump_->Attach(RenderThreadImpl::current());
    }
    device_motion_event_pump_->SetListener(listener);
  } else if (listener) {
    // Testing mode: just echo the test data to the listener.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&blink::WebDeviceMotionListener::didChangeDeviceMotion,
                   base::Unretained(listener),
                   g_test_device_motion_data.Get()));
  }
}

// static
void RendererWebKitPlatformSupportImpl::SetMockDeviceMotionDataForTesting(
    const blink::WebDeviceMotionData& data) {
  g_test_device_motion_data.Get() = data;
}

// static
void RendererWebKitPlatformSupportImpl::ResetMockScreenOrientationForTesting()
{
  if (!(g_test_screen_orientation_controller == 0))
    g_test_screen_orientation_controller.Get().ResetData();
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::setDeviceOrientationListener(
    blink::WebDeviceOrientationListener* listener) {
  if (g_test_device_orientation_data == 0) {
    if (!device_orientation_event_pump_) {
      device_orientation_event_pump_.reset(new DeviceOrientationEventPump);
      device_orientation_event_pump_->Attach(RenderThreadImpl::current());
    }
    device_orientation_event_pump_->SetListener(listener);
  } else if (listener) {
    // Testing mode: just echo the test data to the listener.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &blink::WebDeviceOrientationListener::didChangeDeviceOrientation,
            base::Unretained(listener),
            g_test_device_orientation_data.Get()));
  }
}

// static
void RendererWebKitPlatformSupportImpl::SetMockDeviceOrientationDataForTesting(
    const blink::WebDeviceOrientationData& data) {
  g_test_device_orientation_data.Get() = data;
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::vibrate(unsigned int milliseconds) {
  RenderThread::Get()->Send(
      new ViewHostMsg_Vibrate(base::checked_cast<int64>(milliseconds)));
}

void RendererWebKitPlatformSupportImpl::cancelVibration() {
  RenderThread::Get()->Send(new ViewHostMsg_CancelVibration());
}

//------------------------------------------------------------------------------

// static
void RendererWebKitPlatformSupportImpl::SetMockScreenOrientationForTesting(
    RenderView* render_view,
    blink::WebScreenOrientationType orientation) {
  g_test_screen_orientation_controller.Get()
      .UpdateDeviceOrientation(render_view, orientation);
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::queryStorageUsageAndQuota(
    const blink::WebURL& storage_partition,
    blink::WebStorageQuotaType type,
    blink::WebStorageQuotaCallbacks callbacks) {
  if (!thread_safe_sender_.get() || !quota_message_filter_.get())
    return;
  QuotaDispatcher::ThreadSpecificInstance(
      thread_safe_sender_.get(),
      quota_message_filter_.get())->QueryStorageUsageAndQuota(
          storage_partition,
          static_cast<quota::StorageType>(type),
          QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

//------------------------------------------------------------------------------

void RendererWebKitPlatformSupportImpl::setBatteryStatusListener(
    blink::WebBatteryStatusListener* listener) {
  if (RenderThreadImpl::current() &&
      RenderThreadImpl::current()->layout_test_mode()) {
    // If we are in test mode, we want to use a fake battery status dispatcher,
    // which does not communicate with the browser process. Battery status
    // changes are signalled by invoking MockBatteryStatusChangedForTesting().
    g_test_battery_status_dispatcher.Get().SetListener(listener);
    return;
  }

  if (!battery_status_dispatcher_) {
    battery_status_dispatcher_.reset(
        new BatteryStatusDispatcher(RenderThreadImpl::current()));
  }
  battery_status_dispatcher_->SetListener(listener);
}

// static
void RendererWebKitPlatformSupportImpl::MockBatteryStatusChangedForTesting(
    const blink::WebBatteryStatus& status) {
  g_test_battery_status_dispatcher.Get().PostBatteryStatusChange(status);
}

}  // namespace content
