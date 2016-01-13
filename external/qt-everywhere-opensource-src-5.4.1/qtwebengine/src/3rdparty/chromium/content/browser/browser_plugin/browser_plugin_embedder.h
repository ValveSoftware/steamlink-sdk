// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginEmbedder handles messages coming from a BrowserPlugin's
// embedder that are not directed at any particular existing guest process.
// In the beginning, when a BrowserPlugin instance in the embedder renderer
// process requests an initial navigation, the WebContents for that renderer
// renderer creates a BrowserPluginEmbedder for itself. The
// BrowserPluginEmbedder, in turn, forwards the requests to a
// BrowserPluginGuestManager, which creates and manages the lifetime of the new
// guest.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"

struct BrowserPluginHostMsg_Attach_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;

namespace gfx {
class Point;
}

namespace content {

class BrowserPluginGuest;
class BrowserPluginGuestManager;
class RenderWidgetHostImpl;
class WebContentsImpl;
struct NativeWebKeyboardEvent;

class CONTENT_EXPORT BrowserPluginEmbedder : public WebContentsObserver {
 public:
  virtual ~BrowserPluginEmbedder();

  static BrowserPluginEmbedder* Create(WebContentsImpl* web_contents);

  // Returns this embedder's WebContentsImpl.
  WebContentsImpl* GetWebContents() const;

  // Called when embedder's |rwh| has sent screen rects to renderer.
  void DidSendScreenRects();

  // WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void DragSourceEndedAt(int client_x, int client_y, int screen_x,
      int screen_y, blink::WebDragOperation operation);

  void OnUpdateDragCursor(bool* handled);

  void DragEnteredGuest(BrowserPluginGuest* guest);

  void DragLeftGuest(BrowserPluginGuest* guest);

  void StartDrag(BrowserPluginGuest* guest);

  // Sends EndSystemDrag message to the guest that initiated the last drag/drop
  // operation, if there's any.
  void SystemDragEnded();

 private:
  explicit BrowserPluginEmbedder(WebContentsImpl* web_contents);

  BrowserPluginGuestManager* GetBrowserPluginGuestManager() const;

  bool DidSendScreenRectsCallback(WebContents* guest_web_contents);

  bool SetZoomLevelCallback(double level, WebContents* guest_web_contents);

  bool UnlockMouseIfNecessaryCallback(const NativeWebKeyboardEvent& event,
                                      WebContents* guest);

  // Called by the content embedder when a guest exists with the provided
  // |instance_id|.
  void OnGuestCallback(int instance_id,
                       const BrowserPluginHostMsg_Attach_Params& params,
                       const base::DictionaryValue* extra_params,
                       WebContents* guest_web_contents);

  // Message handlers.

  void OnAttach(int instance_id,
                const BrowserPluginHostMsg_Attach_Params& params,
                const base::DictionaryValue& extra_params);
  void OnPluginAtPositionResponse(int instance_id,
                                  int request_id,
                                  const gfx::Point& position);

  // Used to correctly update the cursor when dragging over a guest, and to
  // handle a race condition when dropping onto the guest that started the drag
  // (the race is that the dragend message arrives before the drop message so
  // the drop never takes place).
  // crbug.com/233571
  base::WeakPtr<BrowserPluginGuest> guest_dragging_over_;

  // Pointer to the guest that started the drag, used to forward necessary drag
  // status messages to the correct guest.
  base::WeakPtr<BrowserPluginGuest> guest_started_drag_;

  base::WeakPtrFactory<BrowserPluginEmbedder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginEmbedder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
