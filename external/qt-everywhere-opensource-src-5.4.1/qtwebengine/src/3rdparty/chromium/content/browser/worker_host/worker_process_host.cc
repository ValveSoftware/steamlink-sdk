// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_process_host.h"

#include <set>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/devtools/worker_devtools_message_filter.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/mime_registry_message_filter.h"
#include "content/browser/quota_dispatcher_host.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"
#include "content/browser/renderer_host/websocket_dispatcher_host.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_switches.h"
#include "net/base/mime_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend.h"
#include "webkit/common/resource_type.h"

#if defined(OS_WIN)
#include "content/common/sandbox_win.h"
#endif

namespace content {
namespace {

// NOTE: changes to this class need to be reviewed by the security team.
class WorkerSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  WorkerSandboxedProcessLauncherDelegate(ChildProcessHost* host,
                                         bool debugging_child)
#if defined(OS_POSIX)
      : ipc_fd_(host->TakeClientFileDescriptor()),
        debugging_child_(debugging_child)
#endif  // OS_POSIX
  {}

  virtual ~WorkerSandboxedProcessLauncherDelegate() {}

#if defined(OS_WIN)
  virtual void PreSpawnTarget(sandbox::TargetPolicy* policy,
                              bool* success) {
    AddBaseHandleClosePolicy(policy);
  }
#elif defined(OS_POSIX)
  virtual bool ShouldUseZygote() OVERRIDE {
    return !debugging_child_;
  }
  virtual int GetIpcFd() OVERRIDE {
    return ipc_fd_;
  }
#endif  // OS_WIN

 private:
#if defined(OS_POSIX)
  int ipc_fd_;
  bool debugging_child_;
#endif  // OS_POSIX
};

// Notifies RenderViewHost that one or more worker objects crashed.
void WorkerCrashCallback(int render_process_unique_id, int render_frame_id) {
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_unique_id, render_frame_id);
  if (host)
    host->delegate()->WorkerCrashed(host);
}

void WorkerCreatedCallback(int render_process_id,
                           int render_frame_id,
                           int worker_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  GetContentClient()->browser()->WorkerProcessCreated(site_instance,
                                                      worker_process_id);
}

void WorkerTerminatedCallback(int render_process_id,
                              int render_frame_id,
                              int worker_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  GetContentClient()->browser()->WorkerProcessTerminated(site_instance,
                                                         worker_process_id);
}

}  // namespace

WorkerProcessHost::WorkerProcessHost(
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition)
    : resource_context_(resource_context),
      partition_(partition),
      process_launched_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(resource_context_);
  process_.reset(
      new BrowserChildProcessHostImpl(PROCESS_TYPE_WORKER, this));
}

WorkerProcessHost::~WorkerProcessHost() {
  // If we crashed, tell the RenderViewHosts.
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (!i->load_failed()) {
      const WorkerDocumentSet::DocumentInfoSet& parents =
          i->worker_document_set()->documents();
      for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
               parents.begin(); parent_iter != parents.end(); ++parent_iter) {
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&WorkerCrashCallback, parent_iter->render_process_id(),
                       parent_iter->render_frame_id()));
      }
    }
    WorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(
        this, i->worker_route_id());
  }

  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(
      process_->GetData().id);
}

bool WorkerProcessHost::Send(IPC::Message* message) {
  return process_->Send(message);
}

