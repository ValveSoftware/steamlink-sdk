/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#ifndef WebFrameClient_h
#define WebFrameClient_h

#include "../platform/WebColor.h"
#include "WebAXObject.h"
#include "WebDOMMessageEvent.h"
#include "WebDataSource.h"
#include "WebFileChooserParams.h"
#include "WebFrame.h"
#include "WebFrameOwnerProperties.h"
#include "WebHistoryCommitType.h"
#include "WebHistoryItem.h"
#include "WebIconURL.h"
#include "WebNavigationPolicy.h"
#include "WebNavigationType.h"
#include "WebNavigatorContentUtilsClient.h"
#include "WebSandboxFlags.h"
#include "WebTextDirection.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebEffectiveConnectionType.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFileSystemType.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebLoadingBehaviorFlag.h"
#include "public/platform/WebPageVisibilityState.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebSetSinkIdCallbacks.h"
#include "public/platform/WebStorageQuotaCallbacks.h"
#include "public/platform/WebStorageQuotaType.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebContentSecurityPolicy.h"
#include <v8.h>

namespace blink {

enum class WebTreeScopeType;
class ServiceRegistry;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebAppBannerClient;
class WebBluetooth;
class WebColorChooser;
class WebColorChooserClient;
class WebContentDecryptionModule;
class WebCookieJar;
class WebCString;
class WebDataSource;
class WebEncryptedMediaClient;
class WebExternalPopupMenu;
class WebExternalPopupMenuClient;
class WebFileChooserCompletion;
class WebFormElement;
class WebInstalledAppClient;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
class WebMediaPlayerSource;
class WebMediaSession;
class WebMediaStream;
class WebMIDIClient;
class WebNotificationPermissionCallback;
class WebPermissionClient;
class WebServiceWorkerProvider;
class WebSocketHandle;
class WebPlugin;
class WebPresentationClient;
class WebPushClient;
class WebRTCPeerConnectionHandler;
class WebScreenOrientationClient;
class WebString;
class WebURL;
class WebURLResponse;
class WebUserMediaClient;
class WebWorkerContentSettingsClientProxy;
struct WebColorSuggestion;
struct WebConsoleMessage;
struct WebContextMenuData;
struct WebPluginParams;
struct WebPopupMenuInfo;
struct WebRect;
struct WebURLError;

class WebFrameClient {
public:
    // Factory methods -----------------------------------------------------

    // May return null.
    virtual WebPlugin* createPlugin(WebLocalFrame*, const WebPluginParams&) { return 0; }

    // May return null.
    // WebContentDecryptionModule* may be null if one has not yet been set.
    virtual WebMediaPlayer* createMediaPlayer(const WebMediaPlayerSource&, WebMediaPlayerClient*, WebMediaPlayerEncryptedMediaClient*, WebContentDecryptionModule*, const WebString& sinkId, WebMediaSession*) { return 0; }

    // May return null.
    virtual WebMediaSession* createMediaSession() { return 0; }

    // May return null.
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebApplicationCacheHostClient*) { return 0; }

    // May return null.
    virtual WebServiceWorkerProvider* createServiceWorkerProvider() { return 0; }

    // May return null.
    virtual WebWorkerContentSettingsClientProxy* createWorkerContentSettingsClientProxy() { return 0; }

    // Create a new WebPopupMenu. In the "createExternalPopupMenu" form, the
    // client is responsible for rendering the contents of the popup menu.
    virtual WebExternalPopupMenu* createExternalPopupMenu(
        const WebPopupMenuInfo&, WebExternalPopupMenuClient*) { return 0; }


    // Services ------------------------------------------------------------

    // A frame specific cookie jar.  May return null, in which case
    // WebKitPlatformSupport::cookieJar() will be called to access cookies.
    virtual WebCookieJar* cookieJar() { return 0; }

    // Returns a blame context for attributing work belonging to this frame.
    virtual BlameContext* frameBlameContext() { return nullptr; }

