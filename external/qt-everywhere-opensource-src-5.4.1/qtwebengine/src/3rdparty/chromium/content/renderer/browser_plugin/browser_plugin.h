// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
#define  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_

#include "third_party/WebKit/public/web/WebPlugin.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/renderer/browser_plugin/browser_plugin_bindings.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "third_party/WebKit/public/web/WebWidget.h"

struct BrowserPluginHostMsg_AutoSize_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
struct BrowserPluginMsg_UpdateRect_Params;
struct FrameMsg_BuffersSwapped_Params;

namespace content {

class ChildFrameCompositingHelper;
class BrowserPluginManager;
class MockBrowserPlugin;

class CONTENT_EXPORT BrowserPlugin :
    NON_EXPORTED_BASE(public blink::WebPlugin),
    public MouseLockDispatcher::LockTarget {
 public:
  RenderViewImpl* render_view() const { return render_view_.get(); }
  int render_view_routing_id() const { return render_view_routing_id_; }
  int guest_instance_id() const { return guest_instance_id_; }
  bool attached() const { return attached_; }
  BrowserPluginManager* browser_plugin_manager() const {
    return browser_plugin_manager_.get();
  }

  bool OnMessageReceived(const IPC::Message& msg);

  // Update Browser Plugin's DOM Node attribute |attribute_name| with the value
  // |attribute_value|.
  void UpdateDOMAttribute(const std::string& attribute_name,
                          const std::string& attribute_value);
  // Remove the DOM Node attribute with the name |attribute_name|.
  void RemoveDOMAttribute(const std::string& attribute_name);
  // Get Browser Plugin's DOM Node attribute |attribute_name|'s value.
  std::string GetDOMAttributeValue(const std::string& attribute_name) const;
  // Checks if the attribute |attribute_name| exists in the DOM.
  bool HasDOMAttribute(const std::string& attribute_name) const;

  // Get the allowtransparency attribute value.
  bool GetAllowTransparencyAttribute() const;
  // Parse the allowtransparency attribute and adjust transparency of
  // BrowserPlugin accordingly.
  void ParseAllowTransparencyAttribute();
  // Get the autosize attribute value.
  bool GetAutoSizeAttribute() const;
  // Parses the autosize attribute value.
  void ParseAutoSizeAttribute();
  // Get the maxheight attribute value.
  int GetMaxHeightAttribute() const;
  // Get the maxwidth attribute value.
  int GetMaxWidthAttribute() const;
  // Get the minheight attribute value.
  int GetMinHeightAttribute() const;
  // Get the minwidth attribute value.
  int GetMinWidthAttribute() const;
  // Parse the minwidth, maxwidth, minheight, and maxheight attribute values.
  void ParseSizeContraintsChanged();

  bool InAutoSizeBounds(const gfx::Size& size) const;

  // Get the guest's DOMWindow proxy.
  NPObject* GetContentWindow() const;

  // Returns whether the guest process has crashed.
  bool guest_crashed() const { return guest_crashed_; }
  // Returns whether this BrowserPlugin has allocated an instance ID.
  bool HasGuestInstanceID() const;

  // Informs the guest of an updated focus state.
  void UpdateGuestFocusState();
  // Indicates whether the guest should be focused.
  bool ShouldGuestBeFocused() const;

  // Embedder's device scale factor changed, we need to update the guest
  // renderer.
  void UpdateDeviceScaleFactor(float device_scale_factor);

  // A request to enable hardware compositing.
  void EnableCompositing(bool enable);

  // Provided that a guest instance ID has been allocated, this method attaches
  // this BrowserPlugin instance to that guest. |extra_params| are parameters
  // passed in by the content embedder to the browser process.
  void Attach(int guest_instance_id,
              scoped_ptr<base::DictionaryValue> extra_params);

  // Notify the plugin about a compositor commit so that frame ACKs could be
  // sent, if needed.
  void DidCommitCompositorFrame();

  // Returns whether a message should be forwarded to BrowserPlugin.
  static bool ShouldForwardToBrowserPlugin(const IPC::Message& message);

  // blink::WebPlugin implementation.
  virtual blink::WebPluginContainer* container() const OVERRIDE;
  virtual bool initialize(blink::WebPluginContainer* container) OVERRIDE;
  virtual void destroy() OVERRIDE;
  virtual NPObject* scriptableObject() OVERRIDE;
  virtual struct _NPP* pluginNPP() OVERRIDE;
  virtual bool supportsKeyboardFocus() const OVERRIDE;
  virtual bool supportsEditCommands() const OVERRIDE;
  virtual bool supportsInputMethod() const OVERRIDE;
  virtual bool canProcessDrag() const OVERRIDE;
  virtual void paint(
      blink::WebCanvas* canvas,
      const blink::WebRect& rect) OVERRIDE;
  virtual void updateGeometry(
      const blink::WebRect& frame_rect,
      const blink::WebRect& clip_rect,
      const blink::WebVector<blink::WebRect>& cut_outs_rects,
      bool is_visible) OVERRIDE;
  virtual void updateFocus(bool focused) OVERRIDE;
  virtual void updateVisibility(bool visible) OVERRIDE;
  virtual bool acceptsInputEvents() OVERRIDE;
  virtual bool handleInputEvent(
      const blink::WebInputEvent& event,
      blink::WebCursorInfo& cursor_info) OVERRIDE;
  virtual bool handleDragStatusUpdate(blink::WebDragStatus drag_status,
                                      const blink::WebDragData& drag_data,
                                      blink::WebDragOperationsMask mask,
                                      const blink::WebPoint& position,
                                      const blink::WebPoint& screen) OVERRIDE;
  virtual void didReceiveResponse(
      const blink::WebURLResponse& response) OVERRIDE;
  virtual void didReceiveData(const char* data, int data_length) OVERRIDE;
  virtual void didFinishLoading() OVERRIDE;
  virtual void didFailLoading(const blink::WebURLError& error) OVERRIDE;
  virtual void didFinishLoadingFrameRequest(
      const blink::WebURL& url,
      void* notify_data) OVERRIDE;
  virtual void didFailLoadingFrameRequest(
      const blink::WebURL& url,
      void* notify_data,
      const blink::WebURLError& error) OVERRIDE;
  virtual bool executeEditCommand(const blink::WebString& name) OVERRIDE;
  virtual bool executeEditCommand(const blink::WebString& name,
                                  const blink::WebString& value) OVERRIDE;
  virtual bool setComposition(
      const blink::WebString& text,
      const blink::WebVector<blink::WebCompositionUnderline>& underlines,
      int selectionStart,
      int selectionEnd) OVERRIDE;
  virtual bool confirmComposition(
      const blink::WebString& text,
      blink::WebWidget::ConfirmCompositionBehavior selectionBehavior) OVERRIDE;
  virtual void extendSelectionAndDelete(int before, int after) OVERRIDE;

  // MouseLockDispatcher::LockTarget implementation.
  virtual void OnLockMouseACK(bool succeeded) OVERRIDE;
  virtual void OnMouseLockLost() OVERRIDE;
  virtual bool HandleMouseLockedInputEvent(
          const blink::WebMouseEvent& event) OVERRIDE;

 private:
  friend class base::DeleteHelper<BrowserPlugin>;
  // Only the manager is allowed to create a BrowserPlugin.
  friend class BrowserPluginManagerImpl;
  friend class MockBrowserPluginManager;

  // For unit/integration tests.
  friend class MockBrowserPlugin;

  // A BrowserPlugin object is a controller that represents an instance of a
  // browser plugin within the embedder renderer process. Once a BrowserPlugin
  // does an initial navigation or is attached to a newly created guest, it
  // acquires a guest_instance_id as well. The guest instance ID uniquely
  // identifies a guest WebContents that's hosted by this BrowserPlugin.
  BrowserPlugin(RenderViewImpl* render_view,
                blink::WebFrame* frame,
                bool auto_navigate);

  virtual ~BrowserPlugin();

  int width() const { return plugin_rect_.width(); }
  int height() const { return plugin_rect_.height(); }
  gfx::Size plugin_size() const { return plugin_rect_.size(); }
  gfx::Rect plugin_rect() const { return plugin_rect_; }
  // Gets the Max Height value used for auto size.
  int GetAdjustedMaxHeight() const;
  // Gets the Max Width value used for auto size.
  int GetAdjustedMaxWidth() const;
  // Gets the Min Height value used for auto size.
  int GetAdjustedMinHeight() const;
  // Gets the Min Width value used for auto size.
  int GetAdjustedMinWidth() const;

  // Virtual to allow for mocking in tests.
  virtual float GetDeviceScaleFactor() const;

  void ShowSadGraphic();

  // Triggers the event-listeners for |event_name|. Note that the function
  // frees all the values in |props|.
  void TriggerEvent(const std::string& event_name,
                    std::map<std::string, base::Value*>* props);

  // Populates BrowserPluginHostMsg_ResizeGuest_Params with resize state.
  void PopulateResizeGuestParameters(
      BrowserPluginHostMsg_ResizeGuest_Params* params,
      const gfx::Size& view_size,
      bool needs_repaint);

  // Populates BrowserPluginHostMsg_AutoSize_Params object with autosize state.
  void PopulateAutoSizeParameters(
      BrowserPluginHostMsg_AutoSize_Params* params, bool auto_size_enabled);

  // Populates both AutoSize and ResizeGuest parameters based on the current
  // autosize state.
  void GetSizeParams(
      BrowserPluginHostMsg_AutoSize_Params* auto_size_params,
      BrowserPluginHostMsg_ResizeGuest_Params* resize_guest_params,
      bool needs_repaint);

  // Informs the guest of an updated autosize state.
  void UpdateGuestAutoSizeState(bool auto_size_enabled);


  // IPC message handlers.
  // Please keep in alphabetical order.
  void OnAdvanceFocus(int instance_id, bool reverse);
  void OnAttachACK(int instance_id);
  void OnBuffersSwapped(int instance_id,
                        const FrameMsg_BuffersSwapped_Params& params);
  void OnCompositorFrameSwapped(const IPC::Message& message);
  void OnCopyFromCompositingSurface(int instance_id,
                                    int request_id,
                                    gfx::Rect source_rect,
                                    gfx::Size dest_size);
  void OnGuestContentWindowReady(int instance_id,
                                 int content_window_routing_id);
  void OnGuestGone(int instance_id);
  void OnSetCursor(int instance_id, const WebCursor& cursor);
  void OnSetMouseLock(int instance_id, bool enable);
  void OnShouldAcceptTouchEvents(int instance_id, bool accept);
  void OnUpdateRect(int instance_id,
                    const BrowserPluginMsg_UpdateRect_Params& params);

  // This is the browser-process-allocated instance ID that uniquely identifies
  // a guest WebContents.
  int guest_instance_id_;
  // This indicates whether this BrowserPlugin has been attached to a
  // WebContents.
  bool attached_;
  const base::WeakPtr<RenderViewImpl> render_view_;
  // We cache the |render_view_|'s routing ID because we need it on destruction.
  // If the |render_view_| is destroyed before the BrowserPlugin is destroyed
  // then we will attempt to access a NULL pointer.
  const int render_view_routing_id_;
  blink::WebPluginContainer* container_;
  scoped_ptr<BrowserPluginBindings> bindings_;
  bool paint_ack_received_;
  gfx::Rect plugin_rect_;
  float last_device_scale_factor_;
  // Bitmap for crashed plugin. Lazily initialized, non-owning pointer.
  SkBitmap* sad_guest_;
  bool guest_crashed_;
  bool is_auto_size_state_dirty_;
  // Maximum size constraint for autosize.
  gfx::Size max_auto_size_;
  int content_window_routing_id_;
  bool plugin_focused_;
  // Tracks the visibility of the browser plugin regardless of the whole
  // embedder RenderView's visibility.
  bool visible_;

  const bool auto_navigate_;
  std::string html_string_;

  WebCursor cursor_;

  gfx::Size last_view_size_;
  bool mouse_locked_;

  // BrowserPlugin outlives RenderViewImpl in Chrome Apps and so we need to
  // store the BrowserPlugin's BrowserPluginManager in a member variable to
  // avoid accessing the RenderViewImpl.
  const scoped_refptr<BrowserPluginManager> browser_plugin_manager_;

  // Used for HW compositing.
  scoped_refptr<ChildFrameCompositingHelper> compositing_helper_;

  // Used to identify the plugin to WebBindings.
  scoped_ptr<struct _NPP> npp_;

  // URL for the embedder frame.
  const GURL embedder_frame_url_;

  std::vector<EditCommand> edit_commands_;

  // Weak factory used in v8 |MakeWeak| callback, since the v8 callback might
  // get called after BrowserPlugin has been destroyed.
  base::WeakPtrFactory<BrowserPlugin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPlugin);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
