// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router.h"

#include <stddef.h>

#include <tuple>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"

using base::DictionaryValue;
using base::ListValue;
using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

void DoNothing(ExtensionHost* host) {}

// A dictionary of event names to lists of filters that this extension has
// registered from its lazy background page.
const char kFilteredEvents[] = "filtered_events";

// Sends a notification about an event to the API activity monitor and the
// ExtensionHost for |extension_id| on the UI thread. Can be called from any
// thread.
void NotifyEventDispatched(void* browser_context_id,
                           const std::string& extension_id,
                           const std::string& event_name,
                           const base::ListValue& args) {
  // Notify the ApiActivityMonitor about the event dispatch.
  BrowserContext* context = static_cast<BrowserContext*>(browser_context_id);
  activity_monitor::OnApiEventDispatched(context, extension_id, event_name,
                                         args);
}

// A global identifier used to distinguish extension events.
base::StaticAtomicSequenceNumber g_extension_event_id;

}  // namespace

const char EventRouter::kRegisteredEvents[] = "events";

struct EventRouter::ListenerProcess {
  content::RenderProcessHost* process;
  std::string extension_id;

  ListenerProcess(content::RenderProcessHost* process,
                  const std::string& extension_id)
      : process(process), extension_id(extension_id) {}

  bool operator<(const ListenerProcess& that) const {
    return std::tie(process, extension_id) <
           std::tie(that.process, that.extension_id);
  }
};

// static
void EventRouter::DispatchExtensionMessage(IPC::Sender* ipc_sender,
                                           void* browser_context_id,
                                           const std::string& extension_id,
                                           int event_id,
                                           const std::string& event_name,
                                           ListValue* event_args,
                                           UserGestureState user_gesture,
                                           const EventFilteringInfo& info) {
  NotifyEventDispatched(browser_context_id, extension_id, event_name,
                        *event_args);

  // TODO(chirantan): Make event dispatch a separate IPC so that it doesn't
  // piggyback off MessageInvoke, which is used for other things.
  ListValue args;
  args.Set(0, new base::StringValue(event_name));
  args.Set(1, event_args);
  args.Set(2, info.AsValue().release());
  args.Set(3, new base::FundamentalValue(event_id));
  ipc_sender->Send(new ExtensionMsg_MessageInvoke(
      MSG_ROUTING_CONTROL,
      extension_id,
      kEventBindings,
      "dispatchEvent",
      args,
      user_gesture == USER_GESTURE_ENABLED));

  // DispatchExtensionMessage does _not_ take ownership of event_args, so we
  // must ensure that the destruction of args does not attempt to free it.
  std::unique_ptr<base::Value> removed_event_args;
  args.Remove(1, &removed_event_args);
  ignore_result(removed_event_args.release());
}

// static
EventRouter* EventRouter::Get(content::BrowserContext* browser_context) {
  return EventRouterFactory::GetForBrowserContext(browser_context);
}

// static
std::string EventRouter::GetBaseEventName(const std::string& full_event_name) {
  size_t slash_sep = full_event_name.find('/');
  return full_event_name.substr(0, slash_sep);
}

// static
void EventRouter::DispatchEventToSender(IPC::Sender* ipc_sender,
                                        void* browser_context_id,
                                        const std::string& extension_id,
                                        events::HistogramValue histogram_value,
                                        const std::string& event_name,
                                        std::unique_ptr<ListValue> event_args,
                                        UserGestureState user_gesture,
                                        const EventFilteringInfo& info) {
  int event_id = g_extension_event_id.GetNext();

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DoDispatchEventToSenderBookkeepingOnUI(browser_context_id, extension_id,
                                           event_id, histogram_value,
                                           event_name);
  } else {
    // This is called from WebRequest API.
    // TODO(lazyboy): Skip this entirely: http://crbug.com/488747.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&EventRouter::DoDispatchEventToSenderBookkeepingOnUI,
                   browser_context_id, extension_id, event_id, histogram_value,
                   event_name));
  }

  DispatchExtensionMessage(ipc_sender, browser_context_id, extension_id,
                           event_id, event_name, event_args.get(), user_gesture,
                           info);
}