    // General notifications -----------------------------------------------

    // Indicates if creating a plugin without an associated renderer is supported.
    virtual bool canCreatePluginWithoutRenderer(const WebString& mimeType) { return false; }

    // Indicates that another page has accessed the DOM of the initial empty
    // document of a main frame. After this, it is no longer safe to show a
    // pending navigation's URL, because a URL spoof is possible.
    virtual void didAccessInitialDocument() { }

    // A child frame was created in this frame. This is called when the frame
    // is created and initialized. Takes the name of the new frame, the parent
    // frame and returns a new WebFrame. The WebFrame is considered in-use
    // until frameDetached() is called on it.
    // Note: If you override this, you should almost certainly be overriding
    // frameDetached().
    virtual WebFrame* createChildFrame(WebLocalFrame* parent, WebTreeScopeType, const WebString& name, const WebString& uniqueName, WebSandboxFlags sandboxFlags, const WebFrameOwnerProperties&) { return nullptr; }

    // This frame has set its opener to another frame, or disowned the opener
    // if opener is null. See http://html.spec.whatwg.org/#dom-opener.
    virtual void didChangeOpener(WebFrame*) { }

    // Specifies the reason for the detachment.
    enum class DetachType { Remove, Swap };

    // This frame has been detached from the view, but has not been closed yet.
    virtual void frameDetached(WebFrame*, DetachType) { }

    // This frame has become focused..
    virtual void frameFocused() { }

    // This frame is about to be closed. This is called after frameDetached,
    // when the document is being unloaded, due to new one committing.
    virtual void willClose(WebFrame*) { }

    // This frame's name has changed.
    virtual void didChangeName(const WebString& name, const WebString& uniqueName) { }

    // This frame has set an insecure request policy.
    virtual void didEnforceInsecureRequestPolicy(WebInsecureRequestPolicy) {}

    // This frame has been updated to a unique origin, which should be
    // considered potentially trustworthy if
    // |isPotentiallyTrustworthyUniqueOrigin| is true. TODO(estark):
    // this method only exists to support dynamic sandboxing via a CSP
    // delivered in a <meta> tag. This is not supposed to be allowed per
    // the CSP spec and should be ripped out. https://crbug.com/594645
    virtual void didUpdateToUniqueOrigin(bool isPotentiallyTrustworthyUniqueOrigin) {}

    // The sandbox flags have changed for a child frame of this frame.
    virtual void didChangeSandboxFlags(WebFrame* childFrame, WebSandboxFlags flags) { }

    // Called when a new Content Security Policy is added to the frame's
    // document.  This can be triggered by handling of HTTP headers, handling
    // of <meta> element, or by inheriting CSP from the parent (in case of
    // about:blank).
    virtual void didAddContentSecurityPolicy(const WebString& headerValue, WebContentSecurityPolicyType, WebContentSecurityPolicySource) { }

    // Some frame owner properties have changed for a child frame of this frame.
    // Frame owner properties currently include: scrolling, marginwidth and
    // marginheight.
    virtual void didChangeFrameOwnerProperties(WebFrame* childFrame, const WebFrameOwnerProperties&) { }

    // Called when a watched CSS selector matches or stops matching.
    virtual void didMatchCSS(WebLocalFrame*, const WebVector<WebString>& newlyMatchingSelectors, const WebVector<WebString>& stoppedMatchingSelectors) { }


    // Console messages ----------------------------------------------------

    // Whether or not we should report a detailed message for the given source.
    virtual bool shouldReportDetailedMessageForSource(const WebString& source) { return false; }

    // A new message was added to the console.
    virtual void didAddMessageToConsole(const WebConsoleMessage&, const WebString& sourceName, unsigned sourceLine, const WebString& stackTrace) { }


    // Load commands -------------------------------------------------------

    // The client should handle the navigation externally.
    virtual void loadURLExternally(const WebURLRequest&, WebNavigationPolicy, const WebString& downloadName, bool shouldReplaceCurrentEntry) {}

