// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8.h"

class GURL;
class SkBitmap;

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}

namespace blink {
class WebAppBannerClient;
class WebAudioDevice;
class WebClipboard;
class WebFrame;
class WebLocalFrame;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
class WebPlugin;
class WebPluginContainer;
class WebPrescientNetworking;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebSpeechSynthesizer;
class WebSpeechSynthesizerClient;
class WebThemeEngine;
class WebURLResponse;
class WebURLRequest;
class WebWorkerContentSettingsClientProxy;
struct WebPluginParams;
struct WebURLError;
}

namespace cc {
class ImageSerializationProcessor;
}

namespace media {
class GpuVideoAcceleratorFactories;
class KeySystemProperties;
class MediaLog;
class RendererFactory;
}

namespace shell {
class InterfaceRegistry;
}

namespace content {
class BrowserPluginDelegate;
class DocumentState;
class MediaStreamRendererFactory;
class RenderFrame;
class RenderView;
class SynchronousCompositor;
struct WebPluginInfo;

// Embedder API for participating in renderer logic.
class CONTENT_EXPORT ContentRendererClient {
 public:
  virtual ~ContentRendererClient() {}

  // Notifies us that the RenderThread has been created.
  virtual void RenderThreadStarted() {}

  // Notifies that a new RenderFrame has been created.
  virtual void RenderFrameCreated(RenderFrame* render_frame) {}

  // Notifies that a new RenderView has been created.
  virtual void RenderViewCreated(RenderView* render_view) {}

  // Returns the bitmap to show when a plugin crashed, or NULL for none.
  virtual SkBitmap* GetSadPluginBitmap();

  // Returns the bitmap to show when a <webview> guest has crashed, or NULL for
  // none.
  virtual SkBitmap* GetSadWebViewBitmap();

  // Allows the embedder to override creating a plugin. If it returns true, then
  // |plugin| will contain the created plugin, although it could be NULL. If it
  // returns false, the content layer will create the plugin.
  virtual bool OverrideCreatePlugin(
      RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params,
      blink::WebPlugin** plugin);

  // Creates a replacement plugin that is shown when the plugin at |file_path|
  // couldn't be loaded. This allows the embedder to show a custom placeholder.
  // This may return nullptr. However, if it does return a WebPlugin, it must
  // never fail to initialize.
  virtual blink::WebPlugin* CreatePluginReplacement(
      RenderFrame* render_frame,
      const base::FilePath& plugin_path);

  // Creates a delegate for browser plugin.
  virtual BrowserPluginDelegate* CreateBrowserPluginDelegate(
      RenderFrame* render_frame,
      const std::string& mime_type,
      const GURL& original_url);

  // Returns true if the embedder has an error page to show for the given http
  // status code. If so |error_domain| should be set to according to WebURLError
  // and the embedder's GetNavigationErrorHtml will be called afterwards to get
  // the error html.
  virtual bool HasErrorPage(int http_status_code,
                            std::string* error_domain);

  // Returns true if the embedder prefers not to show an error page for a failed
  // navigation to |url| in |render_frame|.
  virtual bool ShouldSuppressErrorPage(RenderFrame* render_frame,
                                       const GURL& url);

  // Returns the information to display when a navigation error occurs.
  // If |error_html| is not null then it may be set to a HTML page containing
  // the details of the error and maybe links to more info.
  // If |error_description| is not null it may be set to contain a brief
  // message describing the error that has occurred.
  // Either of the out parameters may be not written to in certain cases
  // (lack of information on the error code) so the caller should take care to
  // initialize the string values with safe defaults before the call.
  virtual void GetNavigationErrorStrings(
      content::RenderFrame* render_frame,
      const blink::WebURLRequest& failed_request,
      const blink::WebURLError& error,
      std::string* error_html,
      base::string16* error_description) {}

  // Allows the embedder to control when media resources are loaded. Embedders
  // can run |closure| immediately if they don't wish to defer media resource
  // loading.  If |has_played_media_before| is true, the render frame has
  // previously started media playback (i.e. played audio and video).
  virtual void DeferMediaLoad(RenderFrame* render_frame,
                              bool has_played_media_before,
                              const base::Closure& closure);

