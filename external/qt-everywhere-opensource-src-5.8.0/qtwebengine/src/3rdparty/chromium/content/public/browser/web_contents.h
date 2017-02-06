// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_

#include <stdint.h>

#include <set>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/stop_find_action.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace base {
class DictionaryValue;
class TimeTicks;
}

namespace blink {
struct WebFindOptions;
}

namespace net {
struct LoadStateWithParam;
}

namespace content {

class BrowserContext;
class BrowserPluginGuestDelegate;
class InterstitialPage;
class PageState;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHostView;
class WebContentsDelegate;
struct CustomContextMenuContext;
struct DropData;
struct Manifest;
struct MHTMLGenerationParams;
struct PageImportanceSignals;
struct RendererPreferences;

// WebContents is the core class in content/. A WebContents renders web content
// (usually HTML) in a rectangular area.
//
// Instantiating one is simple:
//   std::unique_ptr<content::WebContents> web_contents(
//       content::WebContents::Create(
//           content::WebContents::CreateParams(browser_context)));
//   gfx::NativeView view = web_contents->GetNativeView();
//   // |view| is an HWND, NSView*, GtkWidget*, etc.; insert it into the view
//   // hierarchy wherever it needs to go.
//
// That's it; go to your kitchen, grab a scone, and chill. WebContents will do
// all the multi-process stuff behind the scenes. More details are at
// http://www.chromium.org/developers/design-documents/multi-process-architecture
// .
//
// Each WebContents has exactly one NavigationController; each
// NavigationController belongs to one WebContents. The NavigationController can
// be obtained from GetController(), and is used to load URLs into the
// WebContents, navigate it backwards/forwards, etc. See navigation_controller.h
// for more details.
class WebContents : public PageNavigator,
                    public IPC::Sender,
                    public base::SupportsUserData {
 public:
  struct CONTENT_EXPORT CreateParams {
    explicit CreateParams(BrowserContext* context);
    CreateParams(const CreateParams& other);
    ~CreateParams();
    CreateParams(BrowserContext* context, scoped_refptr<SiteInstance> site);

    BrowserContext* browser_context;

    // Specifying a SiteInstance here is optional.  It can be set to avoid an
    // extra process swap if the first navigation is expected to require a
    // privileged process.
    scoped_refptr<SiteInstance> site_instance;

    // The process id of the frame initiating the open.
    int opener_render_process_id;

    // The routing id of the frame initiating the open.
    int opener_render_frame_id;

    // If the opener is suppressed, then the new WebContents doesn't hold a
    // reference to its opener.
    bool opener_suppressed;

    // Indicates whether this WebContents was created with a window.opener.
    // This is used when determining whether the WebContents is allowed to be
    // closed via window.close(). This may be true even with a null |opener|
    // (e.g., for blocked popups).
    bool created_with_opener;

    // The routing ids of the RenderView, main RenderFrame, and the widget for
    // the main RenderFrame. Either all routing IDs must be provided or all must
    // be MSG_ROUTING_NONE to have WebContents make the assignment. If provided,
    // these routing IDs are associated with |site_instance->GetProcess()|.
    int32_t routing_id;
    int32_t main_frame_routing_id;
    int32_t main_frame_widget_routing_id;

    // The name of the top-level frame of the new window. It is non-empty
    // when creating a named window (e.g. <a target="foo"> or
    // window.open('', 'bar')).
    std::string main_frame_name;

    // Initial size of the new WebContent's view. Can be (0, 0) if not needed.
    gfx::Size initial_size;

    // True if the contents should be initially hidden.
    bool initially_hidden;

    // If non-null then this WebContents will be hosted by a BrowserPlugin.
    BrowserPluginGuestDelegate* guest_delegate;

    // Used to specify the location context which display the new view should
    // belong. This can be nullptr if not needed.
    gfx::NativeView context;

    // Used to specify that the new WebContents creation is driven by the
    // renderer process. In this case, the renderer-side objects, such as
    // RenderFrame, have already been created on the renderer side, and
    // WebContents construction should take this into account.
    bool renderer_initiated_creation;

    // True if the WebContents should create its renderer process and main
    // RenderFrame before the first navigation. This is useful to reduce
    // the latency of the first navigation in cases where it might
    // not happen right away.
    // Note that the pre-created renderer process may not be used if the first
    // navigation requires a dedicated or privileged process, such as a WebUI.
    bool initialize_renderer;
  };

  // Creates a new WebContents.
  CONTENT_EXPORT static WebContents* Create(const CreateParams& params);

  // Similar to Create() above but should be used when you need to prepopulate
  // the SessionStorageNamespaceMap of the WebContents. This can happen if
  // you duplicate a WebContents, try to reconstitute it from a saved state,
  // or when you create a new WebContents based on another one (eg., when
  // servicing a window.open() call).
  //
  // You do not want to call this. If you think you do, make sure you completely
  // understand when SessionStorageNamespace objects should be cloned, why
  // they should not be shared by multiple WebContents, and what bad things
  // can happen if you share the object.
  CONTENT_EXPORT static WebContents* CreateWithSessionStorage(
      const CreateParams& params,
      const SessionStorageNamespaceMap& session_storage_namespace_map);

  // Returns the WebContents that owns the RenderViewHost, or nullptr if the
  // render view host's delegate isn't a WebContents.
  CONTENT_EXPORT static WebContents* FromRenderViewHost(RenderViewHost* rvh);

  CONTENT_EXPORT static WebContents* FromRenderFrameHost(RenderFrameHost* rfh);

  ~WebContents() override {}

  // Intrinsic tab state -------------------------------------------------------

  // Gets/Sets the delegate.
  virtual WebContentsDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebContentsDelegate* delegate) = 0;