    // Navigational queries ------------------------------------------------

    // The client may choose to alter the navigation policy.  Otherwise,
    // defaultPolicy should just be returned.

    struct NavigationPolicyInfo {
        WebDataSource::ExtraData* extraData;

        // Note: if browser side navigations are enabled, the client may modify
        // the urlRequest. However, should this happen, the client should change
        // the WebNavigationPolicy to WebNavigationPolicyIgnore, and the load
        // should stop in blink. In all other cases, the urlRequest should not
        // be modified.
        WebURLRequest& urlRequest;
        WebNavigationType navigationType;
        WebNavigationPolicy defaultPolicy;
        bool replacesCurrentHistoryItem;
        bool isHistoryNavigationInNewChildFrame;
        bool isClientRedirect;

        NavigationPolicyInfo(WebURLRequest& urlRequest)
            : extraData(nullptr)
            , urlRequest(urlRequest)
            , navigationType(WebNavigationTypeOther)
            , defaultPolicy(WebNavigationPolicyIgnore)
            , replacesCurrentHistoryItem(false)
            , isHistoryNavigationInNewChildFrame(false)
            , isClientRedirect(false)
        {
        }
    };

    virtual WebNavigationPolicy decidePolicyForNavigation(const NavigationPolicyInfo& info)
    {
        return info.defaultPolicy;
    }

    // During a history navigation, we may choose to load new subframes from history as well.
    // This returns such a history item if appropriate.
    virtual WebHistoryItem historyItemForNewChildFrame() { return WebHistoryItem(); }

    // Whether the client is handling a navigation request.
    virtual bool hasPendingNavigation() { return false; }

    // Navigational notifications ------------------------------------------

    // These notifications bracket any loading that occurs in the WebFrame.
    virtual void didStartLoading(bool toDifferentDocument) { }
    virtual void didStopLoading() { }

    // Notification that some progress was made loading the current frame.
    // loadProgress is a value between 0 (nothing loaded) and 1.0 (frame fully
    // loaded).
    virtual void didChangeLoadProgress(double loadProgress) { }

    // A form submission has been requested, but the page's submit event handler
    // hasn't yet had a chance to run (and possibly alter/interrupt the submit.)
    virtual void willSendSubmitEvent(const WebFormElement&) { }

    // A form submission is about to occur.
    virtual void willSubmitForm(const WebFormElement&) { }

    // A datasource has been created for a new navigation.  The given
    // datasource will become the provisional datasource for the frame.
    virtual void didCreateDataSource(WebLocalFrame*, WebDataSource*) { }

    // A new provisional load has been started.
    virtual void didStartProvisionalLoad(WebLocalFrame* localFrame, double triggeringEventTime) { }

    // The provisional load was redirected via a HTTP 3xx response.
    virtual void didReceiveServerRedirectForProvisionalLoad(WebLocalFrame*) { }

    // The provisional load failed. The WebHistoryCommitType is the commit type
    // that would have been used had the load succeeded.
    virtual void didFailProvisionalLoad(WebLocalFrame*, const WebURLError&, WebHistoryCommitType) { }

    // The provisional datasource is now committed.  The first part of the
    // response body has been received, and the encoding of the response
    // body is known.
    virtual void didCommitProvisionalLoad(WebLocalFrame*, const WebHistoryItem&, WebHistoryCommitType) { }

    // The frame's document has just been initialized.
    virtual void didCreateNewDocument(WebLocalFrame* frame) { }

    // The window object for the frame has been cleared of any extra
    // properties that may have been set by script from the previously
    // loaded document.
    virtual void didClearWindowObject(WebLocalFrame* frame) { }

    // The document element has been created.
    // This method may not invalidate the frame, nor execute JavaScript code.
    virtual void didCreateDocumentElement(WebLocalFrame*) { }

