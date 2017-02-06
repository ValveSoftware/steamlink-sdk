// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_

#include <stddef.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/console_message_level.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"

class GURL;

namespace blink {
class WebFrame;
class WebLocalFrame;
class WebPlugin;
class WebURLRequest;
class WebURLResponse;
struct WebPluginParams;
}

namespace gfx {
class Range;
class Size;
}

namespace shell {
class InterfaceRegistry;
class InterfaceProvider;
}

namespace url {
class Origin;
}

namespace v8 {
template <typename T> class Local;
class Context;
class Isolate;
}

namespace content {
class ContextMenuClient;
class PluginInstanceThrottler;
class RenderAccessibility;
class RenderView;
struct ContextMenuParams;
struct WebPluginInfo;
struct WebPreferences;

// This interface wraps functionality, which is specific to frames, such as
// navigation. It provides communication with a corresponding RenderFrameHost
// in the browser process.
class CONTENT_EXPORT RenderFrame : public IPC::Listener,
                                   public IPC::Sender {
 public:
  // These numeric values are used in UMA logs; do not change them.
  enum PeripheralContentStatus {
    // Content is peripheral because it doesn't meet any of the below criteria.
    CONTENT_STATUS_PERIPHERAL = 0,
    // Content is essential because it's same-origin with the top-level frame.
    CONTENT_STATUS_ESSENTIAL_SAME_ORIGIN = 1,
    // Content is essential even though it's cross-origin, because it's large.
    CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_BIG = 2,
    // Content is essential because there's large content from the same origin.
    CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_WHITELISTED = 3,
    // Content is essential because it's tiny in size.
    CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_TINY = 4,
    // Content is essential because it has an unknown size.
    CONTENT_STATUS_ESSENTIAL_UNKNOWN_SIZE = 5,
    // Must be last.
    CONTENT_STATUS_NUM_ITEMS
  };

  // Returns the RenderFrame given a WebFrame.
  static RenderFrame* FromWebFrame(blink::WebFrame* web_frame);

  // Returns the RenderFrame given a routing id.
  static RenderFrame* FromRoutingID(int routing_id);

  // Returns the RenderView associated with this frame.
  virtual RenderView* GetRenderView() = 0;

  // Return the RenderAccessibility associated with this frame.
  virtual RenderAccessibility* GetRenderAccessibility() = 0;

  // Get the routing ID of the frame.
  virtual int GetRoutingID() = 0;

  // Returns the associated WebFrame.
  virtual blink::WebLocalFrame* GetWebFrame() = 0;

   // Gets WebKit related preferences associated with this frame.
  virtual WebPreferences& GetWebkitPreferences() = 0;

  // Shows a context menu with the given information. The given client will
  // be called with the result.
  //
  // The request ID will be returned by this function. This is passed to the
  // client functions for identification.
  //
  // If the client is destroyed, CancelContextMenu() should be called with the
  // request ID returned by this function.
  //
  // Note: if you end up having clients outliving the RenderFrame, we should add
  // a CancelContextMenuCallback function that takes a request id.
  virtual int ShowContextMenu(ContextMenuClient* client,
                              const ContextMenuParams& params) = 0;

  // Cancels a context menu in the event that the client is destroyed before the
  // menu is closed.
  virtual void CancelContextMenu(int request_id) = 0;

  // Create a new NPAPI/Pepper plugin depending on |info|. Returns NULL if no
  // plugin was found. |throttler| may be empty.
  virtual blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params,
      std::unique_ptr<PluginInstanceThrottler> throttler) = 0;

  // The client should handle the navigation externally.
  virtual void LoadURLExternally(const blink::WebURLRequest& request,
                                 blink::WebNavigationPolicy policy) = 0;

  // Execute a string of JavaScript in this frame's context.
  virtual void ExecuteJavaScript(const base::string16& javascript) = 0;

  // Returns true if this is the main (top-level) frame.
  virtual bool IsMainFrame() = 0;

  // Return true if this frame is hidden.
  virtual bool IsHidden() = 0;

  // Returns the InterfaceRegistry that this process uses to expose interfaces
  // to the application running in this frame.
  virtual shell::InterfaceRegistry* GetInterfaceRegistry() = 0;

  // Returns the InterfaceProvider that this process can use to bind
  // interfaces exposed to it by the application running in this frame.
  virtual shell::InterfaceProvider* GetRemoteInterfaces() = 0;

#if defined(ENABLE_PLUGINS)
  // Registers a plugin that has been marked peripheral. If the origin
  // whitelist is later updated and includes |content_origin|, then
  // |unthrottle_callback| will be called.
  virtual void RegisterPeripheralPlugin(
      const url::Origin& content_origin,
      const base::Closure& unthrottle_callback) = 0;

  // Returns the peripheral content heuristic decision.
  //
  // Power Saver is enabled for plugin content that are cross-origin and
  // heuristically determined to be not essential to the web page content.
  //
  // Plugin content is defined to be cross-origin when the plugin source's
  // origin differs from the top level frame's origin. For example:
  //  - Cross-origin:  a.com -> b.com/plugin.swf
  //  - Cross-origin:  a.com -> b.com/iframe.html -> b.com/plugin.swf
  //  - Same-origin:   a.com -> b.com/iframe-to-a.html -> a.com/plugin.swf
  //
  // |main_frame_origin| is the origin of the main frame.
  //
  // |content_origin| is the origin of the plugin content.
  //
  // |unobscured_size| are zoom and device scale independent logical pixels.
  virtual PeripheralContentStatus GetPeripheralContentStatus(
      const url::Origin& main_frame_origin,
      const url::Origin& content_origin,
      const gfx::Size& unobscured_size) const = 0;

  // Whitelists a |content_origin| so its content will never be throttled in
  // this RenderFrame. Whitelist is cleared by top level navigation.
  virtual void WhitelistContentOrigin(const url::Origin& content_origin) = 0;
#endif

  // Returns true if this frame is a FTP directory listing.
  virtual bool IsFTPDirectoryListing() = 0;

  // Attaches the browser plugin identified by |element_instance_id| to guest
  // content created by the embedder.
  virtual void AttachGuest(int element_instance_id) = 0;

  // Detaches the browser plugin identified by |element_instance_id| from guest
  // content created by the embedder.
  virtual void DetachGuest(int element_instance_id) = 0;

  // Notifies the browser of text selection changes made.
  virtual void SetSelectedText(const base::string16& selection_text,
                               size_t offset,
                               const gfx::Range& range) = 0;

  // Ensures that builtin mojo bindings modules are available in |context|.
  virtual void EnsureMojoBuiltinsAreAvailable(
      v8::Isolate* isolate,
      v8::Local<v8::Context> context) = 0;

  // Adds |message| to the DevTools console.
  virtual void AddMessageToConsole(ConsoleMessageLevel level,
                                   const std::string& message) = 0;

  // Whether or not this frame is using Lo-Fi.
  virtual bool IsUsingLoFi() const = 0;

  // Whether or not this frame is currently pasting.
  virtual bool IsPasting() const = 0;

  // Returns the current visibility of the frame.
  virtual blink::WebPageVisibilityState GetVisibilityState() const = 0;

 protected:
  ~RenderFrame() override {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameImpl;
  RenderFrame() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