  // Gets the controller for this WebContents.
  virtual NavigationController& GetController() = 0;
  virtual const NavigationController& GetController() const = 0;

  // Returns the user browser context associated with this WebContents (via the
  // NavigationController).
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Gets the URL that is currently being displayed, if there is one.
  // This method is deprecated. DO NOT USE! Pick either |GetVisibleURL| or
  // |GetLastCommittedURL| as appropriate.
  virtual const GURL& GetURL() const = 0;

  // Gets the virtual URL currently being displayed in the URL bar, if there is
  // one. This URL might be a pending navigation that hasn't committed yet, so
  // it is not guaranteed to match the current page in this WebContents. A
  // typical example of this is interstitials, which show the URL of the
  // new/loading page (active) but the security context is of the old page (last
  // committed).
  virtual const GURL& GetVisibleURL() const = 0;

  // Gets the virtual URL of the last committed page in this WebContents.
  // Virtual URLs are meant to be displayed to the user (e.g., they include the
  // "view-source:" prefix for view source URLs, unlike NavigationEntry::GetURL
  // and NavigationHandle::GetURL). The last committed page is the current
  // security context and the content that is actually displayed within the tab.
  // See also GetVisibleURL above, which may differ from this URL.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  virtual RenderProcessHost* GetRenderProcessHost() const = 0;

  // Returns the main frame for the currently active view.
  virtual RenderFrameHost* GetMainFrame() = 0;

  // Returns the focused frame for the currently active view.
  virtual RenderFrameHost* GetFocusedFrame() = 0;

  // Returns the current RenderFrameHost for a given FrameTreeNode ID if it is
  // part of this tab. See RenderFrameHost::GetFrameTreeNodeId for documentation
  // on this ID.
  virtual RenderFrameHost* FindFrameByFrameTreeNodeId(
      int frame_tree_node_id) = 0;

  // Calls |on_frame| for each frame in the currently active view.
  // Note: The RenderFrameHost parameter is not guaranteed to have a live
  // RenderFrame counterpart in the renderer process. Callbacks should check
  // IsRenderFrameLive, as sending IPC messages to it in this case will fail
  // silently.
  virtual void ForEachFrame(
      const base::Callback<void(RenderFrameHost*)>& on_frame) = 0;

  // Returns a vector of all RenderFrameHosts in the currently active view in
  // breadth-first traversal order.
  virtual std::vector<RenderFrameHost*> GetAllFrames() = 0;

