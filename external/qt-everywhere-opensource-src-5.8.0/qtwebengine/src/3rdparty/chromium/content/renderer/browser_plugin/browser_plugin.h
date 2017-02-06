// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_

#include "third_party/WebKit/public/web/WebPlugin.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebWidget.h"

struct BrowserPluginHostMsg_ResizeGuest_Params;
struct FrameMsg_BuffersSwapped_Params;

namespace cc {
class SurfaceId;
struct SurfaceSequence;
}

namespace content {

class BrowserPluginDelegate;
class BrowserPluginManager;
class ChildFrameCompositingHelper;

class CONTENT_EXPORT BrowserPlugin :
    NON_EXPORTED_BASE(public blink::WebPlugin),
    public MouseLockDispatcher::LockTarget {
 public:
  static BrowserPlugin* GetFromNode(blink::WebNode& node);

  int render_frame_routing_id() const { return render_frame_routing_id_; }
  int browser_plugin_instance_id() const { return browser_plugin_instance_id_; }
  bool attached() const { return attached_; }

  bool OnMessageReceived(const IPC::Message& msg);

  // Update Browser Plugin's DOM Node attribute |attribute_name| with the value
  // |attribute_value|.
  void UpdateDOMAttribute(const std::string& attribute_name,
                          const base::string16& attribute_value);

  // Returns whether the guest process has crashed.
  bool guest_crashed() const { return guest_crashed_; }

  // Informs the guest of an updated focus state.
  void UpdateGuestFocusState(blink::WebFocusType focus_type);

  // Indicates whether the guest should be focused.
  bool ShouldGuestBeFocused() const;

  // A request to enable hardware compositing.
  void EnableCompositing(bool enable);

  // Called by CompositingHelper to send current SurfaceSequence to browser.
  void SendSatisfySequence(const cc::SurfaceSequence& sequence);

  // Provided that a guest instance ID has been allocated, this method attaches
  // this BrowserPlugin instance to that guest.
  void Attach();

  // This method detaches this BrowserPlugin instance from the guest that it's
  // currently attached to, if any.
  void Detach();

  // Notify the plugin about a compositor commit so that frame ACKs could be
  // sent, if needed.
  void DidCommitCompositorFrame();

  // Returns whether a message should be forwarded to BrowserPlugin.
  static bool ShouldForwardToBrowserPlugin(const IPC::Message& message);

  // blink::WebPlugin implementation.
  blink::WebPluginContainer* container() const override;
  bool initialize(blink::WebPluginContainer* container) override;
  void destroy() override;
  v8::Local<v8::Object> v8ScriptableObject(v8::Isolate* isolate) override;
  bool supportsKeyboardFocus() const override;
  bool supportsEditCommands() const override;
  bool supportsInputMethod() const override;
  bool canProcessDrag() const override;
  void updateAllLifecyclePhases() override {}
  void paint(blink::WebCanvas* canvas, const blink::WebRect& rect) override {}
  void updateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      const blink::WebVector<blink::WebRect>& cut_outs_rects,
                      bool is_visible) override;
  void updateFocus(bool focused, blink::WebFocusType focus_type) override;
  void updateVisibility(bool visible) override;
  blink::WebInputEventResult handleInputEvent(
      const blink::WebInputEvent& event,
      blink::WebCursorInfo& cursor_info) override;
  bool handleDragStatusUpdate(blink::WebDragStatus drag_status,
                              const blink::WebDragData& drag_data,
                              blink::WebDragOperationsMask mask,
                              const blink::WebPoint& position,
                              const blink::WebPoint& screen) override;
  void didReceiveResponse(const blink::WebURLResponse& response) override;
  void didReceiveData(const char* data, int data_length) override;
  void didFinishLoading() override;
  void didFailLoading(const blink::WebURLError& error) override;
  bool executeEditCommand(const blink::WebString& name) override;
  bool executeEditCommand(const blink::WebString& name,
                          const blink::WebString& value) override;
  bool setComposition(
      const blink::WebString& text,
      const blink::WebVector<blink::WebCompositionUnderline>& underlines,
      int selectionStart,
      int selectionEnd) override;
  bool confirmComposition(
      const blink::WebString& text,
      blink::WebWidget::ConfirmCompositionBehavior selectionBehavior) override;
  void extendSelectionAndDelete(int before, int after) override;

  // MouseLockDispatcher::LockTarget implementation.
  void OnLockMouseACK(bool succeeded) override;
  void OnMouseLockLost() override;
  bool HandleMouseLockedInputEvent(const blink::WebMouseEvent& event) override;

 private:
  friend class base::DeleteHelper<BrowserPlugin>;
  // Only the manager is allowed to create a BrowserPlugin.
  friend class BrowserPluginManager;

  // A BrowserPlugin object is a controller that represents an instance of a
  // browser plugin within the embedder renderer process. Once a BrowserPlugin
  // does an initial navigation or is attached to a newly created guest, it
  // acquires a browser_plugin_instance_id as well. The guest instance ID
  // uniquely identifies a guest WebContents that's hosted by this
  // BrowserPlugin.
  BrowserPlugin(RenderFrame* render_frame,
                const base::WeakPtr<BrowserPluginDelegate>& delegate);

  ~BrowserPlugin() override;

  gfx::Rect view_rect() const { return view_rect_; }

  void UpdateInternalInstanceId();

  // IPC message handlers.
  // Please keep in alphabetical order.
  void OnAdvanceFocus(int instance_id, bool reverse);
  void OnGuestGone(int instance_id);
  void OnSetChildFrameSurface(int instance_id,
                              const cc::SurfaceId& surface_id,
                              const gfx::Size& frame_size,
                              float scale_factor,
                              const cc::SurfaceSequence& sequence);
  void OnSetContentsOpaque(int instance_id, bool opaque);
  void OnSetCursor(int instance_id, const WebCursor& cursor);
  void OnSetMouseLock(int instance_id, bool enable);
  void OnSetTooltipText(int browser_plugin_instance_id,
                        const base::string16& tooltip_text);
  void OnShouldAcceptTouchEvents(int instance_id, bool accept);

  // This indicates whether this BrowserPlugin has been attached to a
  // WebContents and is ready to receive IPCs.
  bool attached_;
  // We cache the |render_frame_routing_id| because we need it on destruction.
  // If the RenderFrame is destroyed before the BrowserPlugin is destroyed
  // then we will attempt to access a nullptr.
  const int render_frame_routing_id_;
  blink::WebPluginContainer* container_;
  // The plugin's rect in css pixels.
  gfx::Rect view_rect_;
  bool guest_crashed_;
  bool plugin_focused_;
  // Tracks the visibility of the browser plugin regardless of the whole
  // embedder RenderView's visibility.
  bool visible_;

  WebCursor cursor_;

  bool mouse_locked_;

  // This indicates that the BrowserPlugin has a geometry.
  bool ready_;

  // Used for HW compositing.
  scoped_refptr<ChildFrameCompositingHelper> compositing_helper_;

  // URL for the embedder frame.
  int browser_plugin_instance_id_;

  std::vector<EditCommand> edit_commands_;

  // We call lifetime managing methods on |delegate_|, but we do not directly
  // own this. The delegate destroys itself.
  base::WeakPtr<BrowserPluginDelegate> delegate_;

  // Weak factory used in v8 |MakeWeak| callback, since the v8 callback might
  // get called after BrowserPlugin has been destroyed.
  base::WeakPtrFactory<BrowserPlugin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPlugin);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
