// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/values.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// A simple mock to keep track of listener additions and removals.
class MockEventRouterObserver : public EventRouter::Observer {
 public:
  MockEventRouterObserver()
      : listener_added_count_(0),
        listener_removed_count_(0) {}
  virtual ~MockEventRouterObserver() {}

  int listener_added_count() const { return listener_added_count_; }
  int listener_removed_count() const { return listener_removed_count_; }
  const std::string& last_event_name() const { return last_event_name_; }

  void Reset() {
    listener_added_count_ = 0;
    listener_removed_count_ = 0;
    last_event_name_.clear();
  }

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override {
    listener_added_count_++;
    last_event_name_ = details.event_name;
  }

  void OnListenerRemoved(const EventListenerInfo& details) override {
    listener_removed_count_++;
    last_event_name_ = details.event_name;
  }

 private:
  int listener_added_count_;
  int listener_removed_count_;
  std::string last_event_name_;

  DISALLOW_COPY_AND_ASSIGN(MockEventRouterObserver);
};

typedef base::Callback<std::unique_ptr<EventListener>(
    const std::string&,           // event_name
    content::RenderProcessHost*,  // process
    base::DictionaryValue*        // filter (takes ownership)
    )>
    EventListenerConstructor;

std::unique_ptr<EventListener> CreateEventListenerForExtension(
    const std::string& extension_id,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::DictionaryValue* filter) {
  return EventListener::ForExtension(event_name, extension_id, process,
                                     base::WrapUnique(filter));
}

std::unique_ptr<EventListener> CreateEventListenerForURL(
    const GURL& listener_url,
    const std::string& event_name,
    content::RenderProcessHost* process,
    base::DictionaryValue* filter) {
  return EventListener::ForURL(event_name, listener_url, process,
                               base::WrapUnique(filter));
}

// Creates an extension.  If |component| is true, it is created as a component
// extension.  If |persistent| is true, it is created with a persistent
// background page; otherwise it is created with an event page.
scoped_refptr<Extension> CreateExtension(bool component, bool persistent) {
  ExtensionBuilder builder;
  std::unique_ptr<base::DictionaryValue> manifest =
      base::WrapUnique(new base::DictionaryValue());
  manifest->SetString("name", "foo");
  manifest->SetString("version", "1.0.0");
  manifest->SetInteger("manifest_version", 2);
  manifest->SetString("background.page", "background.html");
  manifest->SetBoolean("background.persistent", persistent);
  builder.SetManifest(std::move(manifest));
  if (component)
    builder.SetLocation(Manifest::Location::COMPONENT);

  return builder.Build();
}

}  // namespace

class EventRouterTest : public ExtensionsTest {
 public:
  EventRouterTest()
      : notification_service_(content::NotificationService::Create()) {}

 protected:
  // Tests adding and removing observers from EventRouter.
  void RunEventRouterObserverTest(const EventListenerConstructor& constructor);

  // Tests that the correct counts are recorded for the Extensions.Events
  // histograms.
  void ExpectHistogramCounts(int dispatch_count,
                             int component_count,
                             int persistent_count,
                             int suspended_count,
                             int component_suspended_count,
                             int running_count) {
    if (dispatch_count) {
      histogram_tester_.ExpectBucketCount("Extensions.Events.Dispatch",
                                          events::HistogramValue::FOR_TEST,
                                          dispatch_count);
    }
    if (component_count) {
      histogram_tester_.ExpectBucketCount(
          "Extensions.Events.DispatchToComponent",
          events::HistogramValue::FOR_TEST, component_count);
    }
    if (persistent_count) {
      histogram_tester_.ExpectBucketCount(
          "Extensions.Events.DispatchWithPersistentBackgroundPage",
          events::HistogramValue::FOR_TEST, persistent_count);
    }
    if (suspended_count) {
      histogram_tester_.ExpectBucketCount(
          "Extensions.Events.DispatchWithSuspendedEventPage",
          events::HistogramValue::FOR_TEST, suspended_count);
    }
    if (component_suspended_count) {
      histogram_tester_.ExpectBucketCount(
          "Extensions.Events.DispatchToComponentWithSuspendedEventPage",
          events::HistogramValue::FOR_TEST, component_suspended_count);
    }
    if (running_count) {
      histogram_tester_.ExpectBucketCount(
          "Extensions.Events.DispatchWithRunningEventPage",
          events::HistogramValue::FOR_TEST, running_count);
    }
  }

 private:
  std::unique_ptr<content::NotificationService> notification_service_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(EventRouterTest);
};

TEST_F(EventRouterTest, GetBaseEventName) {
  // Normal event names are passed through unchanged.
  EXPECT_EQ("foo.onBar", EventRouter::GetBaseEventName("foo.onBar"));

  // Sub-events are converted to the part before the slash.
  EXPECT_EQ("foo.onBar", EventRouter::GetBaseEventName("foo.onBar/123"));
}

