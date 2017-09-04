// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_EVENT_ROUTER_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_observer.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/event_filtering_info.h"
#include "ipc/ipc_sender.h"
#include "url/gurl.h"

class GURL;
class PrefService;

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace extensions {
class ActivityLog;
class Extension;
class ExtensionHost;
class ExtensionPrefs;
class ExtensionRegistry;

struct Event;
struct EventDispatchInfo;
struct EventListenerInfo;

class EventRouter : public KeyedService,
                    public content::NotificationObserver,
                    public ExtensionRegistryObserver,
                    public EventListenerMap::Delegate,
                    public content::RenderProcessHostObserver {
 public:
  // These constants convey the state of our knowledge of whether we're in
  // a user-caused gesture as part of DispatchEvent.
  enum UserGestureState {
    USER_GESTURE_UNKNOWN = 0,
    USER_GESTURE_ENABLED = 1,
    USER_GESTURE_NOT_ENABLED = 2,
  };

  // The pref key for the list of event names for which an extension has
  // registered from its lazy background page.
  static const char kRegisteredEvents[];

  // Observers register interest in events with a particular name and are
  // notified when a listener is added or removed. Observers are matched by
  // the base name of the event (e.g. adding an event listener for event name
  // "foo.onBar/123" will trigger observers registered for "foo.onBar").
  class Observer {
   public:
    // Called when a listener is added.
    virtual void OnListenerAdded(const EventListenerInfo& details) {}
    // Called when a listener is removed.
    virtual void OnListenerRemoved(const EventListenerInfo& details) {}
  };

  // Gets the EventRouter for |browser_context|.
  static EventRouter* Get(content::BrowserContext* browser_context);

  // Converts event names like "foo.onBar/123" into "foo.onBar". Event names
  // without a "/" are returned unchanged.
  static std::string GetBaseEventName(const std::string& full_event_name);

  // Sends an event via ipc_sender to the given extension. Can be called on any
  // thread.
  //
  // It is very rare to call this function directly. Instead use the instance
  // methods BroadcastEvent or DispatchEventToExtension.
  static void DispatchEventToSender(IPC::Sender* ipc_sender,
                                    void* browser_context_id,
                                    const std::string& extension_id,
                                    events::HistogramValue histogram_value,
                                    const std::string& event_name,
                                    std::unique_ptr<base::ListValue> event_args,
                                    UserGestureState user_gesture,
                                    const EventFilteringInfo& info);

  // An EventRouter is shared between |browser_context| and its associated
  // incognito context. |extension_prefs| may be NULL in tests.
  EventRouter(content::BrowserContext* browser_context,
              ExtensionPrefs* extension_prefs);
  ~EventRouter() override;

  // Add or remove an extension as an event listener for |event_name|.
  //
  // Note that multiple extensions can share a process due to process
  // collapsing. Also, a single extension can have 2 processes if it is a split
  // mode extension.
  void AddEventListener(const std::string& event_name,
                        content::RenderProcessHost* process,
                        const std::string& extension_id);
  void RemoveEventListener(const std::string& event_name,
                           content::RenderProcessHost* process,
                           const std::string& extension_id);

  // Add or remove a URL as an event listener for |event_name|.
  void AddEventListenerForURL(const std::string& event_name,
                              content::RenderProcessHost* process,
                              const GURL& listener_url);
  void RemoveEventListenerForURL(const std::string& event_name,
                                 content::RenderProcessHost* process,
                                 const GURL& listener_url);

  EventListenerMap& listeners() { return listeners_; }

  // Registers an observer to be notified when an event listener for
  // |event_name| is added or removed. There can currently be only one observer
  // for each distinct |event_name|.
  void RegisterObserver(Observer* observer,
                        const std::string& event_name);

  // Unregisters an observer from all events.
  void UnregisterObserver(Observer* observer);

  // Add or remove the extension as having a lazy background page that listens
  // to the event. The difference from the above methods is that these will be
  // remembered even after the process goes away. We use this list to decide
  // which extension pages to load when dispatching an event.
  void AddLazyEventListener(const std::string& event_name,
                            const std::string& extension_id);
  void RemoveLazyEventListener(const std::string& event_name,
                               const std::string& extension_id);

  // If |add_lazy_listener| is true also add the lazy version of this listener.
  void AddFilteredEventListener(const std::string& event_name,
                                content::RenderProcessHost* process,
                                const std::string& extension_id,
                                const base::DictionaryValue& filter,
                                bool add_lazy_listener);

  // If |remove_lazy_listener| is true also remove the lazy version of this
  // listener.
  void RemoveFilteredEventListener(const std::string& event_name,
                                   content::RenderProcessHost* process,
                                   const std::string& extension_id,
                                   const base::DictionaryValue& filter,
                                   bool remove_lazy_listener);

  // Returns true if there is at least one listener for the given event.
  bool HasEventListener(const std::string& event_name);

  // Returns true if the extension is listening to the given event.
  virtual bool ExtensionHasEventListener(const std::string& extension_id,
                                         const std::string& event_name);

  // Return or set the list of events for which the given extension has
  // registered.
  std::set<std::string> GetRegisteredEvents(const std::string& extension_id);
  void SetRegisteredEvents(const std::string& extension_id,
                           const std::set<std::string>& events);

  // Broadcasts an event to every listener registered for that event.
  virtual void BroadcastEvent(std::unique_ptr<Event> event);

  // Dispatches an event to the given extension.
  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        std::unique_ptr<Event> event);

  // Dispatches |event| to the given extension as if the extension has a lazy
  // listener for it. NOTE: This should be used rarely, for dispatching events
  // to extensions that haven't had a chance to add their own listeners yet, eg:
  // newly installed extensions.
  void DispatchEventWithLazyListener(const std::string& extension_id,
                                     std::unique_ptr<Event> event);

  // Record the Event Ack from the renderer. (One less event in-flight.)
  void OnEventAck(content::BrowserContext* context,
                  const std::string& extension_id);

  // Reports UMA for an event dispatched to |extension| with histogram value
  // |histogram_value|. Must be called on the UI thread.
  //
  // |did_enqueue| should be true if the event was queued waiting for a process
  // to start, like an event page.
  void ReportEvent(events::HistogramValue histogram_value,
                   const Extension* extension,
                   bool did_enqueue);

 private:
  friend class EventRouterTest;

  // The extension and process that contains the event listener for a given
  // event.
  struct ListenerProcess;

  // A map between an event name and a set of extensions that are listening
  // to that event.
  typedef std::map<std::string, std::set<ListenerProcess> > ListenerMap;

  // An identifier for an event dispatch that is used to prevent double dispatch
  // due to race conditions between the direct and lazy dispatch paths.
  typedef std::pair<const content::BrowserContext*, std::string>
      EventDispatchIdentifier;

  // TODO(gdk): Document this.
  static void DispatchExtensionMessage(
      IPC::Sender* ipc_sender,
      void* browser_context_id,
      const std::string& extension_id,
      int event_id,
      const std::string& event_name,
      base::ListValue* event_args,
      UserGestureState user_gesture,
      const extensions::EventFilteringInfo& info);

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;
  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  // Returns true if the given listener map contains a event listeners for
  // the given event. If |extension_id| is non-empty, we also check that that
  // extension is one of the listeners.
  bool HasEventListenerImpl(const ListenerMap& listeners,
                            const std::string& extension_id,
                            const std::string& event_name);

  // Shared by all event dispatch methods. If |restrict_to_extension_id| is
  // empty, the event is broadcast.  An event that just came off the pending
  // list may not be delayed again.
  void DispatchEventImpl(const std::string& restrict_to_extension_id,
                         const linked_ptr<Event>& event);

  // Ensures that all lazy background pages that are interested in the given
  // event are loaded, and queues the event if the page is not ready yet.
  // Inserts an EventDispatchIdentifier into |already_dispatched| for each lazy
  // event dispatch that is queued.
  void DispatchLazyEvent(const std::string& extension_id,
                         const linked_ptr<Event>& event,
                         std::set<EventDispatchIdentifier>* already_dispatched,
                         const base::DictionaryValue* listener_filter);

  // Dispatches the event to the specified extension or URL running in
  // |process|.
  void DispatchEventToProcess(const std::string& extension_id,
                              const GURL& listener_url,
                              content::RenderProcessHost* process,
                              const linked_ptr<Event>& event,
                              const base::DictionaryValue* listener_filter,
                              bool did_enqueue);

  // Returns false when the event is scoped to a context and the listening
  // extension does not have access to events from that context. Also fills
  // |event_args| with the proper arguments to send, which may differ if
  // the event crosses the incognito boundary.
  bool CanDispatchEventToBrowserContext(content::BrowserContext* context,
                                        const Extension* extension,
                                        const linked_ptr<Event>& event);

  // Possibly loads given extension's background page in preparation to
  // dispatch an event.  Returns true if the event was queued for subsequent
  // dispatch, false otherwise.
  bool MaybeLoadLazyBackgroundPageToDispatchEvent(
      content::BrowserContext* context,
      const Extension* extension,
      const linked_ptr<Event>& event,
      const base::DictionaryValue* listener_filter);

  // Adds a filter to an event.
  void AddFilterToEvent(const std::string& event_name,
                        const std::string& extension_id,
                        const base::DictionaryValue* filter);

  // Removes a filter from an event.
  void RemoveFilterFromEvent(const std::string& event_name,
                             const std::string& extension_id,
                             const base::DictionaryValue* filter);

  // Returns the dictionary of event filters that the given extension has
  // registered.
  const base::DictionaryValue* GetFilteredEvents(
      const std::string& extension_id);

  // Track the dispatched events that have not yet sent an ACK from the
  // renderer.
  void IncrementInFlightEvents(content::BrowserContext* context,
                               const Extension* extension,
                               int event_id,
                               const std::string& event_name);

  // static
  static void DoDispatchEventToSenderBookkeepingOnUI(
      void* browser_context_id,
      const std::string& extension_id,
      int event_id,
      events::HistogramValue histogram_value,
      const std::string& event_name);

  void DispatchPendingEvent(const linked_ptr<Event>& event,
                            ExtensionHost* host);

  // Implementation of EventListenerMap::Delegate.
  void OnListenerAdded(const EventListener* listener) override;
  void OnListenerRemoved(const EventListener* listener) override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(content::RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  content::BrowserContext* browser_context_;

  // The ExtensionPrefs associated with |browser_context_|. May be NULL in
  // tests.
  ExtensionPrefs* extension_prefs_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  EventListenerMap listeners_;

  // Map from base event name to observer.
  typedef base::hash_map<std::string, Observer*> ObserverMap;
  ObserverMap observers_;

  std::set<content::RenderProcessHost*> observed_process_set_;

  DISALLOW_COPY_AND_ASSIGN(EventRouter);
};

struct Event {
  // This callback should return true if the event should be dispatched to the
  // given context and extension, and false otherwise.
  typedef base::Callback<bool(content::BrowserContext*,
                              const Extension*,
                              Event*,
                              const base::DictionaryValue*)>
      WillDispatchCallback;

  // The identifier for the event, for histograms. In most cases this
  // correlates 1:1 with |event_name|, in some cases events will generate
  // their own names, but they cannot generate their own identifier.
  events::HistogramValue histogram_value;

  // The event to dispatch.
  std::string event_name;

  // Arguments to send to the event listener.
  std::unique_ptr<base::ListValue> event_args;

  // If non-NULL, then the event will not be sent to other BrowserContexts
  // unless the extension has permission (e.g. incognito tab update -> normal
  // tab only works if extension is allowed incognito access).
  content::BrowserContext* restrict_to_browser_context;

  // If not empty, the event is only sent to extensions with host permissions
  // for this url.
  GURL event_url;

  // Whether a user gesture triggered the event.
  EventRouter::UserGestureState user_gesture;

  // Extra information used to filter which events are sent to the listener.
  EventFilteringInfo filter_info;

  // If specified, this is called before dispatching an event to each
  // extension. The third argument is a mutable reference to event_args,
  // allowing the caller to provide different arguments depending on the
  // extension and profile. This is guaranteed to be called synchronously with
  // DispatchEvent, so callers don't need to worry about lifetime.
  //
  // NOTE: the Extension argument to this may be NULL because it's possible for
  // this event to be dispatched to non-extension processes, like WebUI.
  WillDispatchCallback will_dispatch_callback;

  Event(events::HistogramValue histogram_value,
        const std::string& event_name,
        std::unique_ptr<base::ListValue> event_args);

  Event(events::HistogramValue histogram_value,
        const std::string& event_name,
        std::unique_ptr<base::ListValue> event_args,
        content::BrowserContext* restrict_to_browser_context);

  Event(events::HistogramValue histogram_value,
        const std::string& event_name,
        std::unique_ptr<base::ListValue> event_args,
        content::BrowserContext* restrict_to_browser_context,
        const GURL& event_url,
        EventRouter::UserGestureState user_gesture,
        const EventFilteringInfo& info);

  ~Event();

  // Makes a deep copy of this instance. Ownership is transferred to the
  // caller.
  Event* DeepCopy();
};

struct EventListenerInfo {
  EventListenerInfo(const std::string& event_name,
                    const std::string& extension_id,
                    const GURL& listener_url,
                    content::BrowserContext* browser_context);
  // The event name including any sub-event, e.g. "runtime.onStartup" or
  // "webRequest.onCompleted/123".
  const std::string event_name;

  const std::string extension_id;
  const GURL listener_url;
  content::BrowserContext* browser_context;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENT_ROUTER_H_