    // Like |didCreateDocumentElement|, except this method may run JavaScript
    // code (and possibly invalidate the frame).
    virtual void runScriptsAtDocumentElementAvailable(WebLocalFrame*) { }

    // The page title is available.
    virtual void didReceiveTitle(WebLocalFrame* frame, const WebString& title, WebTextDirection direction) { }

    // The icon for the page have changed.
    virtual void didChangeIcon(WebLocalFrame*, WebIconURL::Type) { }

    // The frame's document finished loading.
    // This method may not execute JavaScript code.
    virtual void didFinishDocumentLoad(WebLocalFrame*) { }

    // Like |didFinishDocumentLoad|, except this method may run JavaScript
    // code (and possibly invalidate the frame).
    virtual void runScriptsAtDocumentReady(WebLocalFrame*, bool documentIsEmpty) { }

    // The 'load' event was dispatched.
    virtual void didHandleOnloadEvents(WebLocalFrame*) { }

    // The frame's document or one of its subresources failed to load. The
    // WebHistoryCommitType is the commit type that would have been used had the
    // load succeeded.
    virtual void didFailLoad(WebLocalFrame*, const WebURLError&, WebHistoryCommitType) { }

    // The frame's document and all of its subresources succeeded to load.
    virtual void didFinishLoad(WebLocalFrame*) { }

    // The navigation resulted in no change to the documents within the page.
    // For example, the navigation may have just resulted in scrolling to a
    // named anchor or a PopState event may have been dispatched.
    virtual void didNavigateWithinPage(WebLocalFrame*, const WebHistoryItem&, WebHistoryCommitType, bool contentInitiated) { }

    // Called upon update to scroll position, document state, and other
    // non-navigational events related to the data held by WebHistoryItem.
    // WARNING: This method may be called very frequently.
    virtual void didUpdateCurrentHistoryItem() { }

    // The frame's manifest has changed.
    virtual void didChangeManifest() { }

    // The frame's theme color has changed.
    virtual void didChangeThemeColor() { }

    // Called to dispatch a load event for this frame in the FrameOwner of an
    // out-of-process parent frame.
    virtual void dispatchLoad() { }

    // Returns the effective connection type when the frame was fetched.
    virtual WebEffectiveConnectionType getEffectiveConnectionType() { return WebEffectiveConnectionType::TypeUnknown; }

    // Web Notifications ---------------------------------------------------

    // Requests permission to display platform notifications on the origin of this frame.
    virtual void requestNotificationPermission(const WebSecurityOrigin&, WebNotificationPermissionCallback* callback) { }


    // Push API ---------------------------------------------------

    // Used to access the embedder for the Push API.
    virtual WebPushClient* pushClient() { return 0; }


    // Presentation API ----------------------------------------------------

    // Used to access the embedder for the Presentation API.
    virtual WebPresentationClient* presentationClient() { return 0; }

    // InstalledApp API ----------------------------------------------------

    // Used to access the embedder for the InstalledApp API.
    virtual WebInstalledAppClient* installedAppClient() { return nullptr; }

    // Editing -------------------------------------------------------------

    // These methods allow the client to intercept and overrule editing
    // operations.
    virtual void didChangeSelection(bool isSelectionEmpty) { }


    // Dialogs -------------------------------------------------------------

    // This method opens the color chooser and returns a new WebColorChooser
    // instance. If there is a WebColorChooser already from the last time this
    // was called, it ends the color chooser by calling endChooser, and replaces
    // it with the new one. The given list of suggestions can be used to show a
    // simple interface with a limited set of choices.

    virtual WebColorChooser* createColorChooser(
        WebColorChooserClient*,
        const WebColor&,
        const WebVector<WebColorSuggestion>&) { return 0; }

    // Displays a modal alert dialog containing the given message. Returns
    // once the user dismisses the dialog.
    virtual void runModalAlertDialog(const WebString& message) { }

    // Displays a modal confirmation dialog with the given message as
    // description and OK/Cancel choices. Returns true if the user selects
    // 'OK' or false otherwise.
    virtual bool runModalConfirmDialog(const WebString& message) { return false; }

