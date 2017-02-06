// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_manager.h"

#include "base/bind.h"
#include "base/strings/nullable_string16.h"
#include "content/common/manifest_manager_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/fetchers/manifest_fetcher.h"
#include "content/renderer/manifest/manifest_parser.h"
#include "content/renderer/manifest/manifest_uma_util.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

ManifestManager::ManifestManager(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      may_have_manifest_(false),
      manifest_dirty_(true) {
}

ManifestManager::~ManifestManager() {
  if (fetcher_)
    fetcher_->Cancel();

  // Consumers in the browser process will not receive this message but they
  // will be aware of the RenderFrame dying and should act on that. Consumers
  // in the renderer process should be correctly notified.
  ResolveCallbacks(ResolveStateFailure);
}

bool ManifestManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ManifestManager, message)
    IPC_MESSAGE_HANDLER(ManifestManagerMsg_HasManifest, OnHasManifest)
    IPC_MESSAGE_HANDLER(ManifestManagerMsg_RequestManifest, OnRequestManifest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ManifestManager::OnHasManifest(int request_id) {
  GURL url(render_frame()->GetWebFrame()->document().manifestURL());

  bool has_manifest = may_have_manifest_ && !url.is_empty();
  Send(new ManifestManagerHostMsg_HasManifestResponse(
      routing_id(), request_id, has_manifest));
}

void ManifestManager::OnRequestManifest(int request_id) {
  GetManifest(base::Bind(&ManifestManager::OnRequestManifestComplete,
                         base::Unretained(this), request_id));
}

void ManifestManager::OnRequestManifestComplete(
    int request_id, const Manifest& manifest,
    const ManifestDebugInfo&) {
  // When sent via IPC, the Manifest must follow certain security rules.
  Manifest ipc_manifest = manifest;
  ipc_manifest.name = base::NullableString16(
      ipc_manifest.name.string().substr(0, Manifest::kMaxIPCStringLength),
      ipc_manifest.name.is_null());
  ipc_manifest.short_name = base::NullableString16(
        ipc_manifest.short_name.string().substr(0,
                                                Manifest::kMaxIPCStringLength),
        ipc_manifest.short_name.is_null());
  for (auto& icon : ipc_manifest.icons) {
    icon.type = base::NullableString16(
        icon.type.string().substr(0, Manifest::kMaxIPCStringLength),
        icon.type.is_null());
  }
  ipc_manifest.gcm_sender_id = base::NullableString16(
        ipc_manifest.gcm_sender_id.string().substr(
            0, Manifest::kMaxIPCStringLength),
        ipc_manifest.gcm_sender_id.is_null());
  for (auto& related_application : ipc_manifest.related_applications) {
    related_application.id =
        base::NullableString16(related_application.id.string().substr(
                                   0, Manifest::kMaxIPCStringLength),
                               related_application.id.is_null());
  }

  Send(new ManifestManagerHostMsg_RequestManifestResponse(
      routing_id(), request_id, ipc_manifest));
}

void ManifestManager::GetManifest(const GetManifestCallback& callback) {
  if (!may_have_manifest_) {
    callback.Run(Manifest(), ManifestDebugInfo());
    return;
  }

  if (!manifest_dirty_) {
    callback.Run(manifest_, manifest_debug_info_);
    return;
  }

  pending_callbacks_.push_back(callback);

  // Just wait for the running call to be done if there are other callbacks.
  if (pending_callbacks_.size() > 1)
    return;

  FetchManifest();
}

void ManifestManager::DidChangeManifest() {
  may_have_manifest_ = true;
  manifest_dirty_ = true;
}

void ManifestManager::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  if (is_same_page_navigation)
    return;

  may_have_manifest_ = false;
  manifest_dirty_ = true;
}

void ManifestManager::FetchManifest() {
  GURL url(render_frame()->GetWebFrame()->document().manifestURL());

  if (url.is_empty()) {
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_EMPTY_URL);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  fetcher_.reset(new ManifestFetcher(url));
  fetcher_->Start(
      render_frame()->GetWebFrame(),
      render_frame()->GetWebFrame()->document().manifestUseCredentials(),
      base::Bind(&ManifestManager::OnManifestFetchComplete,
                 base::Unretained(this),
                 render_frame()->GetWebFrame()->document().url()));
}

static const std::string& GetMessagePrefix() {
  CR_DEFINE_STATIC_LOCAL(std::string, message_prefix, ("Manifest: "));
  return message_prefix;
}

void ManifestManager::OnManifestFetchComplete(
    const GURL& document_url,
    const blink::WebURLResponse& response,
    const std::string& data) {
  if (response.isNull() && data.empty()) {
    ManifestUmaUtil::FetchFailed(ManifestUmaUtil::FETCH_UNSPECIFIED_REASON);
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  ManifestUmaUtil::FetchSucceeded();
  ManifestParser parser(data, response.url(), document_url);
  parser.Parse();

  fetcher_.reset();
  manifest_debug_info_.raw_data = data;
  parser.TakeErrors(&manifest_debug_info_.errors);

  for (const auto& error : manifest_debug_info_.errors) {
    blink::WebConsoleMessage message;
    message.level = error.critical ? blink::WebConsoleMessage::LevelError :
        blink::WebConsoleMessage::LevelWarning;
    message.text =
        blink::WebString::fromUTF8(GetMessagePrefix() + error.message);
    message.url =
        render_frame()->GetWebFrame()->document().manifestURL().string();
    message.lineNumber = error.line;
    message.columnNumber = error.column;
    render_frame()->GetWebFrame()->addMessageToConsole(message);
  }

  // Having errors while parsing the manifest doesn't mean the manifest parsing
  // failed. Some properties might have been ignored but some others kept.
  if (parser.failed()) {
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  manifest_ = parser.manifest();
  ResolveCallbacks(ResolveStateSuccess);
}

void ManifestManager::ResolveCallbacks(ResolveState state) {
  if (state == ResolveStateFailure)
    manifest_ = Manifest();

  manifest_dirty_ = state != ResolveStateSuccess;

  std::list<GetManifestCallback> callbacks;
  callbacks.swap(pending_callbacks_);

  for (std::list<GetManifestCallback>::const_iterator it = callbacks.begin();
       it != callbacks.end(); ++it) {
    it->Run(manifest_, manifest_debug_info_);
  }
}

void ManifestManager::OnDestruct() {
  delete this;
}

} // namespace content