  // Sends the given IPC to all live frames in this WebContents and returns the
  // number of sent messages (i.e. the number of processed frames).
  virtual int SendToAllFrames(IPC::Message* message) = 0;

  // Gets the current RenderViewHost for this tab.
  virtual RenderViewHost* GetRenderViewHost() const = 0;

  // Gets the current RenderViewHost's routing id. Returns
  // MSG_ROUTING_NONE when there is no RenderViewHost.
  virtual int GetRoutingID() const = 0;

  // Returns the currently active RenderWidgetHostView. This may change over
  // time and can be nullptr (during setup and teardown).
  virtual RenderWidgetHostView* GetRenderWidgetHostView() const = 0;

  // Returns the outermost RenderWidgetHostView. This will return the platform
  // specific RenderWidgetHostView (as opposed to
  // RenderWidgetHostViewChildFrame), which can be used to create context
  // menus.
  virtual RenderWidgetHostView* GetTopLevelRenderWidgetHostView() = 0;

  // Causes the current page to be closed, including running its onunload event
  // handler.
  virtual void ClosePage() = 0;

  // Returns the currently active fullscreen widget. If there is none, returns
  // nullptr.
  virtual RenderWidgetHostView* GetFullscreenRenderWidgetHostView() const = 0;

  // Returns the theme color for the underlying content as set by the
  // theme-color meta tag.
  virtual SkColor GetThemeColor() const = 0;

  // Create a WebUI page for the given url. In most cases, this doesn't need to
  // be called by embedders since content will create its own WebUI objects as
  // necessary. However if the embedder wants to create its own WebUI object and
  // keep track of it manually, it can use this. |frame_name| is used to
  // identify the frame and cannot be empty.
  virtual WebUI* CreateSubframeWebUI(const GURL& url,
                                     const std::string& frame_name) = 0;

  // Returns the committed WebUI if one exists, otherwise the pending one.
  virtual WebUI* GetWebUI() const = 0;
  virtual WebUI* GetCommittedWebUI() const = 0;

  // Allows overriding the user agent used for NavigationEntries it owns.
  virtual void SetUserAgentOverride(const std::string& override) = 0;
  virtual const std::string& GetUserAgentOverride() const = 0;

  // Enable the accessibility tree for this WebContents in the renderer,
  // but don't enable creating a native accessibility tree on the browser
  // side.
  virtual void EnableTreeOnlyAccessibilityMode() = 0;

  // Returns true only if "tree only" accessibility mode is on.
  virtual bool IsTreeOnlyAccessibilityModeForTesting() const = 0;

  // Returns true only if complete accessibility mode is on, meaning there's
  // both renderer accessibility, and a native browser accessibility tree.
  virtual bool IsFullAccessibilityModeForTesting() const = 0;

  virtual const PageImportanceSignals& GetPageImportanceSignals() const = 0;

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const base::string16& GetTitle() const = 0;

  // The max page ID for any page that the current SiteInstance has loaded in
  // this WebContents.  Page IDs are specific to a given SiteInstance and
  // WebContents, corresponding to a specific RenderView in the renderer.
  // Page IDs increase with each new page that is loaded by a tab.
  virtual int32_t GetMaxPageID() = 0;

  // The max page ID for any page that the given SiteInstance has loaded in
  // this WebContents.
  virtual int32_t GetMaxPageIDForSiteInstance(SiteInstance* site_instance) = 0;

  // Returns the SiteInstance associated with the current page.
  virtual SiteInstance* GetSiteInstance() const = 0;

  // Returns the SiteInstance for the pending navigation, if any.  Otherwise
  // returns the current SiteInstance.
  virtual SiteInstance* GetPendingSiteInstance() const = 0;

  // Returns whether this WebContents is loading a resource.
  virtual bool IsLoading() const = 0;

  // Returns whether this WebContents is loading and and the load is to a
  // different top-level document (rather than being a navigation within the
  // same document). This being true implies that IsLoading() is also true.
  virtual bool IsLoadingToDifferentDocument() const = 0;