EventRouter::EventRouter(BrowserContext* browser_context,
                         ExtensionPrefs* extension_prefs)
    : browser_context_(browser_context),
      extension_prefs_(extension_prefs),
      extension_registry_observer_(this),
      listeners_(this) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_ENABLED,
                 content::Source<BrowserContext>(browser_context_));
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

EventRouter::~EventRouter() {
  for (auto* process : observed_process_set_)
    process->RemoveObserver(this);
}

void EventRouter::AddEventListener(const std::string& event_name,
                                   content::RenderProcessHost* process,
                                   const std::string& extension_id) {
  listeners_.AddListener(EventListener::ForExtension(
      event_name, extension_id, process, std::unique_ptr<DictionaryValue>()));
}

void EventRouter::RemoveEventListener(const std::string& event_name,
                                      content::RenderProcessHost* process,
                                      const std::string& extension_id) {
  std::unique_ptr<EventListener> listener = EventListener::ForExtension(
      event_name, extension_id, process, std::unique_ptr<DictionaryValue>());
  listeners_.RemoveListener(listener.get());
}

void EventRouter::AddEventListenerForURL(const std::string& event_name,
                                         content::RenderProcessHost* process,
                                         const GURL& listener_url) {
  listeners_.AddListener(EventListener::ForURL(
      event_name, listener_url, process, std::unique_ptr<DictionaryValue>()));
}

void EventRouter::RemoveEventListenerForURL(const std::string& event_name,
                                            content::RenderProcessHost* process,
                                            const GURL& listener_url) {
  std::unique_ptr<EventListener> listener = EventListener::ForURL(
      event_name, listener_url, process, std::unique_ptr<DictionaryValue>());
  listeners_.RemoveListener(listener.get());
}

void EventRouter::RegisterObserver(Observer* observer,
                                   const std::string& event_name) {
  // Observing sub-event names like "foo.onBar/123" is not allowed.
  DCHECK(event_name.find('/') == std::string::npos);
  observers_[event_name] = observer;
}

void EventRouter::UnregisterObserver(Observer* observer) {
  std::vector<ObserverMap::iterator> iters_to_remove;
  for (ObserverMap::iterator iter = observers_.begin();
       iter != observers_.end(); ++iter) {
    if (iter->second == observer)
      iters_to_remove.push_back(iter);
  }
  for (size_t i = 0; i < iters_to_remove.size(); ++i)
    observers_.erase(iters_to_remove[i]);
}

void EventRouter::OnListenerAdded(const EventListener* listener) {
  const EventListenerInfo details(listener->event_name(),
                                  listener->extension_id(),
                                  listener->listener_url(),
                                  listener->GetBrowserContext());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  ObserverMap::iterator observer = observers_.find(base_event_name);
  if (observer != observers_.end())
    observer->second->OnListenerAdded(details);

  content::RenderProcessHost* process = listener->process();
  if (process) {
    bool inserted = observed_process_set_.insert(process).second;
    if (inserted)
      process->AddObserver(this);
  }
}

void EventRouter::OnListenerRemoved(const EventListener* listener) {
  const EventListenerInfo details(listener->event_name(),
                                  listener->extension_id(),
                                  listener->listener_url(),
                                  listener->GetBrowserContext());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  ObserverMap::iterator observer = observers_.find(base_event_name);
  if (observer != observers_.end())
    observer->second->OnListenerRemoved(details);
}

void EventRouter::RenderProcessExited(content::RenderProcessHost* host,
                                      base::TerminationStatus status,
                                      int exit_code) {
  listeners_.RemoveListenersForProcess(host);
  observed_process_set_.erase(host);
  host->RemoveObserver(this);
}

void EventRouter::RenderProcessHostDestroyed(content::RenderProcessHost* host) {
  listeners_.RemoveListenersForProcess(host);
  observed_process_set_.erase(host);
  host->RemoveObserver(this);
}

void EventRouter::AddLazyEventListener(const std::string& event_name,
                                       const std::string& extension_id) {
  bool is_new = listeners_.AddListener(EventListener::ForExtension(
      event_name, extension_id, NULL, std::unique_ptr<DictionaryValue>()));

  if (is_new) {
    std::set<std::string> events = GetRegisteredEvents(extension_id);
    bool prefs_is_new = events.insert(event_name).second;
    if (prefs_is_new)
      SetRegisteredEvents(extension_id, events);
  }
}