bool WorkerProcessHost::Init(int render_process_id, int render_frame_id) {
  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

#if defined(OS_LINUX)
  int flags = ChildProcessHost::CHILD_ALLOW_SELF;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kWorkerProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  std::string locale = GetContentClient()->browser()->GetApplicationLocale();
  cmd_line->AppendSwitchASCII(switches::kLang, locale);

  static const char* const kSwitchNames[] = {
    switches::kDisableApplicationCache,
    switches::kDisableDatabases,
#if defined(OS_WIN)
    switches::kDisableDesktopNotifications,
#endif
    switches::kDisableFileSystem,
    switches::kDisableSeccompFilterSandbox,
    switches::kEnableExperimentalWebPlatformFeatures,
    switches::kEnablePreciseMemoryInfo,
    switches::kEnableServiceWorker,
#if defined(OS_MACOSX)
    switches::kEnableSandboxLogging,
#endif
    switches::kJavaScriptFlags,
    switches::kNoSandbox
  };
  cmd_line->CopySwitchesFrom(*CommandLine::ForCurrentProcess(), kSwitchNames,
                             arraysize(kSwitchNames));

bool debugging_child = false;
#if defined(OS_POSIX)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kWaitForDebuggerChildren);
    if (value.empty() || value == switches::kWorkerProcess) {
      cmd_line->AppendSwitch(switches::kWaitForDebugger);
      debugging_child = true;
    }
  }
#endif

  process_->Launch(
      new WorkerSandboxedProcessLauncherDelegate(process_->GetHost(),
                                                 debugging_child),
      cmd_line);

  ChildProcessSecurityPolicyImpl::GetInstance()->AddWorker(
      process_->GetData().id, render_process_id);
  CreateMessageFilters(render_process_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WorkerCreatedCallback,
                 render_process_id,
                 render_frame_id,
                 process_->GetData().id));
  return true;
}

void WorkerProcessHost::CreateMessageFilters(int render_process_id) {
  ChromeBlobStorageContext* blob_storage_context =
      GetChromeBlobStorageContextForResourceContext(resource_context_);
  StreamContext* stream_context =
      GetStreamContextForResourceContext(resource_context_);

  net::URLRequestContextGetter* url_request_context =
      partition_.url_request_context();

  ResourceMessageFilter::GetContextsCallback get_contexts_callback(
      base::Bind(&WorkerProcessHost::GetContexts,
      base::Unretained(this)));

  ResourceMessageFilter* resource_message_filter = new ResourceMessageFilter(
      process_->GetData().id, PROCESS_TYPE_WORKER,
      partition_.appcache_service(),
      blob_storage_context,
      partition_.filesystem_context(),
      partition_.service_worker_context(),
      get_contexts_callback);
  process_->AddFilter(resource_message_filter);

  MessagePortMessageFilter* message_port_message_filter =
      new MessagePortMessageFilter(
          base::Bind(&WorkerServiceImpl::next_worker_route_id,
                     base::Unretained(WorkerServiceImpl::GetInstance())));
  process_->AddFilter(message_port_message_filter);
  worker_message_filter_ = new WorkerMessageFilter(render_process_id,
                                                   resource_context_,
                                                   partition_,
                                                   message_port_message_filter);
  process_->AddFilter(worker_message_filter_.get());
  process_->AddFilter(new AppCacheDispatcherHost(
      partition_.appcache_service(), process_->GetData().id));
  process_->AddFilter(new FileAPIMessageFilter(
      process_->GetData().id,
      url_request_context,
      partition_.filesystem_context(),
      blob_storage_context,
      stream_context));
  process_->AddFilter(new FileUtilitiesMessageFilter(
      process_->GetData().id));
  process_->AddFilter(new MimeRegistryMessageFilter());
  process_->AddFilter(new DatabaseMessageFilter(partition_.database_tracker()));
  process_->AddFilter(new QuotaDispatcherHost(
      process_->GetData().id,
      partition_.quota_manager(),
      GetContentClient()->browser()->CreateQuotaPermissionContext()));

  SocketStreamDispatcherHost::GetRequestContextCallback
      request_context_callback(
          base::Bind(&WorkerProcessHost::GetRequestContext,
          base::Unretained(this)));

  SocketStreamDispatcherHost* socket_stream_dispatcher_host =
      new SocketStreamDispatcherHost(
          render_process_id,
          request_context_callback,
          resource_context_);
  socket_stream_dispatcher_host_ = socket_stream_dispatcher_host;
  process_->AddFilter(socket_stream_dispatcher_host);

  WebSocketDispatcherHost::GetRequestContextCallback
      websocket_request_context_callback(
          base::Bind(&WorkerProcessHost::GetRequestContext,
                     base::Unretained(this),
                     ResourceType::SUB_RESOURCE));

  process_->AddFilter(new WebSocketDispatcherHost(
      render_process_id, websocket_request_context_callback));

  process_->AddFilter(new WorkerDevToolsMessageFilter(process_->GetData().id));
  process_->AddFilter(
      new IndexedDBDispatcherHost(process_->GetData().id,
                                  url_request_context,
                                  partition_.indexed_db_context(),
                                  blob_storage_context));
}

