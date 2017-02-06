/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Platform_h
#define Platform_h

#ifdef WIN32
#include <windows.h>
#endif

#include "BlameContext.h"
#include "UserMetricsAction.h"
#include "WebAudioDevice.h"
#include "WebCommon.h"
#include "WebData.h"
#include "WebDeviceLightListener.h"
#include "WebGamepadListener.h"
#include "WebGamepads.h"
#include "WebGestureDevice.h"
#include "WebLocalizedString.h"
#include "WebPlatformEventType.h"
#include "WebSize.h"
#include "WebSpeechSynthesizer.h"
#include "WebStorageQuotaCallbacks.h"
#include "WebStorageQuotaType.h"
#include "WebString.h"
#include "WebURLError.h"
#include "WebVector.h"
#include "base/metrics/user_metrics_action.h"

class GrContext;

namespace v8 {
class Context;
template<class T> class Local;
}

namespace blink {

class ServiceRegistry;
class WebAudioBus;
class WebBlobRegistry;
class WebCanvasCaptureHandler;
class WebClipboard;
class WebCompositorSupport;
class WebCookieJar;
class WebCrypto;
class WebDatabaseObserver;
class WebPlatformEventListener;
class WebFallbackThemeEngine;
class WebFileSystem;
class WebFileUtilities;
class WebFlingAnimator;
class WebGestureCurve;
class WebGraphicsContext3DProvider;
class WebIDBFactory;
class WebImageCaptureFrameGrabber;
class WebInstalledApp;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMediaPlayer;
class WebMediaRecorderHandler;
class WebMediaStream;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
class WebMediaStreamTrack;
class WebMessagePortChannel;
class WebMimeRegistry;
class WebNotificationManager;
class WebPermissionClient;
class WebPluginListBuilder;
class WebPrescientNetworking;
class WebProcessMemoryDump;
class WebPublicSuffixList;
class WebPushProvider;
class WebRTCCertificateGenerator;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebSandboxSupport;
class WebScrollbarBehavior;
class WebSecurityOrigin;
class WebServiceWorkerCacheStorage;
class WebSocketHandle;
class WebSpeechSynthesizer;
class WebSpeechSynthesizerClient;
class WebStorageNamespace;
class WebSyncProvider;
struct WebFloatPoint;
class WebThemeEngine;
class WebThread;
class WebTrialTokenValidator;
class WebURLLoader;
class WebURLLoaderMockFactory;
class WebURLResponse;
class WebURLResponse;
struct WebSize;

class BLINK_PLATFORM_EXPORT Platform {
public:
    // HTML5 Database ------------------------------------------------------

#ifdef WIN32
    typedef HANDLE FileHandle;
#else
    typedef int FileHandle;
#endif

    // Initialize platform and wtf. If you need to initialize the entire Blink,
    // you should use blink::initialize.
    static void initialize(Platform*);
    static void shutdown();
    static Platform* current();

    // Used to switch the current platform only for testing.
    static void setCurrentPlatformForTesting(Platform*);

    // May return null.
    virtual WebCookieJar* cookieJar() { return nullptr; }

    // Must return non-null.
    virtual WebClipboard* clipboard() { return nullptr; }

    // Must return non-null.
    virtual WebFileUtilities* fileUtilities() { return nullptr; }

    // Must return non-null.
    virtual WebMimeRegistry* mimeRegistry() { return nullptr; }

    // May return null if sandbox support is not necessary
    virtual WebSandboxSupport* sandboxSupport() { return nullptr; }

    // May return null on some platforms.
    virtual WebThemeEngine* themeEngine() { return nullptr; }

    virtual WebFallbackThemeEngine* fallbackThemeEngine() { return nullptr; }

    // May return null.
    virtual WebSpeechSynthesizer* createSpeechSynthesizer(WebSpeechSynthesizerClient*) { return nullptr; }


    // Audio --------------------------------------------------------------

    virtual double audioHardwareSampleRate() { return 0; }
    virtual size_t audioHardwareBufferSize() { return 0; }
    virtual unsigned audioHardwareOutputChannels() { return 0; }

    // Creates a device for audio I/O.
    // Pass in (numberOfInputChannels > 0) if live/local audio input is desired.
    virtual WebAudioDevice* createAudioDevice(size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfChannels, double sampleRate, WebAudioDevice::RenderCallback*, const WebString& deviceId, const WebSecurityOrigin&) { return nullptr; }


    // MIDI ----------------------------------------------------------------