  // Returns whether this WebContents is waiting for a first-response for the
  // main resource of the page.
  virtual bool IsWaitingForResponse() const = 0;

  // Returns the current load state and the URL associated with it.
  // The load state is only updated while IsLoading() is true.
  virtual const net::LoadStateWithParam& GetLoadState() const = 0;
  virtual const base::string16& GetLoadStateHost() const = 0;

  // Returns the upload progress.
  virtual uint64_t GetUploadSize() const = 0;
  virtual uint64_t GetUploadPosition() const = 0;

  // Returns the character encoding of the page.
  virtual const std::string& GetEncoding() const = 0;

  // True if this is a secure page which displayed insecure content.
  virtual bool DisplayedInsecureContent() const = 0;

  // Internal state ------------------------------------------------------------

  // Indicates whether the WebContents is being captured (e.g., for screenshots
  // or mirroring).  Increment calls must be balanced with an equivalent number
  // of decrement calls.  |capture_size| specifies the capturer's video
  // resolution, but can be empty to mean "unspecified."  The first screen
  // capturer that provides a non-empty |capture_size| will override the value
  // returned by GetPreferredSize() until all captures have ended.
  virtual void IncrementCapturerCount(const gfx::Size& capture_size) = 0;
  virtual void DecrementCapturerCount() = 0;
  virtual int GetCapturerCount() const = 0;

  // Indicates/Sets whether all audio output from this WebContents is muted.
  virtual bool IsAudioMuted() const = 0;
  virtual void SetAudioMuted(bool mute) = 0;

  // Indicates whether any frame in the WebContents is connected to a Bluetooth
  // Device.
  virtual bool IsConnectedToBluetoothDevice() const = 0;

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  virtual bool IsCrashed() const  = 0;
  virtual void SetIsCrashed(base::TerminationStatus status, int error_code) = 0;

  virtual base::TerminationStatus GetCrashedStatus() const = 0;
  virtual int GetCrashedErrorCode() const = 0;

  // Whether the tab is in the process of being destroyed.
  virtual bool IsBeingDestroyed() const = 0;

  // Convenience method for notifying the delegate of a navigation state
  // change.
  virtual void NotifyNavigationStateChanged(InvalidateTypes changed_flags) = 0;

  // Get/Set the last time that the WebContents was made active (either when it
  // was created or shown with WasShown()).
  virtual base::TimeTicks GetLastActiveTime() const = 0;
  virtual void SetLastActiveTime(base::TimeTicks last_active_time) = 0;

  // Invoked when the WebContents becomes shown/hidden.
  virtual void WasShown() = 0;
  virtual void WasHidden() = 0;

  // Returns true if the before unload and unload listeners need to be
  // fired. The value of this changes over time. For example, if true and the
  // before unload listener is executed and allows the user to exit, then this
  // returns false.
  virtual bool NeedToFireBeforeUnload() = 0;

  // Runs the beforeunload handler for the main frame. See also ClosePage and
  // SwapOut in RenderViewHost, which run the unload handler.
  //
  // TODO(creis): We should run the beforeunload handler for every frame that
  // has one.
  virtual void DispatchBeforeUnload() = 0;

  // Attaches this inner WebContents to its container frame
  // |outer_contents_frame| in |outer_web_contents|.
  virtual void AttachToOuterWebContentsFrame(
      WebContents* outer_web_contents,
      RenderFrameHost* outer_contents_frame) = 0;

  // Commands ------------------------------------------------------------------

  // Stop any pending navigation.
  virtual void Stop() = 0;

  // Creates a new WebContents with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  virtual WebContents* Clone() = 0;

  // Reloads the focused frame.
  virtual void ReloadFocusedFrame(bool bypass_cache) = 0;

  // Reloads all the Lo-Fi images in this WebContents. Ignores the cache and
  // reloads from the network.
  virtual void ReloadLoFiImages() = 0;