void WorkerProcessHost::CreateWorker(const WorkerInstance& instance,
                                     bool pause_on_start) {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestURL(
      process_->GetData().id, instance.url());

  instances_.push_back(instance);

  WorkerProcessMsg_CreateWorker_Params params;
  params.url = instance.url();
  params.name = instance.name();
  params.content_security_policy = instance.content_security_policy();
  params.security_policy_type = instance.security_policy_type();
  params.pause_on_start = pause_on_start;
  params.route_id = instance.worker_route_id();
  Send(new WorkerProcessMsg_CreateWorker(params));

  UpdateTitle();

  // Walk all pending filters and let them know the worker has been created
  // (could be more than one in the case where we had to queue up worker
  // creation because the worker process limit was reached).
  for (WorkerInstance::FilterList::const_iterator i =
           instance.filters().begin();
       i != instance.filters().end(); ++i) {
    i->filter()->Send(new ViewMsg_WorkerCreated(i->route_id()));
  }
}

bool WorkerProcessHost::FilterMessage(const IPC::Message& message,
                                      WorkerMessageFilter* filter) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (!i->closed() && i->HasFilter(filter, message.routing_id())) {
      RelayMessage(message, filter, &(*i));
      return true;
    }
  }

  return false;
}

void WorkerProcessHost::OnProcessLaunched() {
  process_launched_ = true;

  WorkerServiceImpl::GetInstance()->NotifyWorkerProcessCreated();
}

bool WorkerProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerProcessHost, message)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerContextClosed,
                        OnWorkerContextClosed)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerContextDestroyed,
                        OnWorkerContextDestroyed)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerScriptLoaded,
                        OnWorkerScriptLoaded)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerScriptLoadFailed,
                        OnWorkerScriptLoadFailed)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerConnected,
                        OnWorkerConnected)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_RequestFileSystemAccessSync,
                        OnRequestFileSystemAccessSync)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowIndexedDB, OnAllowIndexedDB)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_ForceKillWorker,
                        OnForceKillWorkerProcess)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// Sent to notify the browser process when a worker context invokes close(), so
// no new connections are sent to shared workers.
void WorkerProcessHost::OnWorkerContextClosed(int worker_route_id) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() == worker_route_id) {
      // Set the closed flag - this will stop any further messages from
      // being sent to the worker (messages can still be sent from the worker,
      // for exception reporting, etc).
      i->set_closed(true);
      break;
    }
  }
}

void WorkerProcessHost::OnWorkerContextDestroyed(int worker_route_id) {
  WorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(
      this, worker_route_id);
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() == worker_route_id) {
      instances_.erase(i);
      UpdateTitle();
      return;
    }
  }
}

void WorkerProcessHost::OnWorkerScriptLoaded(int worker_route_id) {
  WorkerDevToolsManager::GetInstance()->WorkerContextStarted(this,
                                                             worker_route_id);
}

void WorkerProcessHost::OnWorkerScriptLoadFailed(int worker_route_id) {
  bool shutdown = true;
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() != worker_route_id) {
      shutdown = false;
      continue;
    }
    i->set_load_failed(true);
    for (WorkerInstance::FilterList::const_iterator j = i->filters().begin();
          j != i->filters().end(); ++j) {
      j->filter()->Send(new ViewMsg_WorkerScriptLoadFailed(j->route_id()));
    }
  }
  if (shutdown) {
    base::KillProcess(
          process_->GetData().handle, RESULT_CODE_NORMAL_EXIT, false);
  }
}

