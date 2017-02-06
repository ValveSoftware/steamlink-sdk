// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {
namespace {

class MimeHandlerStreamManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  MimeHandlerStreamManagerFactory();
  static MimeHandlerStreamManagerFactory* GetInstance();
  MimeHandlerStreamManager* Get(content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

MimeHandlerStreamManagerFactory::MimeHandlerStreamManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "MimeHandlerStreamManager",
          BrowserContextDependencyManager::GetInstance()) {
}

// static
MimeHandlerStreamManagerFactory*
MimeHandlerStreamManagerFactory::GetInstance() {
  return base::Singleton<MimeHandlerStreamManagerFactory>::get();
}

MimeHandlerStreamManager* MimeHandlerStreamManagerFactory::Get(
    content::BrowserContext* context) {
  return static_cast<MimeHandlerStreamManager*>(
      GetServiceForBrowserContext(context, true));
}

KeyedService* MimeHandlerStreamManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new MimeHandlerStreamManager();
}

content::BrowserContext*
MimeHandlerStreamManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

}  // namespace

// A WebContentsObserver that observes for a particular RenderFrameHost either
// navigating or closing (including by crashing). This is necessary to ensure
// that streams that aren't claimed by a MimeHandlerViewGuest are not leaked, by
// aborting the stream if any of those events occurs.
class MimeHandlerStreamManager::EmbedderObserver
    : public content::WebContentsObserver {
 public:
  EmbedderObserver(MimeHandlerStreamManager* stream_manager,
                   int render_process_id,
                   int render_frame_id,
                   const std::string& view_id);

 private:
  // WebContentsObserver overrides.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void WebContentsDestroyed() override;

  void AbortStream();

  bool IsTrackedRenderFrameHost(content::RenderFrameHost* render_frame_host);

  MimeHandlerStreamManager* const stream_manager_;
  const int render_process_id_;
  const int render_frame_id_;
  const std::string view_id_;
};

MimeHandlerStreamManager::MimeHandlerStreamManager()
    : extension_registry_observer_(this) {
}

MimeHandlerStreamManager::~MimeHandlerStreamManager() {
}

// static
MimeHandlerStreamManager* MimeHandlerStreamManager::Get(
    content::BrowserContext* context) {
  return MimeHandlerStreamManagerFactory::GetInstance()->Get(context);
}

void MimeHandlerStreamManager::AddStream(
    const std::string& view_id,
    std::unique_ptr<StreamContainer> stream,
    int render_process_id,
    int render_frame_id) {
  streams_by_extension_id_[stream->extension_id()].insert(view_id);
  auto result = streams_.insert(
      std::make_pair(view_id, make_linked_ptr(stream.release())));
  DCHECK(result.second);
  embedder_observers_[view_id] = make_linked_ptr(
      new EmbedderObserver(this, render_process_id, render_frame_id, view_id));
}

std::unique_ptr<StreamContainer> MimeHandlerStreamManager::ReleaseStream(
    const std::string& view_id) {
  auto stream = streams_.find(view_id);
  if (stream == streams_.end())
    return nullptr;

  std::unique_ptr<StreamContainer> result =
      base::WrapUnique(stream->second.release());
  streams_by_extension_id_[result->extension_id()].erase(view_id);
  streams_.erase(stream);
  embedder_observers_.erase(view_id);
  return result;
}

void MimeHandlerStreamManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  auto streams = streams_by_extension_id_.find(extension->id());
  if (streams == streams_by_extension_id_.end())
    return;

  for (const auto& view_id : streams->second) {
    streams_.erase(view_id);
    embedder_observers_.erase(view_id);
  }
  streams_by_extension_id_.erase(streams);
}

MimeHandlerStreamManager::EmbedderObserver::EmbedderObserver(
    MimeHandlerStreamManager* stream_manager,
    int render_process_id,
    int render_frame_id,
    const std::string& view_id)
    : WebContentsObserver(content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id,
                                           render_frame_id))),
      stream_manager_(stream_manager),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      view_id_(view_id) {
}

void MimeHandlerStreamManager::EmbedderObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!IsTrackedRenderFrameHost(render_frame_host))
    return;

  AbortStream();
}

void MimeHandlerStreamManager::EmbedderObserver::RenderProcessGone(
    base::TerminationStatus status) {
  AbortStream();
}
void MimeHandlerStreamManager::EmbedderObserver::
    DidStartProvisionalLoadForFrame(content::RenderFrameHost* render_frame_host,
                                    const GURL& validated_url,
                                    bool is_error_page,
                                    bool is_iframe_srcdoc) {
  if (!IsTrackedRenderFrameHost(render_frame_host))
    return;

  AbortStream();
}

void MimeHandlerStreamManager::EmbedderObserver::WebContentsDestroyed() {
  AbortStream();
}

void MimeHandlerStreamManager::EmbedderObserver::AbortStream() {
  Observe(nullptr);
  // This will cause the stream to be destroyed.
  stream_manager_->ReleaseStream(view_id_);
}

bool MimeHandlerStreamManager::EmbedderObserver::IsTrackedRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetRoutingID() == render_frame_id_ &&
         render_frame_host->GetProcess()->GetID() == render_process_id_;
}

}  // namespace extensions