void EventRouter::RemoveLazyEventListener(const std::string& event_name,
                                          const std::string& extension_id) {
  std::unique_ptr<EventListener> listener = EventListener::ForExtension(
      event_name, extension_id, NULL, std::unique_ptr<DictionaryValue>());
  bool did_exist = listeners_.RemoveListener(listener.get());

  if (did_exist) {
    std::set<std::string> events = GetRegisteredEvents(extension_id);
    bool prefs_did_exist = events.erase(event_name) > 0;
    DCHECK(prefs_did_exist);
    SetRegisteredEvents(extension_id, events);
  }
}

void EventRouter::AddFilteredEventListener(const std::string& event_name,
                                           content::RenderProcessHost* process,
                                           const std::string& extension_id,
                                           const base::DictionaryValue& filter,
                                           bool add_lazy_listener) {
  listeners_.AddListener(EventListener::ForExtension(
      event_name, extension_id, process,
      std::unique_ptr<DictionaryValue>(filter.DeepCopy())));

  if (add_lazy_listener) {
    bool added = listeners_.AddListener(EventListener::ForExtension(
        event_name, extension_id, NULL,
        std::unique_ptr<DictionaryValue>(filter.DeepCopy())));

    if (added)
      AddFilterToEvent(event_name, extension_id, &filter);
  }
}

void EventRouter::RemoveFilteredEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const std::string& extension_id,
    const base::DictionaryValue& filter,
    bool remove_lazy_listener) {
  std::unique_ptr<EventListener> listener = EventListener::ForExtension(
      event_name, extension_id, process,
      std::unique_ptr<DictionaryValue>(filter.DeepCopy()));

  listeners_.RemoveListener(listener.get());

  if (remove_lazy_listener) {
    listener->MakeLazy();
    bool removed = listeners_.RemoveListener(listener.get());

    if (removed)
      RemoveFilterFromEvent(event_name, extension_id, &filter);
  }
}

bool EventRouter::HasEventListener(const std::string& event_name) {
  return listeners_.HasListenerForEvent(event_name);
}

bool EventRouter::ExtensionHasEventListener(const std::string& extension_id,
                                            const std::string& event_name) {
  return listeners_.HasListenerForExtension(extension_id, event_name);
}

bool EventRouter::HasEventListenerImpl(const ListenerMap& listener_map,
                                       const std::string& extension_id,
                                       const std::string& event_name) {
  ListenerMap::const_iterator it = listener_map.find(event_name);
  if (it == listener_map.end())
    return false;

  const std::set<ListenerProcess>& listeners = it->second;
  if (extension_id.empty())
    return !listeners.empty();

  for (std::set<ListenerProcess>::const_iterator listener = listeners.begin();
       listener != listeners.end(); ++listener) {
    if (listener->extension_id == extension_id)
      return true;
  }
  return false;
}

std::set<std::string> EventRouter::GetRegisteredEvents(
    const std::string& extension_id) {
  std::set<std::string> events;
  const ListValue* events_value = NULL;

  if (!extension_prefs_ ||
      !extension_prefs_->ReadPrefAsList(
           extension_id, kRegisteredEvents, &events_value)) {
    return events;
  }

  for (size_t i = 0; i < events_value->GetSize(); ++i) {
    std::string event;
    if (events_value->GetString(i, &event))
      events.insert(event);
  }
  return events;
}

void EventRouter::SetRegisteredEvents(const std::string& extension_id,
                                      const std::set<std::string>& events) {
  ListValue* events_value = new ListValue;
  for (std::set<std::string>::const_iterator iter = events.begin();
       iter != events.end(); ++iter) {
    events_value->AppendString(*iter);
  }
  extension_prefs_->UpdateExtensionPref(
      extension_id, kRegisteredEvents, events_value);
}