    // Displays a modal input dialog with the given message as description
    // and OK/Cancel choices. The input field is pre-filled with
    // defaultValue. Returns true if the user selects 'OK' or false
    // otherwise. Upon returning true, actualValue contains the value of
    // the input field.
    virtual bool runModalPromptDialog(
        const WebString& message, const WebString& defaultValue,
        WebString* actualValue) { return false; }

    // Displays a modal confirmation dialog with OK/Cancel choices, where 'OK'
    // means that it is okay to proceed with closing the view. Returns true if
    // the user selects 'OK' or false otherwise.
    virtual bool runModalBeforeUnloadDialog(bool isReload) { return true; }

    // This method returns immediately after showing the dialog. When the
    // dialog is closed, it should call the WebFileChooserCompletion to
    // pass the results of the dialog. Returns false if
    // WebFileChooseCompletion will never be called.
    virtual bool runFileChooser(
        const blink::WebFileChooserParams& params,
        WebFileChooserCompletion* chooserCompletion) { return false; }

    // UI ------------------------------------------------------------------

    // Shows a context menu with commands relevant to a specific element on
    // the given frame. Additional context data is supplied.
    virtual void showContextMenu(const WebContextMenuData&) { }

    // This method is called in response to WebView's saveImageAt(x, y).
    // A data url from <canvas> or <img> is passed to the method's argument.
    virtual void saveImageFromDataURL(const WebString&) { }

    // Low-level resource notifications ------------------------------------

    // A request is about to be sent out, and the client may modify it.  Request
    // is writable, and changes to the URL, for example, will change the request
    // made.  If this request is the result of a redirect, then redirectResponse
    // will be non-null and contain the response that triggered the redirect.
    virtual void willSendRequest(
        WebLocalFrame*, unsigned identifier, WebURLRequest&,
        const WebURLResponse& redirectResponse) { }

    // Response headers have been received for the resource request given
    // by identifier.
    virtual void didReceiveResponse(unsigned identifier, const WebURLResponse&) { }

    virtual void didChangeResourcePriority(
        unsigned identifier, const WebURLRequest::Priority& priority, int) { }

    // The resource request given by identifier succeeded.
    virtual void didFinishResourceLoad(
        WebLocalFrame*, unsigned identifier) { }

    // The specified request was satified from WebCore's memory cache.
    virtual void didLoadResourceFromMemoryCache(
        const WebURLRequest&, const WebURLResponse&) { }

    // This frame has displayed inactive content (such as an image) from an
    // insecure source.  Inactive content cannot spread to other frames.
    virtual void didDisplayInsecureContent() { }

    // The indicated security origin has run active content (such as a
    // script) from an insecure source.  Note that the insecure content can
    // spread to other frames in the same origin.
    virtual void didRunInsecureContent(const WebSecurityOrigin&, const WebURL& insecureURL) { }

    // A reflected XSS was encountered in the page and suppressed.
    virtual void didDetectXSS(const WebURL&, bool didBlockEntirePage) { }

    // A PingLoader was created, and a request dispatched to a URL.
    virtual void didDispatchPingLoader(const WebURL&) { }

    // This frame has displayed inactive content (such as an image) from
    // a connection with certificate errors.
    virtual void didDisplayContentWithCertificateErrors(const WebURL& url, const WebCString& securityInfo) {}
    // This frame has run active content (such as a script) from a
    // connection with certificate errors.
    virtual void didRunContentWithCertificateErrors(const WebURL& url, const WebCString& securityInfo) {}

    // A performance timing event (e.g. first paint) occurred
    virtual void didChangePerformanceTiming() { }

    // Blink exhibited a certain loading behavior that the browser process will
    // use for segregated histograms.
    virtual void didObserveLoadingBehavior(WebLoadingBehaviorFlag) { }


    // Script notifications ------------------------------------------------