void WorkerProcessHost::OnWorkerConnected(int message_port_id,
                                          int worker_route_id) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() != worker_route_id)
      continue;
    for (WorkerInstance::FilterList::const_iterator j = i->filters().begin();
          j != i->filters().end(); ++j) {
      if (j->message_port_id() != message_port_id)
        continue;
      j->filter()->Send(new ViewMsg_WorkerConnected(j->route_id()));
      return;
    }
  }
}

void WorkerProcessHost::OnAllowDatabase(int worker_route_id,
                                        const GURL& url,
                                        const base::string16& name,
                                        const base::string16& display_name,
                                        unsigned long estimated_size,
                                        bool* result) {
  *result = GetContentClient()->browser()->AllowWorkerDatabase(
      url, name, display_name, estimated_size, resource_context_,
      GetRenderFrameIDsForWorker(worker_route_id));
}

void WorkerProcessHost::OnRequestFileSystemAccessSync(int worker_route_id,
                                                      const GURL& url,
                                                      bool* result) {
  *result = GetContentClient()->browser()->AllowWorkerFileSystem(
      url, resource_context_, GetRenderFrameIDsForWorker(worker_route_id));
}

void WorkerProcessHost::OnAllowIndexedDB(int worker_route_id,
                                         const GURL& url,
                                         const base::string16& name,
                                         bool* result) {
  *result = GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, name, resource_context_,
      GetRenderFrameIDsForWorker(worker_route_id));
}

void WorkerProcessHost::OnForceKillWorkerProcess() {
  if (process_ && process_launched_)
    base::KillProcess(
          process_->GetData().handle, RESULT_CODE_NORMAL_EXIT, false);
  else
    RecordAction(base::UserMetricsAction("WorkerProcess_BadProcessToKill"));
}

void WorkerProcessHost::RelayMessage(
    const IPC::Message& message,
    WorkerMessageFilter* incoming_filter,
    WorkerInstance* instance) {
  if (message.type() == WorkerMsg_Connect::ID) {
    // Crack the SharedWorker Connect message to setup routing for the port.
    WorkerMsg_Connect::Param params;
    if (!WorkerMsg_Connect::Read(&message, &params))
      return;

    int sent_message_port_id = params.a;
    int new_routing_id = params.b;
    new_routing_id = worker_message_filter_->GetNextRoutingID();
    MessagePortService::GetInstance()->UpdateMessagePort(
        sent_message_port_id,
        worker_message_filter_->message_port_message_filter(),
        new_routing_id);

    instance->SetMessagePortID(incoming_filter,
                               message.routing_id(),
                               sent_message_port_id);
    // Resend the message with the new routing id.
    worker_message_filter_->Send(new WorkerMsg_Connect(
        instance->worker_route_id(), sent_message_port_id, new_routing_id));

    // Send any queued messages for the sent port.
    MessagePortService::GetInstance()->SendQueuedMessagesIfPossible(
        sent_message_port_id);
  } else {
    IPC::Message* new_message = new IPC::Message(message);
    new_message->set_routing_id(instance->worker_route_id());
    worker_message_filter_->Send(new_message);
    return;
  }
}

void WorkerProcessHost::ShutdownSocketStreamDispatcherHostIfNecessary() {
  if (!instances_.size() && socket_stream_dispatcher_host_.get()) {
    // We can assume that this object is going to delete, because
    // currently a WorkerInstance will never be added to a WorkerProcessHost
    // once it is initialized.

    // SocketStreamDispatcherHost should be notified now that the worker
    // process will shutdown soon.
    socket_stream_dispatcher_host_->Shutdown();
    socket_stream_dispatcher_host_ = NULL;
  }
}