void EventRouter::AddFilterToEvent(const std::string& event_name,
                                   const std::string& extension_id,
                                   const DictionaryValue* filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension_id, kFilteredEvents);
  DictionaryValue* filtered_events = update.Get();
  if (!filtered_events)
    filtered_events = update.Create();

  ListValue* filter_list = NULL;
  if (!filtered_events->GetList(event_name, &filter_list)) {
    filter_list = new ListValue;
    filtered_events->SetWithoutPathExpansion(event_name, filter_list);
  }

  filter_list->Append(filter->DeepCopy());
}

void EventRouter::RemoveFilterFromEvent(const std::string& event_name,
                                        const std::string& extension_id,
                                        const DictionaryValue* filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension_id, kFilteredEvents);
  DictionaryValue* filtered_events = update.Get();
  ListValue* filter_list = NULL;
  if (!filtered_events ||
      !filtered_events->GetListWithoutPathExpansion(event_name, &filter_list)) {
    return;
  }

  for (size_t i = 0; i < filter_list->GetSize(); i++) {
    DictionaryValue* filter = NULL;
    CHECK(filter_list->GetDictionary(i, &filter));
    if (filter->Equals(filter)) {
      filter_list->Remove(i, NULL);
      break;
    }
  }
}

const DictionaryValue* EventRouter::GetFilteredEvents(
    const std::string& extension_id) {
  const DictionaryValue* events = NULL;
  extension_prefs_->ReadPrefAsDictionary(
      extension_id, kFilteredEvents, &events);
  return events;
}

void EventRouter::BroadcastEvent(std::unique_ptr<Event> event) {
  DispatchEventImpl(std::string(), linked_ptr<Event>(event.release()));
}

void EventRouter::DispatchEventToExtension(const std::string& extension_id,
                                           std::unique_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  DispatchEventImpl(extension_id, linked_ptr<Event>(event.release()));
}

void EventRouter::DispatchEventWithLazyListener(const std::string& extension_id,
                                                std::unique_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  std::string event_name = event->event_name;
  bool has_listener = ExtensionHasEventListener(extension_id, event_name);
  if (!has_listener)
    AddLazyEventListener(event_name, extension_id);
  DispatchEventToExtension(extension_id, std::move(event));
  if (!has_listener)
    RemoveLazyEventListener(event_name, extension_id);
}

void EventRouter::DispatchEventImpl(const std::string& restrict_to_extension_id,
                                    const linked_ptr<Event>& event) {
  // We don't expect to get events from a completely different browser context.
  DCHECK(!event->restrict_to_browser_context ||
         ExtensionsBrowserClient::Get()->IsSameContext(
             browser_context_, event->restrict_to_browser_context));

  std::set<const EventListener*> listeners(
      listeners_.GetEventListeners(*event));

  std::set<EventDispatchIdentifier> already_dispatched;

  // We dispatch events for lazy background pages first because attempting to do
  // so will cause those that are being suspended to cancel that suspension.
  // As canceling a suspension entails sending an event to the affected
  // background page, and as that event needs to be delivered before we dispatch
  // the event we are dispatching here, we dispatch to the lazy listeners here
  // first.
  for (const EventListener* listener : listeners) {
    if (restrict_to_extension_id.empty() ||
        restrict_to_extension_id == listener->extension_id()) {
      if (listener->IsLazy()) {
        DispatchLazyEvent(listener->extension_id(), event, &already_dispatched,
                          listener->filter());
      }
    }
  }

  for (const EventListener* listener : listeners) {
    if (restrict_to_extension_id.empty() ||
        restrict_to_extension_id == listener->extension_id()) {
      if (listener->process()) {
        EventDispatchIdentifier dispatch_id(listener->GetBrowserContext(),
                                            listener->extension_id());
        if (!ContainsKey(already_dispatched, dispatch_id)) {
          DispatchEventToProcess(listener->extension_id(),
                                 listener->listener_url(), listener->process(),
                                 event, listener->filter(),
                                 false /* did_enqueue */);
        }
      }
    }
  }
}

