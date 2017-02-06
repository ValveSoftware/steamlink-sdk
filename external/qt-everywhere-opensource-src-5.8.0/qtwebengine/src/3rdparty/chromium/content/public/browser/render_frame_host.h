// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include <string>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/file_chooser_params.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Value;
}

namespace shell {
class InterfaceRegistry;
class InterfaceProvider;
}

namespace content {
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHostView;
class SiteInstance;
struct FileChooserFileInfo;

// The interface provides a communication conduit with a frame in the renderer.
class CONTENT_EXPORT RenderFrameHost : public IPC::Listener,
                                       public IPC::Sender {
 public:
  // Returns the RenderFrameHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromID(int render_process_id, int render_frame_id);


#if defined(OS_ANDROID) || defined(TOOLKIT_QT)
  // Globally allows for injecting JavaScript into the main world. This feature
  // is present only to support Android WebView and must not be used in other
  // configurations.
  static void AllowInjectingJavaScriptForAndroidWebView();
#endif

  // Returns a RenderFrameHost given its accessibility tree ID.
  static RenderFrameHost* FromAXTreeID(int ax_tree_id);

  ~RenderFrameHost() override {}

  // Returns the route id for this frame.
  virtual int GetRoutingID() = 0;

  // Returns the accessibility tree ID for this RenderFrameHost.
  virtual int GetAXTreeID() = 0;

  // Returns the SiteInstance grouping all RenderFrameHosts that have script
  // access to this RenderFrameHost, and must therefore live in the same
  // process.
  virtual SiteInstance* GetSiteInstance() = 0;

  // Returns the process for this frame.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the RenderWidgetHostView that can be used to control focus and
  // visibility for this frame.
  virtual RenderWidgetHostView* GetView() = 0;

  // Returns the current RenderFrameHost of the parent frame, or nullptr if
  // there is no parent. The result may be in a different process than the
  // current RenderFrameHost.
  virtual RenderFrameHost* GetParent() = 0;

  // Returns the FrameTreeNode ID for this frame. This ID is browser-global and
  // uniquely identifies a frame that hosts content. The identifier is fixed at
  // the creation of the frame and stays constant for the lifetime of the frame.
  // When the frame is removed, the ID is not used again.
  //
  // A RenderFrameHost is tied to a process. Due to cross-process navigations,
  // the RenderFrameHost may have a shorter lifetime than a frame. Consequently,
  // the same FrameTreeNode ID may refer to a different RenderFrameHost after a
  // navigation.
  virtual int GetFrameTreeNodeId() = 0;

  // Returns the assigned name of the frame, the name of the iframe tag
  // declaring it. For example, <iframe name="framename">[...]</iframe>. It is
  // quite possible for a frame to have no name, in which case GetFrameName will
  // return an empty string.
  virtual const std::string& GetFrameName() = 0;

  // Returns true if the frame is out of process.
  virtual bool IsCrossProcessSubframe() = 0;

  // Returns the last committed URL of the frame.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Returns the last committed origin of the frame.
  //
  // The origin is only available if this RenderFrameHost is current in the
  // frame tree -- i.e., it would be visited by WebContents::ForEachFrame. In
  // particular, this method may CHECK if called from
  // WebContentsObserver::RenderFrameCreated, since non-current frames can be
  // passed to that observer method.
  virtual url::Origin GetLastCommittedOrigin() = 0;

  // Returns the associated widget's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Adds |message| to the DevTools console.
  virtual void AddMessageToConsole(ConsoleMessageLevel level,
                                   const std::string& message) = 0;

  // Runs some JavaScript in this frame's context. If a callback is provided, it
  // will be used to return the result, when the result is available.
  // This API can only be called on chrome:// or chrome-devtools:// URLs.
  typedef base::Callback<void(const base::Value*)> JavaScriptResultCallback;
  virtual void ExecuteJavaScript(const base::string16& javascript) = 0;
  virtual void ExecuteJavaScript(const base::string16& javascript,
                                 const JavaScriptResultCallback& callback) = 0;

  // Runs some JavaScript in an isolated world of top of this frame's context.
  virtual void ExecuteJavaScriptInIsolatedWorld(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback,
      int world_id) = 0;

  // ONLY FOR TESTS: Same as above but without restrictions. Optionally, adds a
  // fake UserGestureIndicator around execution. (crbug.com/408426)
  virtual void ExecuteJavaScriptForTests(const base::string16& javascript) = 0;
  virtual void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback) = 0;
  virtual void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript) = 0;

  // Accessibility actions - these send a message to the RenderFrame
  // to trigger an action on an accessibility object.
  virtual void AccessibilitySetFocus(int acc_obj_id) = 0;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) = 0;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, const gfx::Rect& subfocus) = 0;
  virtual void AccessibilityShowContextMenu(int acc_obj_id) = 0;
  virtual void AccessibilitySetSelection(int anchor_object_id,
                                         int anchor_offset,
                                         int focus_object_id,
                                         int focus_offset) = 0;

  // This is called when the user has committed to the given find in page
  // request (e.g. by pressing enter or by clicking on the next / previous
  // result buttons). It triggers sending a native accessibility event on
  // the result object on the page, navigating assistive technology to that
  // result.
  virtual void ActivateFindInPageResultForAccessibility(int request_id) = 0;

  // Roundtrips through the renderer and compositor pipeline to ensure that any
  // changes to the contents resulting from operations executed prior to this
  // call are visible on screen. The call completes asynchronously by running
  // the supplied |callback| with a value of true upon successful completion and
  // false otherwise (when the frame is destroyed, detached, etc..).
  typedef base::Callback<void(bool)> VisualStateCallback;
  virtual void InsertVisualStateCallback(
      const VisualStateCallback& callback) = 0;

  // Copies the image at the location in viewport coordinates (not frame
  // coordinates) to the clipboard. If there is no image at that location, does
  // nothing.
  virtual void CopyImageAt(int x, int y) = 0;

  // Requests to save the image at the location in viewport coordinates (not
  // frame coordinates). If there is an image at the location, the renderer
  // will post back the appropriate download message to trigger the save UI.
  // If there is no image at that location, does nothing.
  virtual void SaveImageAt(int x, int y) = 0;

  // RenderViewHost for this frame.
  virtual RenderViewHost* GetRenderViewHost() = 0;

  // Returns the InterfaceRegistry that this process uses to expose interfaces
  // to the application running in this frame.
  virtual shell::InterfaceRegistry* GetInterfaceRegistry() = 0;

  // Returns the InterfaceProvider that this process can use to bind
  // interfaces exposed to it by the application running in this frame.
  virtual shell::InterfaceProvider* GetRemoteInterfaces() = 0;

  // Returns the visibility state of the frame. The different visibility states
  // of a frame are defined in Blink.
  virtual blink::WebPageVisibilityState GetVisibilityState() = 0;

  // Returns whether the RenderFrame in the renderer process has been created
  // and still has a connection.  This is valid for all frames.
  virtual bool IsRenderFrameLive() = 0;

  // Get the number of proxies to this frame, in all processes. Exposed for
  // use by resource metrics.
  virtual int GetProxyCount() = 0;

  // Notifies the Listener that one or more files have been chosen by the user
  // from a file chooser dialog for the form. |permissions| is the file
  // selection mode in which the chooser dialog was created.
  virtual void FilesSelectedInChooser(
      const std::vector<content::FileChooserFileInfo>& files,
      FileChooserParams::Mode permissions) = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
