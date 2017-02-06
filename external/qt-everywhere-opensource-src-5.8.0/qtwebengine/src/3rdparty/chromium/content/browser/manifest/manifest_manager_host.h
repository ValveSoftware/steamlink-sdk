// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
#define CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class RenderFrameHost;
class WebContents;
struct Manifest;

// ManifestManagerHost is a helper class that allows callers to get the Manifest
// associated with a frame. It handles the IPC messaging with the child process.
// TODO(mlamouri): keep a cached version and a dirty bit here.
class ManifestManagerHost : public WebContentsObserver {
 public:
  explicit ManifestManagerHost(WebContents* web_contents);
  ~ManifestManagerHost() override;

  using GetManifestCallback = base::Callback<void(const Manifest&)>;
  using HasManifestCallback = base::Callback<void(bool)>;

  // Calls the given callback with the manifest associated with the
  // given RenderFrameHost. If the frame has no manifest or if getting it failed
  // the callback will have an empty manifest.
  void GetManifest(RenderFrameHost*, const GetManifestCallback&);

  // Calls the given callback with a bool indicating whether or not the document
  // associated with the given RenderFrameHost has a manifest.
  void HasManifest(RenderFrameHost*, const HasManifestCallback&);

  // WebContentsObserver
  bool OnMessageReceived(const IPC::Message&, RenderFrameHost*) override;
  void RenderFrameDeleted(RenderFrameHost*) override;

 private:
  using GetCallbackMap = IDMap<GetManifestCallback, IDMapOwnPointer>;
  using HasCallbackMap = IDMap<HasManifestCallback, IDMapOwnPointer>;
  using FrameGetCallbackMap = base::hash_map<RenderFrameHost*, GetCallbackMap*>;
  using FrameHasCallbackMap = base::hash_map<RenderFrameHost*, HasCallbackMap*>;

  void OnRequestManifestResponse(
      RenderFrameHost*, int request_id, const Manifest&);
  void OnHasManifestResponse(
      RenderFrameHost*, int request_id, bool);

  // Returns the CallbackMap associated with the given RenderFrameHost, or null.
  GetCallbackMap* GetCallbackMapForFrame(RenderFrameHost*);
  HasCallbackMap* HasCallbackMapForFrame(RenderFrameHost*);

  FrameGetCallbackMap pending_get_callbacks_;
  FrameHasCallbackMap pending_has_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ManifestManagerHost);
};

} // namespace content

#endif  // CONTENT_BROWSER_MANIFEST_MANIFEST_MANAGER_HOST_H_