void EventRouter::DispatchLazyEvent(
    const std::string& extension_id,
    const linked_ptr<Event>& event,
    std::set<EventDispatchIdentifier>* already_dispatched,
    const base::DictionaryValue* listener_filter) {
  // Check both the original and the incognito browser context to see if we
  // should load a lazy bg page to handle the event. The latter case
  // occurs in the case of split-mode extensions.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return;

  if (MaybeLoadLazyBackgroundPageToDispatchEvent(browser_context_, extension,
                                                 event, listener_filter)) {
    already_dispatched->insert(std::make_pair(browser_context_, extension_id));
  }

  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (browser_client->HasOffTheRecordContext(browser_context_) &&
      IncognitoInfo::IsSplitMode(extension)) {
    BrowserContext* incognito_context =
        browser_client->GetOffTheRecordContext(browser_context_);
    if (MaybeLoadLazyBackgroundPageToDispatchEvent(incognito_context, extension,
                                                   event, listener_filter)) {
      already_dispatched->insert(
          std::make_pair(incognito_context, extension_id));
    }
  }
}

void EventRouter::DispatchEventToProcess(
    const std::string& extension_id,
    const GURL& listener_url,
    content::RenderProcessHost* process,
    const linked_ptr<Event>& event,
    const base::DictionaryValue* listener_filter,
    bool did_enqueue) {
  BrowserContext* listener_context = process->GetBrowserContext();
  ProcessMap* process_map = ProcessMap::Get(listener_context);

  // NOTE: |extension| being NULL does not necessarily imply that this event
  // shouldn't be dispatched. Events can be dispatched to WebUI and webviews as
  // well.  It all depends on what GetMostLikelyContextType returns.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions().GetByID(
          extension_id);

  if (!extension && !extension_id.empty()) {
    // Trying to dispatch an event to an extension that doesn't exist. The
    // extension could have been removed, but we do not unregister it until the
    // extension process is unloaded.
    return;
  }

  if (extension) {
    // Extension-specific checks.
    // Firstly, if the event is for a URL, the Extension must have permission
    // to access that URL.
    if (!event->event_url.is_empty() &&
        event->event_url.host() != extension->id() &&  // event for self is ok
        !extension->permissions_data()
             ->active_permissions()
             .HasEffectiveAccessToURL(event->event_url)) {
      return;
    }
    // Secondly, if the event is for incognito mode, the Extension must be
    // enabled in incognito mode.
    if (!CanDispatchEventToBrowserContext(listener_context, extension, event)) {
      return;
    }
  }

  Feature::Context target_context =
      process_map->GetMostLikelyContextType(extension, process->GetID());

  // We shouldn't be dispatching an event to a webpage, since all such events
  // (e.g.  messaging) don't go through EventRouter.
  DCHECK_NE(Feature::WEB_PAGE_CONTEXT, target_context)
      << "Trying to dispatch event " << event->event_name << " to a webpage,"
      << " but this shouldn't be possible";

  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          event->event_name, extension, target_context, listener_url);
  if (!availability.is_available()) {
    // It shouldn't be possible to reach here, because access is checked on
    // registration. However, for paranoia, check on dispatch as well.
    NOTREACHED() << "Trying to dispatch event " << event->event_name
                 << " which the target does not have access to: "
                 << availability.message();
    return;
  }

  if (!event->will_dispatch_callback.is_null() &&
      !event->will_dispatch_callback.Run(listener_context, extension,
                                         event.get(), listener_filter)) {
    return;
  }

  int event_id = g_extension_event_id.GetNext();
  DispatchExtensionMessage(process, listener_context, extension_id, event_id,
                           event->event_name, event->event_args.get(),
                           event->user_gesture, event->filter_info);

  if (extension) {
    ReportEvent(event->histogram_value, extension, did_enqueue);
    IncrementInFlightEvents(listener_context, extension, event_id,
                            event->event_name);
  }
}

bool EventRouter::CanDispatchEventToBrowserContext(
    BrowserContext* context,
    const Extension* extension,
    const linked_ptr<Event>& event) {
  // Is this event from a different browser context than the renderer (ie, an
  // incognito tab event sent to a normal process, or vice versa).
  bool cross_incognito = event->restrict_to_browser_context &&
                         context != event->restrict_to_browser_context;
  if (!cross_incognito)
    return true;
  return ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(
      extension, context);
}

