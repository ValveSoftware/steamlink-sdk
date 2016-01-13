// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_

#include <list>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/page_zoom.h"
#include "mojo/public/cpp/system/core.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"

class GURL;
struct WebPreferences;

namespace gfx {
class Point;
}

namespace base {
class FilePath;
class Value;
}

namespace media {
class AudioOutputController;
}

namespace ui {
struct SelectedFileInfo;
}

namespace blink {
struct WebMediaPlayerAction;
struct WebPluginAction;
}

namespace content {

class ChildProcessSecurityPolicy;
class RenderFrameHost;
class RenderViewHostDelegate;
class SessionStorageNamespace;
class SiteInstance;
struct DropData;

// A RenderViewHost is responsible for creating and talking to a RenderView
// object in a child process. It exposes a high level API to users, for things
// like loading pages, adjusting the display and other browser functionality,
// which it translates into IPC messages sent over the IPC channel with the
// RenderView. It responds to all IPC messages sent by that RenderView and
// cracks them, calling a delegate object back with higher level types where
// possible.
//
// The intent of this interface is to provide a view-agnostic communication
// conduit with a renderer. This is so we can build HTML views not only as
// WebContents (see WebContents for an example) but also as views, etc.
class CONTENT_EXPORT RenderViewHost : virtual public RenderWidgetHost {
 public:
  // Returns the RenderViewHost given its ID and the ID of its render process.
  // Returns NULL if the IDs do not correspond to a live RenderViewHost.
  static RenderViewHost* FromID(int render_process_id, int render_view_id);

  // Downcasts from a RenderWidgetHost to a RenderViewHost.  Required
  // because RenderWidgetHost is a virtual base class.
  static RenderViewHost* From(RenderWidgetHost* rwh);

  virtual ~RenderViewHost() {}

  // Returns the main frame for this render view.
  virtual RenderFrameHost* GetMainFrame() = 0;

  // Tell the render view to enable a set of javascript bindings. The argument
  // should be a combination of values from BindingsPolicy.
  virtual void AllowBindings(int binding_flags) = 0;

  // Tells the renderer to clear the focused element (if any).
  virtual void ClearFocusedElement() = 0;

  // Returns true if the current focused element is editable.
  virtual bool IsFocusedElementEditable() = 0;

  // Causes the renderer to close the current page, including running its
  // onunload event handler.  A ClosePage_ACK message will be sent to the
  // ResourceDispatcherHost when it is finished.
  virtual void ClosePage() = 0;

  // Copies the image at location x, y to the clipboard (if there indeed is an
  // image at that location).
  virtual void CopyImageAt(int x, int y) = 0;

  // Saves the image at location x, y to the disk (if there indeed is an
  // image at that location).
  virtual void SaveImageAt(int x, int y) = 0;

  // Notifies the listener that a directory enumeration is complete.
  virtual void DirectoryEnumerationFinished(
      int request_id,
      const std::vector<base::FilePath>& files) = 0;

  // Tells the renderer not to add scrollbars with height and width below a
  // threshold.
  virtual void DisableScrollbarsForThreshold(const gfx::Size& size) = 0;

  // Notifies the renderer that a a drag operation that it started has ended,
  // either in a drop or by being cancelled.
  virtual void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y,
      blink::WebDragOperation operation) = 0;

  // Notifies the renderer that we're done with the drag and drop operation.
  // This allows the renderer to reset some state.
  virtual void DragSourceSystemDragEnded() = 0;

  // D&d drop target messages that get sent to WebKit.
  virtual void DragTargetDragEnter(
      const DropData& drop_data,
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) = 0;
  virtual void DragTargetDragOver(
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) = 0;
  virtual void DragTargetDragLeave() = 0;
  virtual void DragTargetDrop(const gfx::Point& client_pt,
                              const gfx::Point& screen_pt,
                              int key_modifiers) = 0;

  // Instructs the RenderView to automatically resize and send back updates
  // for the new size.
  virtual void EnableAutoResize(const gfx::Size& min_size,
                                const gfx::Size& max_size) = 0;

  // Turns off auto-resize and gives a new size that the view should be.
  virtual void DisableAutoResize(const gfx::Size& new_size) = 0;

  // Instructs the RenderView to send back updates to the preferred size.
  virtual void EnablePreferredSizeMode() = 0;

  // Tells the renderer to perform the given action on the media player
  // located at the given point.
  virtual void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const blink::WebMediaPlayerAction& action) = 0;

  // Tells the renderer to perform the given action on the plugin located at
  // the given point.
  virtual void ExecutePluginActionAtLocation(
      const gfx::Point& location, const blink::WebPluginAction& action) = 0;

  // Asks the renderer to exit fullscreen
  virtual void ExitFullscreen() = 0;

  // Notifies the Listener that one or more files have been chosen by the user
  // from a file chooser dialog for the form. |permissions| is the file
  // selection mode in which the chooser dialog was created.
  virtual void FilesSelectedInChooser(
      const std::vector<ui::SelectedFileInfo>& files,
      FileChooserParams::Mode permissions) = 0;

  virtual RenderViewHostDelegate* GetDelegate() const = 0;

  // Returns a bitwise OR of bindings types that have been enabled for this
  // RenderView. See BindingsPolicy for details.
  virtual int GetEnabledBindings() const = 0;

  virtual SiteInstance* GetSiteInstance() const = 0;

  // Returns true if the RenderView is active and has not crashed. Virtual
  // because it is overridden by TestRenderViewHost.
  virtual bool IsRenderViewLive() const = 0;

  // Notification that a move or resize renderer's containing window has
  // started.
  virtual void NotifyMoveOrResizeStarted() = 0;

  // Sets a property with the given name and value on the Web UI binding object.
  // Must call AllowWebUIBindings() on this renderer first.
  virtual void SetWebUIProperty(const std::string& name,
                                const std::string& value) = 0;

  // Changes the zoom level for the current main frame.
  virtual void Zoom(PageZoom zoom) = 0;

  // Send the renderer process the current preferences supplied by the
  // RenderViewHostDelegate.
  virtual void SyncRendererPrefs() = 0;

  // Returns the current WebKit preferences.
  virtual WebPreferences GetWebkitPreferences() = 0;

  // Passes a list of Webkit preferences to the renderer.
  virtual void UpdateWebkitPreferences(const WebPreferences& prefs) = 0;

  // Retrieves the list of AudioOutputController objects associated
  // with this object and passes it to the callback you specify, on
  // the same thread on which you called the method.
  typedef std::list<scoped_refptr<media::AudioOutputController> >
      AudioOutputControllerList;
  typedef base::Callback<void(const AudioOutputControllerList&)>
      GetAudioOutputControllersCallback;
  virtual void GetAudioOutputControllers(
      const GetAudioOutputControllersCallback& callback) const = 0;

  // Sets the mojo handle for WebUI pages.
  virtual void SetWebUIHandle(mojo::ScopedMessagePipeHandle handle) = 0;

  // Notify the render view host to select the word around the caret.
  virtual void SelectWordAroundCaret() = 0;

#if defined(OS_ANDROID)
  // Selects and zooms to the find result nearest to the point (x,y)
  // defined in find-in-page coordinates.
  virtual void ActivateNearestFindResult(int request_id, float x, float y) = 0;

  // Asks the renderer to send the rects of the current find matches.
  virtual void RequestFindMatchRects(int current_version) = 0;
#endif

 private:
  // This interface should only be implemented inside content.
  friend class RenderViewHostImpl;
  RenderViewHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