    // Creates a platform dependent WebMIDIAccessor. MIDIAccessor under platform
    // creates and owns it.
    virtual WebMIDIAccessor* createMIDIAccessor(WebMIDIAccessorClient*) { return nullptr; }


    // Blob ----------------------------------------------------------------

    // Must return non-null.
    virtual WebBlobRegistry* blobRegistry() { return nullptr; }

    // Database ------------------------------------------------------------

    // Opens a database file; dirHandle should be 0 if the caller does not need
    // a handle to the directory containing this file
    virtual FileHandle databaseOpenFile(const WebString& vfsFileName, int desiredFlags) { return FileHandle(); }

    // Deletes a database file and returns the error code
    virtual int databaseDeleteFile(const WebString& vfsFileName, bool syncDir) { return 0; }

    // Returns the attributes of the given database file
    virtual long databaseGetFileAttributes(const WebString& vfsFileName) { return 0; }

    // Returns the size of the given database file
    virtual long long databaseGetFileSize(const WebString& vfsFileName) { return 0; }

    // Returns the space available for the given origin
    virtual long long databaseGetSpaceAvailableForOrigin(const WebSecurityOrigin& origin) { return 0; }

    // Set the size of the given database file
    virtual bool databaseSetFileSize(const WebString& vfsFileName, long long size) { return false; }

    // Return a filename-friendly identifier for an origin.
    virtual WebString databaseCreateOriginIdentifier(const WebSecurityOrigin& origin) { return WebString(); }

    // DOM Storage --------------------------------------------------

    // Return a LocalStorage namespace
    virtual WebStorageNamespace* createLocalStorageNamespace() { return nullptr; }


    // FileSystem ----------------------------------------------------------

    // Must return non-null.
    virtual WebFileSystem* fileSystem() { return nullptr; }

    // Return a filename-friendly identifier for an origin.
    virtual WebString fileSystemCreateOriginIdentifier(const WebSecurityOrigin& origin) { return WebString(); }

    // IDN conversion ------------------------------------------------------

    virtual WebString convertIDNToUnicode(const WebString& host) { return host; }


    // IndexedDB ----------------------------------------------------------

    // Must return non-null.
    virtual WebIDBFactory* idbFactory() { return nullptr; }


    // Cache Storage ----------------------------------------------------------

    // The caller is responsible for deleting the returned object.
    virtual WebServiceWorkerCacheStorage* cacheStorage(const WebSecurityOrigin&) { return nullptr; }

    // Gamepad -------------------------------------------------------------

    virtual void sampleGamepads(WebGamepads& into) { into.length = 0; }

    // History -------------------------------------------------------------

    // Returns the hash for the given canonicalized URL for use in visited
    // link coloring.
    virtual unsigned long long visitedLinkHash(
        const char* canonicalURL, size_t length) { return 0; }

    // Returns whether the given link hash is in the user's history. The
    // hash must have been generated by calling VisitedLinkHash().
    virtual bool isLinkVisited(unsigned long long linkHash) { return false; }


    // Keygen --------------------------------------------------------------

    // Handle the <keygen> tag for generating client certificates
    // Returns a base64 encoded signed copy of a public key from a newly
    // generated key pair and the supplied challenge string. keySizeindex
    // specifies the strength of the key.
    virtual WebString signedPublicKeyAndChallengeString(
        unsigned keySizeIndex, const WebString& challenge, const WebURL& url, const WebURL& topOrigin)
    {
        return WebString();
    }

    // Same as above, but always returns actual value, without any caches.
    virtual size_t actualMemoryUsageMB() { return 0; }

    // Return the number of of processors of the current machine.
    virtual size_t numberOfProcessors() { return 0; }

    static const size_t noDecodedImageByteLimit = static_cast<size_t>(-1);

    // Returns the maximum amount of memory a decoded image should be allowed.
    // See comments on ImageDecoder::m_maxDecodedBytes.
    virtual size_t maxDecodedImageBytes() { return noDecodedImageByteLimit; }

    // Process -------------------------------------------------------------

    // Returns a unique identifier for a process. This may not necessarily be
    // the process's process ID.
    virtual uint32_t getUniqueIdForProcess() { return 0; }


    // Message Ports -------------------------------------------------------

    // Creates a Message Port Channel pair. This can be called on any thread.
    // The returned objects should only be used on the thread they were created on.
    virtual void createMessageChannel(WebMessagePortChannel** channel1, WebMessagePortChannel** channel2) { *channel1 = 0; *channel2 = 0; }