bool EventRouter::MaybeLoadLazyBackgroundPageToDispatchEvent(
    BrowserContext* context,
    const Extension* extension,
    const linked_ptr<Event>& event,
    const base::DictionaryValue* listener_filter) {
  if (!CanDispatchEventToBrowserContext(context, extension, event))
    return false;

  LazyBackgroundTaskQueue* queue = LazyBackgroundTaskQueue::Get(context);
  if (queue->ShouldEnqueueTask(context, extension)) {
    linked_ptr<Event> dispatched_event(event);

    // If there's a dispatch callback, call it now (rather than dispatch time)
    // to avoid lifetime issues. Use a separate copy of the event args, so they
    // last until the event is dispatched.
    if (!event->will_dispatch_callback.is_null()) {
      dispatched_event.reset(event->DeepCopy());
      if (!dispatched_event->will_dispatch_callback.Run(
              context, extension, dispatched_event.get(), listener_filter)) {
        // The event has been canceled.
        return true;
      }
      // Ensure we don't call it again at dispatch time.
      dispatched_event->will_dispatch_callback.Reset();
    }

    queue->AddPendingTask(context, extension->id(),
                          base::Bind(&EventRouter::DispatchPendingEvent,
                                     base::Unretained(this), dispatched_event));
    return true;
  }

  return false;
}

// static
void EventRouter::DoDispatchEventToSenderBookkeepingOnUI(
    void* browser_context_id,
    const std::string& extension_id,
    int event_id,
    events::HistogramValue histogram_value,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      reinterpret_cast<BrowserContext*>(browser_context_id);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return;
  EventRouter* event_router = EventRouter::Get(browser_context);
  event_router->IncrementInFlightEvents(browser_context, extension, event_id,
                                        event_name);
  event_router->ReportEvent(histogram_value, extension,
                            false /* did_enqueue */);
}

void EventRouter::IncrementInFlightEvents(BrowserContext* context,
                                          const Extension* extension,
                                          int event_id,
                                          const std::string& event_name) {
  // TODO(chirantan): Turn this on once crbug.com/464513 is fixed.
  // DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Only increment in-flight events if the lazy background page is active,
  // because that's the only time we'll get an ACK.
  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    ProcessManager* pm = ProcessManager::Get(context);
    ExtensionHost* host = pm->GetBackgroundHostForExtension(extension->id());
    if (host) {
      pm->IncrementLazyKeepaliveCount(extension);
      host->OnBackgroundEventDispatched(event_name, event_id);
    }
  }
}

void EventRouter::OnEventAck(BrowserContext* context,
                             const std::string& extension_id) {
  ProcessManager* pm = ProcessManager::Get(context);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id);
  // The event ACK is routed to the background host, so this should never be
  // NULL.
  CHECK(host);
  // TODO(mpcomplete): We should never get this message unless
  // HasLazyBackgroundPage is true. Find out why we're getting it anyway.
  if (host->extension() &&
      BackgroundInfo::HasLazyBackgroundPage(host->extension()))
    pm->DecrementLazyKeepaliveCount(host->extension());
}

void EventRouter::ReportEvent(events::HistogramValue histogram_value,
                              const Extension* extension,
                              bool did_enqueue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record every event fired.
  UMA_HISTOGRAM_ENUMERATION("Extensions.Events.Dispatch", histogram_value,
                            events::ENUM_BOUNDARY);

  bool is_component = Manifest::IsComponentLocation(extension->location());

  // Record events for component extensions. These should be kept to a minimum,
  // especially if they wake its event page. Component extensions should use
  // declarative APIs as much as possible.
  if (is_component) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.Events.DispatchToComponent",
                              histogram_value, events::ENUM_BOUNDARY);
  }

  // Record events for background pages, if any. The most important statistic
  // is DispatchWithSuspendedEventPage. Events reported there woke an event
  // page. Implementing either filtered or declarative versions of these events
  // should be prioritised.
  //
  // Note: all we know is that the extension *has* a persistent or event page,
  // not that the event is being dispatched *to* such a page. However, this is
  // academic, since extensions with any background page have that background
  // page running (or in the case of suspended event pages, must be started)
  // regardless of where the event is being dispatched. Events are dispatched
  // to a *process* not a *frame*.
  if (BackgroundInfo::HasPersistentBackgroundPage(extension)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.Events.DispatchWithPersistentBackgroundPage",
        histogram_value, events::ENUM_BOUNDARY);
  } else if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    if (did_enqueue) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.Events.DispatchWithSuspendedEventPage", histogram_value,
          events::ENUM_BOUNDARY);
      if (is_component) {
        UMA_HISTOGRAM_ENUMERATION(
            "Extensions.Events.DispatchToComponentWithSuspendedEventPage",
            histogram_value, events::ENUM_BOUNDARY);
      }
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.Events.DispatchWithRunningEventPage", histogram_value,
          events::ENUM_BOUNDARY);
    }
  }
}

