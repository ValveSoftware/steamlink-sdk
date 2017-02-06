// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace extensions {

// A container for loading up an extension inside a BrowserPlugin to handle a
// MIME type. A request for the URL of the data to load inside the container is
// made and a url is sent back in response which points to the URL which the
// container should be navigated to. There are two cases for making this URL
// request, the case where the plugin is embedded and the case where it is top
// level:
// 1) In the top level case a URL request for the data to load has already been
//    made by the renderer on behalf of the plugin. The |DidReceiveData| and
//    |DidFinishLoading| callbacks (from BrowserPluginDelegate) will be called
//    when data is received and when it has finished being received,
//    respectively.
// 2) In the embedded case, no URL request is automatically made by the
//    renderer. We make a URL request for the data inside the container using
//    a WebURLLoader. In this case, the |didReceiveData| and |didFinishLoading|
//    (from WebURLLoaderClient) when data is received and when it has finished
//    being received.
class MimeHandlerViewContainer : public guest_view::GuestViewContainer,
                                 public blink::WebURLLoaderClient {
 public:
  MimeHandlerViewContainer(content::RenderFrame* render_frame,
                           const std::string& mime_type,
                           const GURL& original_url);

  static std::vector<MimeHandlerViewContainer*> FromRenderFrame(
      content::RenderFrame* render_frame);

  // GuestViewContainer implementation.
  bool OnMessage(const IPC::Message& message) override;
  void OnReady() override;

  // BrowserPluginDelegate implementation.
  void DidFinishLoading() override;
  void DidReceiveData(const char* data, int data_length) override;
  void DidResizeElement(const gfx::Size& new_size) override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate*) override;

  // WebURLLoaderClient overrides.
  void didReceiveData(blink::WebURLLoader* loader,
                      const char* data,
                      int data_length,
                      int encoded_data_length) override;
  void didFinishLoading(blink::WebURLLoader* loader,
                        double finish_time,
                        int64_t total_encoded_data_length) override;

  // GuestViewContainer overrides.
  void OnRenderFrameDestroyed() override;

  // Post a JavaScript message to the guest.
  void PostMessage(v8::Isolate* isolate, v8::Local<v8::Value> message);

  // Post |message| to the guest.
  void PostMessageFromValue(const base::Value& message);

 protected:
  ~MimeHandlerViewContainer() override;

 private:
  // Message handlers.
  void OnCreateMimeHandlerViewGuestACK(int element_instance_id);
  void OnGuestAttached(int element_instance_id,
                       int guest_proxy_routing_id);
  void OnMimeHandlerViewGuestOnLoadCompleted(int element_instance_id);

  void CreateMimeHandlerViewGuest();

  // The MIME type of the plugin.
  const std::string mime_type_;

  // The URL of the extension to navigate to.
  std::string view_id_;

  // Whether the plugin is embedded or not.
  bool is_embedded_;

  // The original URL of the plugin.
  GURL original_url_;

  // The RenderView routing ID of the guest.
  int guest_proxy_routing_id_;

  // A URL loader to load the |original_url_| when the plugin is embedded. In
  // the embedded case, no URL request is made automatically.
  std::unique_ptr<blink::WebURLLoader> loader_;

  // The scriptable object that backs the plugin.
  v8::Global<v8::Object> scriptable_object_;

  // Pending postMessage messages that need to be sent to the guest. These are
  // queued while the guest is loading and once it is fully loaded they are
  // delivered so that messages aren't lost.
  std::vector<linked_ptr<v8::Global<v8::Value>>> pending_messages_;

  // True if the guest page has fully loaded and its JavaScript onload function
  // has been called.
  bool guest_loaded_;

  // The size of the element.
  gfx::Size element_size_;

  base::WeakPtrFactory<MimeHandlerViewContainer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