    // Network -------------------------------------------------------------

    // Returns a new WebURLLoader instance.
    virtual WebURLLoader* createURLLoader() { return nullptr; }

    // May return null.
    virtual WebPrescientNetworking* prescientNetworking() { return nullptr; }

    // Returns a new WebSocketHandle instance.
    virtual WebSocketHandle* createWebSocketHandle() { return nullptr; }

    // Returns the User-Agent string.
    virtual WebString userAgent() { return WebString(); }

    // A suggestion to cache this metadata in association with this URL.
    virtual void cacheMetadata(const WebURL&, int64_t responseTime, const char* data, size_t dataSize) {}

    // A suggestion to cache this metadata in association with this URL which resource is in CacheStorage.
    virtual void cacheMetadataInCacheStorage(const WebURL&, int64_t responseTime, const char* data, size_t dataSize, const blink::WebSecurityOrigin& cacheStorageOrigin, const WebString& cacheStorageCacheName) {}

    // Returns the decoded data url if url had a supported mimetype and parsing was successful.
    virtual WebData parseDataURL(const WebURL&, WebString& mimetype, WebString& charset) { return WebData(); }

    virtual WebURLError cancelledError(const WebURL&) const { return WebURLError(); }

    // Returns true and stores the position of the end of the headers to |*end|
    // if the headers part ends in |bytes[0..size]|. Returns false otherwise.
    virtual bool parseMultipartHeadersFromBody(const char* bytes, size_t /* size */, WebURLResponse*, size_t* end) const { return false; }

    // Plugins -------------------------------------------------------------

    // If refresh is true, then cached information should not be used to
    // satisfy this call.
    virtual void getPluginList(bool refresh, WebPluginListBuilder*) { }


    // Public Suffix List --------------------------------------------------

    // May return null on some platforms.
    virtual WebPublicSuffixList* publicSuffixList() { return nullptr; }


    // Resources -----------------------------------------------------------

    // Returns a localized string resource (with substitution parameters).
    virtual WebString queryLocalizedString(WebLocalizedString::Name) { return WebString(); }
    virtual WebString queryLocalizedString(WebLocalizedString::Name, const WebString& parameter) { return WebString(); }
    virtual WebString queryLocalizedString(WebLocalizedString::Name, const WebString& parameter1, const WebString& parameter2) { return WebString(); }


    // Threads -------------------------------------------------------

    // Creates an embedder-defined thread.
    virtual WebThread* createThread(const char* name) { return nullptr; }

    // Returns an interface to the current thread. This is owned by the
    // embedder.
    virtual WebThread* currentThread() { return nullptr; }

    // Returns a blame context for attributing top-level work which does not
    // belong to a particular frame scope.
    virtual BlameContext* topLevelBlameContext() { return nullptr; }

    // Resources -----------------------------------------------------------

    // Returns a blob of data corresponding to the named resource.
    virtual WebData loadResource(const char* name) { return WebData(); }

    // Decodes the in-memory audio file data and returns the linear PCM audio data in the destinationBus.
    // A sample-rate conversion to sampleRate will occur if the file data is at a different sample-rate.
    // Returns true on success.
    virtual bool loadAudioResource(WebAudioBus* destinationBus, const char* audioFileData, size_t dataSize) { return false; }

    // Scrollbar ----------------------------------------------------------

    // Must return non-null.
    virtual WebScrollbarBehavior* scrollbarBehavior() { return nullptr; }


    // Sudden Termination --------------------------------------------------

    // Disable/Enable sudden termination on a process level. When possible, it
    // is preferable to disable sudden termination on a per-frame level via
    // WebFrameClient::suddenTerminationDisablerChanged.
    virtual void suddenTerminationChanged(bool enabled) { }


    // System --------------------------------------------------------------

    // Returns a value such as "en-US".
    virtual WebString defaultLocale() { return WebString(); }

    // Returns an interface to the main thread. Can be null if blink was initialized on a thread without a message loop.
    WebThread* mainThread() const;

    // Returns an interface to the compositor thread. This can be null if the
    // renderer was created with threaded rendering desabled.
    virtual WebThread* compositorThread() const { return 0; }

    // Testing -------------------------------------------------------------

    // Gets a pointer to URLLoaderMockFactory for testing. Will not be available in production builds.
    virtual WebURLLoaderMockFactory* getURLLoaderMockFactory() { return nullptr; }