    // Notifies that a new script context has been created for this frame.
    // This is similar to didClearWindowObject but only called once per
    // frame context.
    virtual void didCreateScriptContext(WebLocalFrame*, v8::Local<v8::Context>, int extensionGroup, int worldId) { }

    // WebKit is about to release its reference to a v8 context for a frame.
    virtual void willReleaseScriptContext(WebLocalFrame*, v8::Local<v8::Context>, int worldId) { }


    // Geometry notifications ----------------------------------------------

    // The main frame scrolled.
    virtual void didChangeScrollOffset(WebLocalFrame*) { }

    // If the frame is loading an HTML document, this will be called to
    // notify that the <body> will be attached soon.
    virtual void willInsertBody(WebLocalFrame*) { }


    // Find-in-page notifications ------------------------------------------

    // Notifies how many matches have been found so far, for a given
    // identifier.  |finalUpdate| specifies whether this is the last update
    // (all frames have completed scoping). This notification is only delivered
    // to the main frame and aggregates all matches across all frames.
    virtual void reportFindInPageMatchCount(
        int identifier, int count, bool finalUpdate) { }

    // Notifies how many matches have been found in a specific frame so far,
    // for a given identifier. Unlike reprotFindInPageMatchCount(), this
    // notification is sent to the client of each frame, and only reports
    // results per-frame.
    virtual void reportFindInFrameMatchCount(
        int identifier, int count, bool finalUpdate) {}

    // Notifies what tick-mark rect is currently selected.   The given
    // identifier lets the client know which request this message belongs
    // to, so that it can choose to ignore the message if it has moved on
    // to other things.  The selection rect is expected to have coordinates
    // relative to the top left corner of the web page area and represent
    // where on the screen the selection rect is currently located.
    virtual void reportFindInPageSelection(
        int identifier, int activeMatchOrdinal, const WebRect& selection) { }

    // Quota ---------------------------------------------------------

    // Requests a new quota size for the origin's storage.
    // |newQuotaInBytes| indicates how much storage space (in bytes) the
    // caller expects to need.
    // WebStorageQuotaCallbacks::didGrantStorageQuota will be called when
    // a new quota is granted. WebStorageQuotaCallbacks::didFail
    // is called with an error code otherwise.
    // Note that the requesting quota size may not always be granted and
    // a smaller amount of quota than requested might be returned.
    virtual void requestStorageQuota(
        WebStorageQuotaType,
        unsigned long long newQuotaInBytes,
        WebStorageQuotaCallbacks) { }

    // WebSocket -----------------------------------------------------

    // A WebSocket object is going to open a new WebSocket connection.
    virtual void willOpenWebSocket(WebSocketHandle*) { }

    // MediaStream -----------------------------------------------------

    // A new WebRTCPeerConnectionHandler is created.
    virtual void willStartUsingPeerConnectionHandler(WebRTCPeerConnectionHandler*) { }

    virtual WebUserMediaClient* userMediaClient() { return 0; }


    // Encrypted Media -------------------------------------------------

    virtual WebEncryptedMediaClient* encryptedMediaClient() { return 0; }


    // Web MIDI -------------------------------------------------------------

    virtual WebMIDIClient* webMIDIClient() { return 0; }

    // User agent ------------------------------------------------------

    // Asks the embedder if a specific user agent should be used. Non-empty
    // strings indicate an override should be used. Otherwise,
    // Platform::current()->userAgent() will be called to provide one.
    virtual WebString userAgentOverride() { return WebString(); }

    // Do not track ----------------------------------------------------

    // Asks the embedder what value the network stack will send for the DNT
    // header. An empty string indicates that no DNT header will be send.
    virtual WebString doNotTrackValue() { return WebString(); }


    // WebGL ------------------------------------------------------

    // Asks the embedder whether WebGL is allowed for the WebFrame. This call is
    // placed here instead of WebContentSettingsClient because this class is
    // implemented in content/, and putting it here avoids adding more public
    // content/ APIs.
    virtual bool allowWebGL(bool defaultValue) { return defaultValue; }