// Tests adding and removing observers from EventRouter.
void EventRouterTest::RunEventRouterObserverTest(
    const EventListenerConstructor& constructor) {
  EventRouter router(NULL, NULL);
  std::unique_ptr<EventListener> listener =
      constructor.Run("event_name", NULL, new base::DictionaryValue());

  // Add/remove works without any observers.
  router.OnListenerAdded(listener.get());
  router.OnListenerRemoved(listener.get());

  // Register observers that both match and don't match the event above.
  MockEventRouterObserver matching_observer;
  router.RegisterObserver(&matching_observer, "event_name");
  MockEventRouterObserver non_matching_observer;
  router.RegisterObserver(&non_matching_observer, "other");

  // Adding a listener notifies the appropriate observers.
  router.OnListenerAdded(listener.get());
  EXPECT_EQ(1, matching_observer.listener_added_count());
  EXPECT_EQ(0, non_matching_observer.listener_added_count());

  // Removing a listener notifies the appropriate observers.
  router.OnListenerRemoved(listener.get());
  EXPECT_EQ(1, matching_observer.listener_removed_count());
  EXPECT_EQ(0, non_matching_observer.listener_removed_count());

  // Adding the listener again notifies again.
  router.OnListenerAdded(listener.get());
  EXPECT_EQ(2, matching_observer.listener_added_count());
  EXPECT_EQ(0, non_matching_observer.listener_added_count());

  // Removing the listener again notifies again.
  router.OnListenerRemoved(listener.get());
  EXPECT_EQ(2, matching_observer.listener_removed_count());
  EXPECT_EQ(0, non_matching_observer.listener_removed_count());

  // Adding a listener with a sub-event notifies the main observer with
  // proper details.
  matching_observer.Reset();
  std::unique_ptr<EventListener> sub_event_listener =
      constructor.Run("event_name/1", NULL, new base::DictionaryValue());
  router.OnListenerAdded(sub_event_listener.get());
  EXPECT_EQ(1, matching_observer.listener_added_count());
  EXPECT_EQ(0, matching_observer.listener_removed_count());
  EXPECT_EQ("event_name/1", matching_observer.last_event_name());

  // Ditto for removing the listener.
  matching_observer.Reset();
  router.OnListenerRemoved(sub_event_listener.get());
  EXPECT_EQ(0, matching_observer.listener_added_count());
  EXPECT_EQ(1, matching_observer.listener_removed_count());
  EXPECT_EQ("event_name/1", matching_observer.last_event_name());
}

TEST_F(EventRouterTest, EventRouterObserverForExtensions) {
  RunEventRouterObserverTest(
      base::Bind(&CreateEventListenerForExtension, "extension_id"));
}

TEST_F(EventRouterTest, EventRouterObserverForURLs) {
  RunEventRouterObserverTest(
      base::Bind(&CreateEventListenerForURL, GURL("http://google.com/path")));
}

TEST_F(EventRouterTest, TestReportEvent) {
  EventRouter router(browser_context(), NULL);
  scoped_refptr<Extension> normal = test_util::CreateEmptyExtension("id1");
  router.ReportEvent(events::HistogramValue::FOR_TEST, normal.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(1 /** Dispatch */, 0 /** DispatchToComponent */,
                        0 /** DispatchWithPersistentBackgroundPage */,
                        0 /** DispatchWithSuspendedEventPage */,
                        0 /** DispatchToComponentWithSuspendedEventPage */,
                        0 /** DispatchWithRunningEventPage */);

  scoped_refptr<Extension> component =
      CreateExtension(true /** component */, true /** persistent */);
  router.ReportEvent(events::HistogramValue::FOR_TEST, component.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(2, 1, 1, 0, 0, 0);

  scoped_refptr<Extension> persistent = CreateExtension(false, true);
  router.ReportEvent(events::HistogramValue::FOR_TEST, persistent.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(3, 1, 2, 0, 0, 0);

  scoped_refptr<Extension> event = CreateExtension(false, false);
  router.ReportEvent(events::HistogramValue::FOR_TEST, event.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(4, 1, 2, 0, 0, 0);
  router.ReportEvent(events::HistogramValue::FOR_TEST, event.get(),
                     true /** did_enqueue */);
  ExpectHistogramCounts(5, 1, 2, 1, 0, 1);

  scoped_refptr<Extension> component_event = CreateExtension(true, false);
  router.ReportEvent(events::HistogramValue::FOR_TEST, component_event.get(),
                     false /** did_enqueue */);
  ExpectHistogramCounts(6, 2, 2, 1, 0, 2);
  router.ReportEvent(events::HistogramValue::FOR_TEST, component_event.get(),
                     true /** did_enqueue */);
  ExpectHistogramCounts(7, 3, 2, 2, 1, 2);
}

}  // namespace extensions