void WorkerProcessHost::FilterShutdown(WorkerMessageFilter* filter) {
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    bool shutdown = false;
    i->RemoveFilters(filter);

    int render_frame_id = 0;
    const WorkerDocumentSet::DocumentInfoSet& documents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
         documents.begin(); doc != documents.end(); ++doc) {
      if (doc->filter() == filter) {
        render_frame_id = doc->render_frame_id();
        break;
      }
    }
    i->worker_document_set()->RemoveAll(filter);
    if (i->worker_document_set()->IsEmpty()) {
      shutdown = true;
    }
    if (shutdown) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&WorkerTerminatedCallback,
                     filter->render_process_id(),
                     render_frame_id,
                     process_->GetData().id));
      Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id()));
      i = instances_.erase(i);
    } else {
      ++i;
    }
  }
  ShutdownSocketStreamDispatcherHostIfNecessary();
}

bool WorkerProcessHost::CanShutdown() {
  return instances_.empty();
}

void WorkerProcessHost::UpdateTitle() {
  std::set<std::string> titles;
  for (Instances::iterator i = instances_.begin(); i != instances_.end(); ++i) {
    // Allow the embedder first crack at special casing the title.
    std::string title = GetContentClient()->browser()->
        GetWorkerProcessTitle(i->url(), resource_context_);

    if (title.empty()) {
      title = net::registry_controlled_domains::GetDomainAndRegistry(
          i->url(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    }

    // Use the host name if the domain is empty, i.e. localhost or IP address.
    if (title.empty())
      title = i->url().host();

    // If the host name is empty, i.e. file url, use the path.
    if (title.empty())
      title = i->url().path();
    titles.insert(title);
  }

  std::string display_title;
  for (std::set<std::string>::iterator i = titles.begin();
       i != titles.end(); ++i) {
    if (!display_title.empty())
      display_title += ", ";
    display_title += *i;
  }

  process_->SetName(base::UTF8ToUTF16(display_title));
}

void WorkerProcessHost::DocumentDetached(WorkerMessageFilter* filter,
                                         unsigned long long document_id) {
  // Walk all instances and remove the document from their document set.
  for (Instances::iterator i = instances_.begin(); i != instances_.end();) {
    int render_frame_id = 0;
    const WorkerDocumentSet::DocumentInfoSet& documents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
         documents.begin(); doc != documents.end(); ++doc) {
      if (doc->filter() == filter && doc->document_id() == document_id) {
        render_frame_id = doc->render_frame_id();
        break;
      }
    }
    i->worker_document_set()->Remove(filter, document_id);
    if (i->worker_document_set()->IsEmpty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&WorkerTerminatedCallback,
                     filter->render_process_id(),
                     render_frame_id,
                     process_->GetData().id));
      // This worker has no more associated documents - shut it down.
      Send(new WorkerMsg_TerminateWorkerContext(i->worker_route_id()));
      i = instances_.erase(i);
    } else {
      ++i;
    }
  }
  ShutdownSocketStreamDispatcherHostIfNecessary();
}

void WorkerProcessHost::TerminateWorker(int worker_route_id) {
  Send(new WorkerMsg_TerminateWorkerContext(worker_route_id));
}

void WorkerProcessHost::SetBackgrounded(bool backgrounded) {
  process_->SetBackgrounded(backgrounded);
}

const ChildProcessData& WorkerProcessHost::GetData() {
  return process_->GetData();
}

std::vector<std::pair<int, int> > WorkerProcessHost::GetRenderFrameIDsForWorker(
    int worker_route_id) {
  std::vector<std::pair<int, int> > result;
  WorkerProcessHost::Instances::const_iterator i;
  for (i = instances_.begin(); i != instances_.end(); ++i) {
    if (i->worker_route_id() != worker_route_id)
      continue;
    const WorkerDocumentSet::DocumentInfoSet& documents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
         documents.begin(); doc != documents.end(); ++doc) {
      result.push_back(
          std::make_pair(doc->render_process_id(), doc->render_frame_id()));
    }
    break;
  }
  return result;
}

void WorkerProcessHost::GetContexts(const ResourceHostMsg_Request& request,
                                    ResourceContext** resource_context,
                                    net::URLRequestContext** request_context) {
  *resource_context = resource_context_;
  *request_context = partition_.url_request_context()->GetURLRequestContext();
}