void EventRouter::DispatchPendingEvent(const linked_ptr<Event>& event,
                                       ExtensionHost* host) {
  if (!host)
    return;

  if (listeners_.HasProcessListener(host->render_process_host(),
                                    host->extension()->id())) {
    DispatchEventToProcess(host->extension()->id(), host->GetURL(),
                           host->render_process_host(), event, nullptr,
                           true /* did_enqueue */);
  }
}

void EventRouter::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_ENABLED: {
      // If the extension has a lazy background page, make sure it gets loaded
      // to register the events the extension is interested in.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
        LazyBackgroundTaskQueue* queue =
            LazyBackgroundTaskQueue::Get(browser_context_);
        queue->AddPendingTask(browser_context_, extension->id(),
                              base::Bind(&DoNothing));
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void EventRouter::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  // Add all registered lazy listeners to our cache.
  std::set<std::string> registered_events =
      GetRegisteredEvents(extension->id());
  listeners_.LoadUnfilteredLazyListeners(extension->id(), registered_events);
  const DictionaryValue* filtered_events = GetFilteredEvents(extension->id());
  if (filtered_events)
    listeners_.LoadFilteredLazyListeners(extension->id(), *filtered_events);
}

void EventRouter::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  // Remove all registered listeners from our cache.
  listeners_.RemoveListenersForExtension(extension->id());
}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             std::unique_ptr<base::ListValue> event_args)
    : Event(histogram_value, event_name, std::move(event_args), nullptr) {}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             std::unique_ptr<base::ListValue> event_args,
             BrowserContext* restrict_to_browser_context)
    : Event(histogram_value,
            event_name,
            std::move(event_args),
            restrict_to_browser_context,
            GURL(),
            EventRouter::USER_GESTURE_UNKNOWN,
            EventFilteringInfo()) {}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             std::unique_ptr<ListValue> event_args_tmp,
             BrowserContext* restrict_to_browser_context,
             const GURL& event_url,
             EventRouter::UserGestureState user_gesture,
             const EventFilteringInfo& filter_info)
    : histogram_value(histogram_value),
      event_name(event_name),
      event_args(std::move(event_args_tmp)),
      restrict_to_browser_context(restrict_to_browser_context),
      event_url(event_url),
      user_gesture(user_gesture),
      filter_info(filter_info) {
  DCHECK(event_args);
  DCHECK_NE(events::UNKNOWN, histogram_value)
      << "events::UNKNOWN cannot be used as a histogram value.\n"
      << "If this is a test, use events::FOR_TEST.\n"
      << "If this is production code, it is important that you use a realistic "
      << "value so that we can accurately track event usage. "
      << "See extension_event_histogram_value.h for inspiration.";
}

Event::~Event() {}

Event* Event::DeepCopy() {
  Event* copy = new Event(
      histogram_value, event_name,
      std::unique_ptr<base::ListValue>(event_args->DeepCopy()),
      restrict_to_browser_context, event_url, user_gesture, filter_info);
  copy->will_dispatch_callback = will_dispatch_callback;
  return copy;
}

EventListenerInfo::EventListenerInfo(const std::string& event_name,
                                     const std::string& extension_id,
                                     const GURL& listener_url,
                                     content::BrowserContext* browser_context)
    : event_name(event_name),
      extension_id(extension_id),
      listener_url(listener_url),
      browser_context(browser_context) {
}

}  // namespace extensions