    // Record to a RAPPOR privacy-preserving metric, see: https://www.chromium.org/developers/design-documents/rappor.
    // recordRappor records a sample string, while recordRapporURL records the eTLD+1 of a url.
    virtual void recordRappor(const char* metric, const WebString& sample) { }
    virtual void recordRapporURL(const char* metric, const blink::WebURL& url) { }

    // Record a UMA sequence action.  The UserMetricsAction construction must
    // be on a single line for extract_actions.py to find it.  Please see
    // that script for more details.  Intended use is:
    // recordAction(UserMetricsAction("MyAction"))
    virtual void recordAction(const UserMetricsAction&) { }

    class TraceLogEnabledStateObserver {
    public:
        virtual ~TraceLogEnabledStateObserver() = default;
        virtual void onTraceLogEnabled() = 0;
        virtual void onTraceLogDisabled() = 0;
    };

    // Register or unregister a trace log state observer. Does not take ownership.
    virtual void addTraceLogEnabledStateObserver(TraceLogEnabledStateObserver*) {}
    virtual void removeTraceLogEnabledStateObserver(TraceLogEnabledStateObserver*) {}

    typedef uint64_t WebMemoryAllocatorDumpGuid;

    // GPU ----------------------------------------------------------------
    //
    struct ContextAttributes {
        bool failIfMajorPerformanceCaveat = false;
        unsigned webGLVersion = 0;
    };
    struct GraphicsInfo {
        unsigned vendorId = 0;
        unsigned deviceId = 0;
        unsigned processCrashCount = 0;
        unsigned resetNotificationStrategy = 0;
        bool sandboxed = false;
        bool amdSwitchable = false;
        bool optimus = false;
        WebString vendorInfo;
        WebString rendererInfo;
        WebString driverVersion;
        WebString errorMessage;
    };
    // Returns a newly allocated and initialized offscreen context provider,
    // backed by an independent context. Returns null if the context cannot be
    // created or initialized.
    // Passing an existing provider to shareContext will create the new context
    // in the same share group as the one passed.
    virtual WebGraphicsContext3DProvider* createOffscreenGraphicsContext3DProvider(
        const ContextAttributes&,
        const WebURL& topDocumentURL,
        WebGraphicsContext3DProvider* shareContext,
        GraphicsInfo*) { return nullptr; }

    // Returns a newly allocated and initialized offscreen context provider,
    // backed by the process-wide shared main thread context. Returns null if
    // the context cannot be created or initialized.
    virtual WebGraphicsContext3DProvider* createSharedOffscreenGraphicsContext3DProvider() { return nullptr; }

    // Returns true if the platform is capable of producing an offscreen context suitable for accelerating 2d canvas.
    // This will return false if the platform cannot promise that contexts will be preserved across operations like
    // locking the screen or if the platform cannot provide a context with suitable performance characteristics.
    //
    // This value must be checked again after a context loss event as the platform's capabilities may have changed.
    virtual bool canAccelerate2dCanvas() { return false; }

    virtual bool isThreadedCompositingEnabled() { return false; }
    virtual bool isThreadedAnimationEnabled() { return true; }

    virtual WebCompositorSupport* compositorSupport() { return nullptr; }

    virtual WebFlingAnimator* createFlingAnimator() { return nullptr; }

    // Creates a new fling animation curve instance for device |deviceSource|
    // with |velocity| and already scrolled |cumulativeScroll| pixels.
    virtual WebGestureCurve* createFlingAnimationCurve(WebGestureDevice deviceSource, const WebFloatPoint& velocity, const WebSize& cumulativeScroll) { return nullptr; }

    // WebRTC ----------------------------------------------------------

    // Creates a WebRTCPeerConnectionHandler for RTCPeerConnection.
    // May return null if WebRTC functionality is not avaliable or if it's out of resources.
    virtual WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient*) { return nullptr; }

    // Creates a WebMediaRecorderHandler to record MediaStreams.
    // May return null if the functionality is not available or out of resources.
    virtual WebMediaRecorderHandler* createMediaRecorderHandler() { return nullptr; }

    // May return null if WebRTC functionality is not available or out of resources.
    virtual WebRTCCertificateGenerator* createRTCCertificateGenerator() { return nullptr; }

    // May return null if WebRTC functionality is not available or out of resources.
    virtual WebMediaStreamCenter* createMediaStreamCenter(WebMediaStreamCenterClient*) { return nullptr; }