  // Allows the embedder to override creating a WebMediaStreamCenter. If it
  // returns NULL the content layer will create the stream center.
  virtual blink::WebMediaStreamCenter* OverrideCreateWebMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client);

  // Allows the embedder to override creating a WebRTCPeerConnectionHandler. If
  // it returns NULL the content layer will create the connection handler.
  virtual blink::WebRTCPeerConnectionHandler*
  OverrideCreateWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);

  // Allows the embedder to override creating a WebMIDIAccessor.  If it
  // returns NULL the content layer will create the MIDI accessor.
  virtual blink::WebMIDIAccessor* OverrideCreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client);

  // Allows the embedder to override creating a WebAudioDevice.  If it
  // returns NULL the content layer will create the audio device.
  virtual blink::WebAudioDevice* OverrideCreateAudioDevice(
      double sample_rate);

  // Allows the embedder to override the blink::WebClipboard used. If it
  // returns NULL the content layer will handle clipboard interactions.
  virtual blink::WebClipboard* OverrideWebClipboard();

  // Allows the embedder to override the WebThemeEngine used. If it returns NULL
  // the content layer will provide an engine.
  virtual blink::WebThemeEngine* OverrideThemeEngine();

  // Allows the embedder to override the WebSpeechSynthesizer used.
  // If it returns NULL the content layer will provide an engine.
  virtual blink::WebSpeechSynthesizer* OverrideSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client);

  // Returns true if the renderer process should schedule the idle handler when
  // all widgets are hidden.
  virtual bool RunIdleHandlerWhenWidgetsHidden();

  // Returns true if the renderer process should allow shared timer suspension
  // after the process has been backgrounded. Defaults to false.
  virtual bool AllowTimerSuspensionWhenProcessBackgrounded();

  // Returns true if a popup window should be allowed.
  virtual bool AllowPopup();

#if defined(OS_ANDROID)
  // TODO(sgurun) This callback is deprecated and will be removed as soon
  // as android webview completes implementation of a resource throttle based
  // shouldoverrideurl implementation. See crbug.com/325351
  //
  // Returns true if the navigation was handled by the embedder and should be
  // ignored by WebKit. This method is used by CEF and android_webview.
  virtual bool HandleNavigation(RenderFrame* render_frame,
                                bool is_content_initiated,
                                int opener_id,
                                blink::WebFrame* frame,
                                const blink::WebURLRequest& request,
                                blink::WebNavigationType type,
                                blink::WebNavigationPolicy default_policy,
                                bool is_redirect);

  // Indicates if the Android MediaPlayer should be used instead of Chrome's
  // built in media player for the given |url|. Defaults to false.
  virtual bool ShouldUseMediaPlayerForURL(const GURL& url);
