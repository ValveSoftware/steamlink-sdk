// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/manifest/manifest_manager_host.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "content/common/manifest_manager_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/manifest.h"
#include "content/public/common/result_codes.h"

namespace content {

namespace {

void KillRenderer(RenderFrameHost* render_frame_host) {
  base::ProcessHandle process_handle =
      render_frame_host->GetProcess()->GetHandle();
  if (process_handle == base::kNullProcessHandle)
    return;
  render_frame_host->GetProcess()->Shutdown(RESULT_CODE_KILLED_BAD_MESSAGE,
                                            false);
}

} // anonymous namespace

ManifestManagerHost::ManifestManagerHost(WebContents* web_contents)
  : WebContentsObserver(web_contents) {
}

ManifestManagerHost::~ManifestManagerHost() {}

ManifestManagerHost::GetCallbackMap*
ManifestManagerHost::GetCallbackMapForFrame(
    RenderFrameHost* render_frame_host) {
  auto it = pending_get_callbacks_.find(render_frame_host);
  return it != pending_get_callbacks_.end() ? it->second.get() : nullptr;
}

void ManifestManagerHost::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  GetCallbackMap* callbacks = GetCallbackMapForFrame(render_frame_host);
  if (!callbacks)
    return;

  // Call the callbacks with a failure state before deleting them. Do this in
  // a block so the iterator is destroyed before |callbacks|.
  {
    GetCallbackMap::const_iterator it(callbacks);
    for (; !it.IsAtEnd(); it.Advance())
      it.GetCurrentValue()->Run(GURL(), Manifest());
  }

  pending_get_callbacks_.erase(render_frame_host);
}

void ManifestManagerHost::GetManifest(RenderFrameHost* render_frame_host,
                                      const GetManifestCallback& callback) {
  GetCallbackMap* callbacks = GetCallbackMapForFrame(render_frame_host);
  if (!callbacks) {
    callbacks = new GetCallbackMap();
    pending_get_callbacks_[render_frame_host] = base::WrapUnique(callbacks);
  }

  int request_id = callbacks->Add(new GetManifestCallback(callback));

  render_frame_host->Send(new ManifestManagerMsg_RequestManifest(
      render_frame_host->GetRoutingID(), request_id));
}

bool ManifestManagerHost::OnMessageReceived(
    const IPC::Message& message, RenderFrameHost* render_frame_host) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(ManifestManagerHost, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ManifestManagerHostMsg_RequestManifestResponse,
                        OnRequestManifestResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ManifestManagerHost::OnRequestManifestResponse(
    RenderFrameHost* render_frame_host,
    int request_id,
    const GURL& manifest_url,
    const Manifest& insecure_manifest) {
  GetCallbackMap* callbacks = GetCallbackMapForFrame(render_frame_host);
  if (!callbacks) {
    DVLOG(1) << "Unexpected RequestManifestResponse to from renderer. "
                "Killing renderer.";
    KillRenderer(render_frame_host);
    return;
  }

  GetManifestCallback* callback = callbacks->Lookup(request_id);
  if (!callback) {
    DVLOG(1) << "Received a request_id (" << request_id << ") from renderer "
                "with no associated callback. Killing renderer.";
    KillRenderer(render_frame_host);
    return;
  }

  // When receiving a Manifest, the browser process can't trust that it is
  // coming from a known and secure source. It must be processed accordingly.
  Manifest manifest = insecure_manifest;
  manifest.name = base::NullableString16(
      manifest.name.string().substr(0, Manifest::kMaxIPCStringLength),
      manifest.name.is_null());
  manifest.short_name = base::NullableString16(
        manifest.short_name.string().substr(0, Manifest::kMaxIPCStringLength),
        manifest.short_name.is_null());
  if (!manifest.start_url.is_valid())
    manifest.start_url = GURL();
  for (auto& icon : manifest.icons) {
    if (!icon.src.is_valid())
      icon.src = GURL();
    icon.type = icon.type.substr(0, Manifest::kMaxIPCStringLength);
  }
  manifest.gcm_sender_id = base::NullableString16(
        manifest.gcm_sender_id.string().substr(
            0, Manifest::kMaxIPCStringLength),
        manifest.gcm_sender_id.is_null());
  for (auto& related_application : manifest.related_applications) {
    if (!related_application.url.is_valid())
      related_application.url = GURL();
    related_application.id =
        base::NullableString16(related_application.id.string().substr(
                                   0, Manifest::kMaxIPCStringLength),
                               related_application.id.is_null());
  }
  // theme_color and background_color are 32 bit unsigned integers with 64 bit
  // integers simply being used to encode the occurence of an error. Therefore,
  // any value outside the range of a 32 bit integer is invalid.
  if (manifest.theme_color < std::numeric_limits<int32_t>::min() ||
      manifest.theme_color > std::numeric_limits<int32_t>::max())
    manifest.theme_color = Manifest::kInvalidOrMissingColor;
  if (manifest.background_color < std::numeric_limits<int32_t>::min() ||
      manifest.background_color > std::numeric_limits<int32_t>::max())
    manifest.background_color = Manifest::kInvalidOrMissingColor;

  callback->Run(manifest_url, manifest);
  callbacks->Remove(request_id);
  if (callbacks->IsEmpty())
    pending_get_callbacks_.erase(render_frame_host);
}

} // namespace content