  // Editing commands ----------------------------------------------------------

  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void CopyToFindPboard() = 0;
  virtual void Paste() = 0;
  virtual void PasteAndMatchStyle() = 0;
  virtual void Delete() = 0;
  virtual void SelectAll() = 0;
  virtual void Unselect() = 0;

  // Adjust the selection starting and ending points in the focused frame by
  // the given amounts. A negative amount moves the selection towards the
  // beginning of the document, a positive amount moves the selection towards
  // the end of the document.
  virtual void AdjustSelectionByCharacterOffset(int start_adjust,
                                                int end_adjust) = 0;

  // Replaces the currently selected word or a word around the cursor.
  virtual void Replace(const base::string16& word) = 0;

  // Replaces the misspelling in the current selection.
  virtual void ReplaceMisspelling(const base::string16& word) = 0;

  // Let the renderer know that the menu has been closed.
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) = 0;

  // Executes custom context menu action that was provided from Blink.
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) = 0;

  // Views and focus -----------------------------------------------------------

  // Returns the native widget that contains the contents of the tab.
  virtual gfx::NativeView GetNativeView() = 0;

  // Returns the native widget with the main content of the tab (i.e. the main
  // render view host, though there may be many popups in the tab as children of
  // the container).
  virtual gfx::NativeView GetContentNativeView() = 0;

  // Returns the outermost native view. This will be used as the parent for
  // dialog boxes.
  virtual gfx::NativeWindow GetTopLevelNativeWindow() = 0;

  // Computes the rectangle for the native widget that contains the contents of
  // the tab in the screen coordinate system.
  virtual gfx::Rect GetContainerBounds() = 0;

  // Get the bounds of the View, relative to the parent.
  virtual gfx::Rect GetViewBounds() = 0;

  // Returns the current drop data, if any.
  virtual DropData* GetDropData() = 0;

  // Sets focus to the native widget for this tab.
  virtual void Focus() = 0;

  // Sets focus to the appropriate element when the WebContents is shown the
  // first time.
  virtual void SetInitialFocus() = 0;

  // Stores the currently focused view.
  virtual void StoreFocus() = 0;

  // Restores focus to the last focus view. If StoreFocus has not yet been
  // invoked, SetInitialFocus is invoked.
  virtual void RestoreFocus() = 0;

  // Focuses the first (last if |reverse| is true) element in the page.
  // Invoked when this tab is getting the focus through tab traversal (|reverse|
  // is true when using Shift-Tab).
  virtual void FocusThroughTabTraversal(bool reverse) = 0;

  // Interstitials -------------------------------------------------------------

  // Various other systems need to know about our interstitials.
  virtual bool ShowingInterstitialPage() const = 0;

  // Returns the currently showing interstitial, nullptr if no interstitial is
  // showing.
  virtual InterstitialPage* GetInterstitialPage() const = 0;

  // Misc state & callbacks ----------------------------------------------------

  // Check whether we can do the saving page operation this page given its MIME
  // type.
  virtual bool IsSavable() = 0;

  // Prepare for saving the current web page to disk.
  virtual void OnSavePage() = 0;

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page. Returns true if the
  // saving process has been initiated successfully.
  virtual bool SavePage(const base::FilePath& main_file,
                        const base::FilePath& dir_path,
                        SavePageType save_type) = 0;

  // Saves the given frame's URL to the local filesystem.
  virtual void SaveFrame(const GURL& url,
                         const Referrer& referrer) = 0;

  // Saves the given frame's URL to the local filesystem. The headers, if
  // provided, is used to make a request to the URL rather than using cache.
  // Format of |headers| is a new line separated list of key value pairs:
  // "<key1>: <value1>\n<key2>: <value2>".
  virtual void SaveFrameWithHeaders(const GURL& url,
                                    const Referrer& referrer,
                                    const std::string& headers) = 0;

  // Generate an MHTML representation of the current page in the given file.
  // If |use_binary_encoding| is specified, a Content-Transfer-Encoding value of
  // 'binary' will be used, instead of a combination of 'quoted-printable' and
  // 'base64'.  Binary encoding is known to have interoperability issues and is
  // not the recommended encoding for shareable content.
  virtual void GenerateMHTML(
      const MHTMLGenerationParams& params,
      const base::Callback<void(int64_t /* size of the file */)>& callback) = 0;

  // Returns the contents MIME type after a navigation.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Returns true if this WebContents will notify about disconnection.
  virtual bool WillNotifyDisconnection() const = 0;

  // Override the encoding and reload the page by sending down
  // ViewMsg_SetPageEncoding to the renderer. |UpdateEncoding| is kinda
  // the opposite of this, by which 'browser' is notified of
  // the encoding of the current tab from 'renderer' (determined by
  // auto-detect, http header, meta, bom detection, etc).
  virtual void SetOverrideEncoding(const std::string& encoding) = 0;

  // Remove any user-defined override encoding and reload by sending down
  // ViewMsg_ResetPageEncodingToDefault to the renderer.
  virtual void ResetOverrideEncoding() = 0;

  // Returns the settings which get passed to the renderer.
  virtual content::RendererPreferences* GetMutableRendererPrefs() = 0;

  // Tells the tab to close now. The tab will take care not to close until it's
  // out of nested message loops.
  virtual void Close() = 0;

  // A render view-originated drag has ended. Informs the render view host and
  // WebContentsDelegate.
  virtual void SystemDragEnded() = 0;

  // Notification the user has made a gesture while focus was on the
  // page. This is used to avoid uninitiated user downloads (aka carpet
  // bombing), see DownloadRequestLimiter for details.
  virtual void UserGestureDone() = 0;

  // Indicates if this tab was explicitly closed by the user (control-w, close
  // tab menu item...). This is false for actions that indirectly close the tab,
  // such as closing the window.  The setter is maintained by TabStripModel, and
  // the getter only useful from within TAB_CLOSED notification
  virtual void SetClosedByUserGesture(bool value) = 0;
  virtual bool GetClosedByUserGesture() const = 0;

  // Opens view-source tab for this contents.
  virtual void ViewSource() = 0;

  virtual void ViewFrameSource(const GURL& url,
                               const PageState& page_state) = 0;

  // Gets the minimum/maximum zoom percent.
  virtual int GetMinimumZoomPercent() const = 0;
  virtual int GetMaximumZoomPercent() const = 0;

  // Set the renderer's page scale to the given factor.
  virtual void SetPageScale(float page_scale_factor) = 0;

  // Gets the preferred size of the contents.
  virtual gfx::Size GetPreferredSize() const = 0;

  // Called when the reponse to a pending mouse lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  virtual bool GotResponseToLockMouseRequest(bool allowed) = 0;

  // Called when the user has selected a color in the color chooser.
  virtual void DidChooseColorInColorChooser(SkColor color) = 0;

  // Called when the color chooser has ended.
  virtual void DidEndColorChooser() = 0;

  // Returns true if the location bar should be focused by default rather than
  // the page contents. The view calls this function when the tab is focused
  // to see what it should do.
  virtual bool FocusLocationBarByDefault() = 0;

  // Does this have an opener associated with it?
  virtual bool HasOpener() const = 0;

  // Returns the opener if HasOpener() is true, or nullptr otherwise.
  virtual WebContents* GetOpener() const = 0;

  typedef base::Callback<void(
      int, /* id */
      int, /* HTTP status code */
      const GURL&, /* image_url */
      const std::vector<SkBitmap>&, /* bitmaps */
      /* The sizes in pixel of the bitmaps before they were resized due to the
         max bitmap size passed to DownloadImage(). Each entry in the bitmaps
         vector corresponds to an entry in the sizes vector. If a bitmap was
         resized, there should be a single returned bitmap. */
      const std::vector<gfx::Size>&)>
          ImageDownloadCallback;

  // Sends a request to download the given image |url| and returns the unique
  // id of the download request. When the download is finished, |callback| will
  // be called with the bitmaps received from the renderer.
  // If |is_favicon| is true, the cookies are not sent and not accepted during
  // download.
  // Bitmaps with pixel sizes larger than |max_bitmap_size| are filtered out
  // from the bitmap results. If there are no bitmap results <=
  // |max_bitmap_size|, the smallest bitmap is resized to |max_bitmap_size| and
  // is the only result. A |max_bitmap_size| of 0 means unlimited.
  // If |bypass_cache| is true, |url| is requested from the server even if it
  // is present in the browser cache.
  virtual int DownloadImage(const GURL& url,
                            bool is_favicon,
                            uint32_t max_bitmap_size,
                            bool bypass_cache,
                            const ImageDownloadCallback& callback) = 0;

  // Returns true if the WebContents is responsible for displaying a subframe
  // in a different process from its parent page.
  // TODO: this doesn't really belong here. With site isolation, this should be
  // removed since we can then embed iframes in different processes.
  virtual bool IsSubframe() const = 0;

  // Finds text on a page. |search_text| should not be empty.
  virtual void Find(int request_id,
                    const base::string16& search_text,
                    const blink::WebFindOptions& options) = 0;

  // Notifies the renderer that the user has closed the FindInPage window
  // (and what action to take regarding the selection).
  virtual void StopFinding(StopFindAction action) = 0;

  // Requests the renderer to insert CSS into the main frame's document.
  virtual void InsertCSS(const std::string& css) = 0;

  // Returns true if audio has recently been audible from the WebContents.
  virtual bool WasRecentlyAudible() = 0;

  typedef base::Callback<void(const Manifest&)> GetManifestCallback;

  // Requests the Manifest of the main frame's document.
  virtual void GetManifest(const GetManifestCallback& callback) = 0;

  typedef base::Callback<void(bool)> HasManifestCallback;

  // Returns true if the main frame has a <link> to a web manifest, otherwise
  // false. This method does not guarantee that the manifest exists at the
  // specified location or is valid.
  virtual void HasManifest(const HasManifestCallback& callback) = 0;

  // Requests the renderer to exit fullscreen.
  // |will_cause_resize| indicates whether the fullscreen change causes a
  // view resize. e.g. This will be false when going from tab fullscreen to
  // browser fullscreen.
  virtual void ExitFullscreen(bool will_cause_resize) = 0;
  virtual void NotifyFullscreenChanged(bool will_cause_resize) = 0;

  // Unblocks requests from renderer for a newly created window. This is
  // used in showCreatedWindow() or sometimes later in cases where
  // delegate->ShouldResumeRequestsForCreatedWindow() indicated the requests
  // should not yet be resumed. Then the client is responsible for calling this
  // as soon as they are ready.
  virtual void ResumeLoadingCreatedWebContents() = 0;

  // Requests to resume the current media session.
  virtual void ResumeMediaSession() = 0;
  // Requests to suspend the current media session.
  virtual void SuspendMediaSession() = 0;
  // Requests to stop the current media session.
  virtual void StopMediaSession() = 0;

#if defined(OS_ANDROID)
  CONTENT_EXPORT static WebContents* FromJavaWebContents(
      jobject jweb_contents_android);
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents() = 0;

  // Selects and zooms to the find result nearest to the point (x,y) defined in
  // find-in-page coordinates.
  virtual void ActivateNearestFindResult(float x, float y) = 0;

  // Requests the rects of the current find matches from the renderer
  // process. |current_version| is the version of find rects that the caller
  // already knows about. This version will be compared to the current find
  // rects version in the renderer process (which is updated whenever the rects
  // change), to see which new rect data will need to be sent back.
  //
  // TODO(paulmeyer): This process will change slightly once multi-process
  // find-in-page is implemented. This comment should be updated at that time.
  virtual void RequestFindMatchRects(int current_version) = 0;
#elif defined(OS_MACOSX)
  // Allowing other views disables optimizations which assume that only a single
  // WebContents is present.
  virtual void SetAllowOtherViews(bool allow) = 0;

  // Returns true if other views are allowed, false otherwise.
  virtual bool GetAllowOtherViews() = 0;
#endif  // OS_ANDROID

 private:
  // This interface should only be implemented inside content.
  friend class WebContentsImpl;
  WebContents() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
