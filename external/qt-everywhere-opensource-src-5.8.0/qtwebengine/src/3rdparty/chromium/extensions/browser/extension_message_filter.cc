// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_message_filter.h"

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/blob_holder.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserThread;
using content::RenderProcessHost;

namespace extensions {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ExtensionMessageFilter") {
    DependsOn(EventRouterFactory::GetInstance());
    DependsOn(ProcessManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

}  // namespace

ExtensionMessageFilter::ExtensionMessageFilter(int render_process_id,
                                               content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::Bind(&ExtensionMessageFilter::ShutdownOnUIThread,
                     base::Unretained(this)));
}

void ExtensionMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

ExtensionMessageFilter::~ExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

EventRouter* ExtensionMessageFilter::GetEventRouter() {
  DCHECK(browser_context_);
  return EventRouter::Get(browser_context_);
}

void ExtensionMessageFilter::ShutdownOnUIThread() {
  browser_context_ = nullptr;
  shutdown_notifier_.reset();
}

void ExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_AddListener::ID:
    case ExtensionHostMsg_RemoveListener::ID:
    case ExtensionHostMsg_AddLazyListener::ID:
    case ExtensionHostMsg_RemoveLazyListener::ID:
    case ExtensionHostMsg_AddFilteredListener::ID:
    case ExtensionHostMsg_RemoveFilteredListener::ID:
    case ExtensionHostMsg_ShouldSuspendAck::ID:
    case ExtensionHostMsg_SuspendAck::ID:
    case ExtensionHostMsg_TransferBlobsAck::ID:
    case ExtensionHostMsg_WakeEventPage::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ExtensionMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool ExtensionMessageFilter::OnMessageReceived(const IPC::Message& message) {
  // If we have been shut down already, return.
  if (!browser_context_)
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddListener,
                        OnExtensionAddListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveListener,
                        OnExtensionRemoveListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddLazyListener,
                        OnExtensionAddLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveLazyListener,
                        OnExtensionRemoveLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddFilteredListener,
                        OnExtensionAddFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveFilteredListener,
                        OnExtensionRemoveFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ShouldSuspendAck,
                        OnExtensionShouldSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_SuspendAck,
                        OnExtensionSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_TransferBlobsAck,
                        OnExtensionTransferBlobsAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_WakeEventPage,
                        OnExtensionWakeEventPage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageFilter::OnExtensionAddListener(
    const std::string& extension_id,
    const GURL& listener_url,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    GetEventRouter()->AddEventListener(event_name, process, extension_id);
  } else if (listener_url.is_valid()) {
    GetEventRouter()->AddEventListenerForURL(event_name, process, listener_url);
  } else {
    NOTREACHED() << "Tried to add an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionRemoveListener(
    const std::string& extension_id,
    const GURL& listener_url,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    GetEventRouter()->RemoveEventListener(event_name, process, extension_id);
  } else if (listener_url.is_valid()) {
    GetEventRouter()->RemoveEventListenerForURL(event_name, process,
                                                listener_url);
  } else {
    NOTREACHED() << "Tried to remove an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionAddLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  GetEventRouter()->AddLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionRemoveLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  GetEventRouter()->RemoveLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionAddFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  GetEventRouter()->AddFilteredEventListener(event_name, process, extension_id,
                                             filter, lazy);
}

void ExtensionMessageFilter::OnExtensionRemoveFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  GetEventRouter()->RemoveFilteredEventListener(event_name, process,
                                                extension_id, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionShouldSuspendAck(
     const std::string& extension_id, int sequence_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  ProcessManager::Get(browser_context_)
      ->OnShouldSuspendAck(extension_id, sequence_id);
}

void ExtensionMessageFilter::OnExtensionSuspendAck(
     const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  ProcessManager::Get(browser_context_)->OnSuspendAck(extension_id);
}

void ExtensionMessageFilter::OnExtensionTransferBlobsAck(
    const std::vector<std::string>& blob_uuids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  BlobHolder::FromRenderProcessHost(process)->DropBlobs(blob_uuids);
}

void ExtensionMessageFilter::OnExtensionWakeEventPage(
    int request_id,
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension) {
    // Don't kill the renderer, it might just be some context which hasn't
    // caught up to extension having been uninstalled.
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context_);

  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    // Wake the event page if it's asleep, or immediately repond with success
    // if it's already awake.
    if (process_manager->IsEventPageSuspended(extension_id)) {
      process_manager->WakeEventPage(
          extension_id,
          base::Bind(&ExtensionMessageFilter::SendWakeEventPageResponse, this,
                     request_id));
    } else {
      SendWakeEventPageResponse(request_id, true);
    }
    return;
  }

  if (BackgroundInfo::HasPersistentBackgroundPage(extension)) {
    // No point in trying to wake a persistent background page. If it's open,
    // immediately return and call it a success. If it's closed, fail.
    SendWakeEventPageResponse(request_id,
                              process_manager->GetBackgroundHostForExtension(
                                  extension_id) != nullptr);
    return;
  }

  // The extension has no background page, so there is nothing to wake.
  SendWakeEventPageResponse(request_id, false);
}

void ExtensionMessageFilter::SendWakeEventPageResponse(int request_id,
                                                       bool success) {
  Send(new ExtensionMsg_WakeEventPageResponse(request_id, success));
}

}  // namespace extensions