net::URLRequestContext* WorkerProcessHost::GetRequestContext(
    ResourceType::Type resource_type) {
  return partition_.url_request_context()->GetURLRequestContext();
}

WorkerProcessHost::WorkerInstance::WorkerInstance(
    const GURL& url,
    const base::string16& name,
    const base::string16& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    int worker_route_id,
    int render_frame_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition)
    : url_(url),
      closed_(false),
      name_(name),
      content_security_policy_(content_security_policy),
      security_policy_type_(security_policy_type),
      worker_route_id_(worker_route_id),
      render_frame_id_(render_frame_id),
      worker_document_set_(new WorkerDocumentSet()),
      resource_context_(resource_context),
      partition_(partition),
      load_failed_(false) {
  DCHECK(resource_context_);
}

WorkerProcessHost::WorkerInstance::~WorkerInstance() {
}

void WorkerProcessHost::WorkerInstance::SetMessagePortID(
    WorkerMessageFilter* filter,
    int route_id,
    int message_port_id) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end(); ++i) {
    if (i->filter() == filter && i->route_id() == route_id) {
      i->set_message_port_id(message_port_id);
      return;
    }
  }
}

// Compares an instance based on the algorithm in the WebWorkers spec - an
// instance matches if the origins of the URLs match, and:
// a) the names are non-empty and equal
// -or-
// b) the names are both empty, and the urls are equal
bool WorkerProcessHost::WorkerInstance::Matches(
    const GURL& match_url,
    const base::string16& match_name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) const {
  // Only match open shared workers.
  if (closed_)
    return false;

  // ResourceContext equivalence is being used as a proxy to ensure we only
  // matched shared workers within the same BrowserContext.
  if (resource_context_ != resource_context)
    return false;

  // We must be in the same storage partition otherwise sharing will violate
  // isolation.
  if (!partition_.Equals(partition))
    return false;

  if (url_.GetOrigin() != match_url.GetOrigin())
    return false;

  if (name_.empty() && match_name.empty())
    return url_ == match_url;

  return name_ == match_name;
}

void WorkerProcessHost::WorkerInstance::AddFilter(WorkerMessageFilter* filter,
                                                  int route_id) {
  CHECK(filter);
  if (!HasFilter(filter, route_id)) {
    FilterInfo info(filter, route_id);
    filters_.push_back(info);
  }
}

void WorkerProcessHost::WorkerInstance::RemoveFilter(
    WorkerMessageFilter* filter, int route_id) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end();) {
    if (i->filter() == filter && i->route_id() == route_id)
      i = filters_.erase(i);
    else
      ++i;
  }
  // Should not be duplicate copies in the filter set.
  DCHECK(!HasFilter(filter, route_id));
}

void WorkerProcessHost::WorkerInstance::RemoveFilters(
    WorkerMessageFilter* filter) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end();) {
    if (i->filter() == filter)
      i = filters_.erase(i);
    else
      ++i;
  }
}

bool WorkerProcessHost::WorkerInstance::HasFilter(
    WorkerMessageFilter* filter, int route_id) const {
  for (FilterList::const_iterator i = filters_.begin(); i != filters_.end();
       ++i) {
    if (i->filter() == filter && i->route_id() == route_id)
      return true;
  }
  return false;
}

bool WorkerProcessHost::WorkerInstance::FrameIsParent(
    int render_process_id, int render_frame_id) const {
  const WorkerDocumentSet::DocumentInfoSet& parents =
      worker_document_set()->documents();
  for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
           parents.begin();
       parent_iter != parents.end(); ++parent_iter) {
    if (parent_iter->render_process_id() == render_process_id &&
        parent_iter->render_frame_id() == render_frame_id) {
      return true;
    }
  }
  return false;
}

WorkerProcessHost::WorkerInstance::FilterInfo
WorkerProcessHost::WorkerInstance::GetFilter() const {
  DCHECK(NumFilters() == 1);
  return *filters_.begin();
}

}  // namespace content