    // Creates a WebCanvasCaptureHandler to capture Canvas output.
    virtual WebCanvasCaptureHandler* createCanvasCaptureHandler(const WebSize&, double, WebMediaStreamTrack*) { return nullptr; }

    // Fills in the WebMediaStream to capture from the WebMediaPlayer identified
    // by the second parameter.
    virtual void createHTMLVideoElementCapturer(WebMediaStream*, WebMediaPlayer*) { }
    virtual void createHTMLAudioElementCapturer(WebMediaStream*, WebMediaPlayer*) { }

    // Creates a WebImageCaptureFrameGrabber to take a snapshot of a Video Tracks.
    // May return null if the functionality is not available.
    virtual WebImageCaptureFrameGrabber* createImageCaptureFrameGrabber() { return nullptr; }

    // WebWorker ----------------------------------------------------------

    virtual void didStartWorkerThread() { }
    virtual void willStopWorkerThread() { }
    virtual void workerContextCreated(const v8::Local<v8::Context>& worker) { }
    virtual bool allowScriptExtensionForServiceWorker(const WebURL& scriptUrl) { return false; }

    // WebCrypto ----------------------------------------------------------

    virtual WebCrypto* crypto() { return nullptr; }

    // Mojo ---------------------------------------------------------------

    virtual ServiceRegistry* serviceRegistry();

    // Platform events -----------------------------------------------------
    // Device Orientation, Device Motion, Device Light, Battery, Gamepad.

    // Request the platform to start listening to the events of the specified
    // type and notify the given listener (if not null) when there is an update.
    virtual void startListening(WebPlatformEventType type, WebPlatformEventListener* listener) { }

    // Request the platform to stop listening to the specified event and no
    // longer notify the listener, if any.
    virtual void stopListening(WebPlatformEventType type) { }

    // This method converts from the supplied DOM code enum to the
    // embedder's DOM code value for the key pressed. |domCode| values are
    // based on the value defined in ui/events/keycodes/dom4/keycode_converter_data.h.
    // Returns null string, if DOM code value is not found.
    virtual WebString domCodeStringFromEnum(int domCode) { return WebString(); }

    // This method converts from the suppled DOM code value to the
    // embedder's DOM code enum for the key pressed. |codeString| is defined in
    // ui/events/keycodes/dom4/keycode_converter_data.h.
    // Returns 0, if DOM code enum is not found.
    virtual int domEnumFromCodeString(const WebString& codeString) { return 0; }

    // This method converts from the supplied DOM |key| enum to the
    // corresponding DOM |key| string value for the key pressed. |domKey| values are
    // based on the value defined in ui/events/keycodes/dom3/dom_key_data.h.
    // Returns empty string, if DOM key value is not found.
    virtual WebString domKeyStringFromEnum(int domKey) { return WebString(); }

    // This method converts from the suppled DOM |key| value to the
    // embedder's DOM |key| enum for the key pressed. |keyString| is defined in
    // ui/events/keycodes/dom3/dom_key_data.h.
    // Returns 0 if DOM key enum is not found.
    virtual int domKeyEnumFromString(const WebString& keyString) { return 0; }

    // Quota -----------------------------------------------------------

    // Queries the storage partition's storage usage and quota information.
    // WebStorageQuotaCallbacks::didQueryStorageUsageAndQuota will be called
    // with the current usage and quota information for the partition. When
    // an error occurs WebStorageQuotaCallbacks::didFail is called with an
    // error code.
    virtual void queryStorageUsageAndQuota(
        const WebURL& storagePartition,
        WebStorageQuotaType,
        WebStorageQuotaCallbacks) { }


    // WebDatabase --------------------------------------------------------

    virtual WebDatabaseObserver* databaseObserver() { return nullptr; }


    // Web Notifications --------------------------------------------------

    virtual WebNotificationManager* notificationManager() { return nullptr; }


    // Push API------------------------------------------------------------

    virtual WebPushProvider* pushProvider() { return nullptr; }


    // Permissions --------------------------------------------------------

    virtual WebPermissionClient* permissionClient() { return nullptr; }


    // Background Sync API------------------------------------------------------------

    virtual WebSyncProvider* backgroundSyncProvider() { return nullptr; }

    // Experimental Framework ----------------------------------------------

    virtual WebTrialTokenValidator* trialTokenValidator() { return nullptr; }

protected:
    Platform();
    virtual ~Platform() { }

    WebThread* m_mainThread;
};

} // namespace blink

#endif