#endif

  // Returns true if we should fork a new process for the given navigation.
  // If |send_referrer| is set to false (which is the default), no referrer
  // header will be send for the navigation. Otherwise, the referrer header is
  // set according to the frame's referrer policy.
  virtual bool ShouldFork(blink::WebLocalFrame* frame,
                          const GURL& url,
                          const std::string& http_method,
                          bool is_initial_navigation,
                          bool is_server_redirect,
                          bool* send_referrer);

  // Notifies the embedder that the given frame is requesting the resource at
  // |url|.  If the function returns true, the url is changed to |new_url|.
  virtual bool WillSendRequest(blink::WebFrame* frame,
                               ui::PageTransition transition_type,
                               const GURL& url,
                               const GURL& first_party_for_cookies,
                               GURL* new_url);

  // See blink::Platform.
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length);
  virtual bool IsLinkVisited(unsigned long long link_hash);
  virtual blink::WebPrescientNetworking* GetPrescientNetworking();
  virtual bool ShouldOverridePageVisibilityState(
      const RenderFrame* render_frame,
      blink::WebPageVisibilityState* override_state);

  // Returns true if the given Pepper plugin is external (requiring special
  // startup steps).
  virtual bool IsExternalPepperPlugin(const std::string& module_name);

  // Returns true if the page at |url| can use Pepper MediaStream APIs.
  virtual bool AllowPepperMediaStreamAPI(const GURL& url);

  // Allows an embedder to provide a media::RendererFactory.
  virtual std::unique_ptr<media::RendererFactory> CreateMediaRendererFactory(
      RenderFrame* render_frame,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const scoped_refptr<media::MediaLog>& media_log);

  // Allows an embedder to provide a MediaStreamRendererFactory.
  virtual std::unique_ptr<MediaStreamRendererFactory>
  CreateMediaStreamRendererFactory();

  // Allows an embedder to provde a cc::ImageSerializationProcessor.
  virtual cc::ImageSerializationProcessor* GetImageSerializationProcessor();

  // Gives the embedder a chance to register the key system(s) it supports by
  // populating |key_systems|.
  virtual void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems);

  // Returns true if we should report a detailed message (including a stack
  // trace) for console [logs|errors|exceptions]. |source| is the WebKit-
  // reported source for the error; this can point to a page or a script,
  // and can be external or internal.
  virtual bool ShouldReportDetailedMessageForSource(
      const base::string16& source) const;

  // Returns true if we should gather stats during resource loads as if the
  // cross-site document blocking policy were enabled. Does not actually block
  // any pages.
  virtual bool ShouldGatherSiteIsolationStats() const;

  // Creates a permission client proxy for in-renderer worker.
  virtual blink::WebWorkerContentSettingsClientProxy*
      CreateWorkerContentSettingsClientProxy(RenderFrame* render_frame,
                                             blink::WebFrame* frame);

  // Returns true if the page at |url| can use Pepper CameraDevice APIs.
  virtual bool IsPluginAllowedToUseCameraDeviceAPI(const GURL& url);

  // Returns true if the page at |url| can use Pepper Compositor APIs.
  virtual bool IsPluginAllowedToUseCompositorAPI(const GURL& url);

  // Returns true if dev channel APIs are available for plugins.
  virtual bool IsPluginAllowedToUseDevChannelAPIs();

  // Records a sample string to a Rappor privacy-preserving metric.
  // See: https://www.chromium.org/developers/design-documents/rappor
  virtual void RecordRappor(const std::string& metric,
                            const std::string& sample) {}

  // Records a domain and registry of a url to a Rappor privacy-preserving
  // metric. See: https://www.chromium.org/developers/design-documents/rappor
  virtual void RecordRapporURL(const std::string& metric, const GURL& url) {}

  // Allows an embedder to provide a blink::WebAppBannerClient.
  virtual std::unique_ptr<blink::WebAppBannerClient> CreateAppBannerClient(
      RenderFrame* render_frame);

  // Gives the embedder a chance to add properties to the context menu.
  // Currently only called when the context menu is for an image.
  virtual void AddImageContextMenuProperties(
      const blink::WebURLResponse& response,
      std::map<std::string, std::string>* properties) {}

  // Notifies that a document element has been inserted in the frame's document.
  // This may be called multiple times for the same document. This method may
  // invalidate the frame.
  virtual void RunScriptsAtDocumentStart(RenderFrame* render_frame) {}

  // Notifies that the DOM is ready in the frame's document.
  // This method may invalidate the frame.
  virtual void RunScriptsAtDocumentEnd(RenderFrame* render_frame) {}

  // Notifies that a service worker context has been created. This function
  // is called from the worker thread.
  virtual void DidInitializeServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int embedded_worker_id,
      const GURL& url) {}

  // Notifies that a service worker context will be destroyed. This function
  // is called from the worker thread.
  virtual void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int embedded_worker_id,
      const GURL& url) {}

  // Whether this renderer should enforce preferences related to the WebRTC
  // routing logic, i.e. allowing multiple routes and non-proxied UDP.
  virtual bool ShouldEnforceWebRTCRoutingPreferences();

  // Notifies that a worker context has been created. This function is called
  // from the worker thread.
  virtual void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) {}

  // Allows the client to expose interfaces from the renderer process to the
  // browser process via |registry|.
  virtual void ExposeInterfacesToBrowser(
      shell::InterfaceRegistry* interface_registry) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_CONTENT_RENDERER_CLIENT_H_