    // Screen Orientation --------------------------------------------------

    // Access the embedder API for (client-based) screen orientation client .
    virtual WebScreenOrientationClient* webScreenOrientationClient() { return 0; }


    // Accessibility -------------------------------------------------------

    // Notifies embedder about an accessibility event.
    virtual void postAccessibilityEvent(const WebAXObject&, WebAXEvent) { }

    // Provides accessibility information about a find in page result.
    virtual void handleAccessibilityFindInPageResult(
        int identifier,
        int matchIndex,
        const WebAXObject& startObject,
        int startOffset,
        const WebAXObject& endObject,
        int endOffset) { }


    // ServiceWorker -------------------------------------------------------

    // Whether the document associated with WebDataSource is controlled by the
    // ServiceWorker.
    virtual bool isControlledByServiceWorker(WebDataSource&) { return false; }

    // Returns an identifier of the service worker controlling the document
    // associated with the WebDataSource.
    virtual int64_t serviceWorkerID(WebDataSource&) { return -1; }


    // Fullscreen ----------------------------------------------------------

    // Called to enter/exit fullscreen mode.
    // After calling enterFullscreen, WebWidget::{will,Did}EnterFullScreen
    // should bound resizing the WebWidget into fullscreen mode.
    // Similarly, when exitFullScreen is called,
    // WebWidget::{will,Did}ExitFullScreen should bound resizing the WebWidget
    // out of fullscreen mode.
    // Note: the return value is ignored.
    virtual bool enterFullscreen() { return false; }
    virtual bool exitFullscreen() { return false; }


    // Sudden termination --------------------------------------------------

    // Called when elements preventing the sudden termination of the frame
    // become present or stop being present. |type| is the type of element
    // (BeforeUnload handler, Unload handler).
    enum SuddenTerminationDisablerType {
        BeforeUnloadHandler,
        UnloadHandler,
    };
    virtual void suddenTerminationDisablerChanged(bool present, SuddenTerminationDisablerType) { }


    // Permissions ---------------------------------------------------------

    // Access the embedder API for permission client.
    virtual WebPermissionClient* permissionClient() { return 0; }

    // App Banners ---------------------------------------------------------
    virtual WebAppBannerClient* appBannerClient() { return 0; }

    // Navigator Content Utils  --------------------------------------------

    // Registers a new URL handler for the given protocol.
    virtual void registerProtocolHandler(const WebString& scheme,
        const WebURL& url,
        const WebString& title) { }

    // Unregisters a given URL handler for the given protocol.
    virtual void unregisterProtocolHandler(const WebString& scheme, const WebURL& url) { }

    // Check if a given URL handler is registered for the given protocol.
    virtual WebCustomHandlersState isProtocolHandlerRegistered(const WebString& scheme, const WebURL& url)
    {
        return WebCustomHandlersNew;
    }

    // Bluetooth -----------------------------------------------------------
    virtual WebBluetooth* bluetooth() { return 0; }


    // Audio Output Devices API --------------------------------------------

    // Checks that the given audio sink exists and is authorized. The result is provided via the callbacks.
    // This method takes ownership of the callbacks pointer.
    virtual void checkIfAudioSinkExistsAndIsAuthorized(const WebString& sinkId, const WebSecurityOrigin&, WebSetSinkIdCallbacks* callbacks)
    {
        if (callbacks) {
            callbacks->onError(WebSetSinkIdError::NotSupported);
            delete callbacks;
        }
    }

    // Mojo ----------------------------------------------------------------
    virtual ServiceRegistry* serviceRegistry() { return nullptr; }

    // Visibility ----------------------------------------------------------

    // Returns the current visibility of the WebFrame.
    virtual WebPageVisibilityState visibilityState() const
    {
        return WebPageVisibilityStateVisible;
    }

protected:
    virtual ~WebFrameClient() { }
};

} // namespace blink

#endif
