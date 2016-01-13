// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/scoped_dbus_error.h"
#include "dbus/test_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

namespace {

// Used to test AddFilterFunction().
DBusHandlerResult DummyHandler(DBusConnection* connection,
                               DBusMessage* raw_message,
                               void* user_data) {
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

// Test helper for BusTest.ListenForServiceOwnerChange that wraps a
// base::RunLoop. At Run() time, the caller pass in the expected number of
// quit calls, and at QuitIfConditionIsSatisified() time, only quit the RunLoop
// if the expected number of quit calls have been reached.
class RunLoopWithExpectedCount {
 public:
  RunLoopWithExpectedCount() : expected_quit_calls_(0), actual_quit_calls_(0) {}
  ~RunLoopWithExpectedCount() {}

  void Run(int expected_quit_calls) {
    DCHECK_EQ(0, expected_quit_calls_);
    DCHECK_EQ(0, actual_quit_calls_);
    expected_quit_calls_ = expected_quit_calls;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void QuitIfConditionIsSatisified() {
    if (++actual_quit_calls_ != expected_quit_calls_)
      return;
    run_loop_->Quit();
    expected_quit_calls_ = 0;
    actual_quit_calls_ = 0;
  }

 private:
  scoped_ptr<base::RunLoop> run_loop_;
  int expected_quit_calls_;
  int actual_quit_calls_;

  DISALLOW_COPY_AND_ASSIGN(RunLoopWithExpectedCount);
};

// Test helper for BusTest.ListenForServiceOwnerChange.
void OnServiceOwnerChanged(RunLoopWithExpectedCount* run_loop_state,
                           std::string* service_owner,
                           int* num_of_owner_changes,
                           const std::string& new_service_owner) {
  *service_owner = new_service_owner;
  ++(*num_of_owner_changes);
  run_loop_state->QuitIfConditionIsSatisified();
}

}  // namespace

TEST(BusTest, GetObjectProxy) {
  Bus::Options options;
  scoped_refptr<Bus> bus = new Bus(options);

  ObjectProxy* object_proxy1 =
      bus->GetObjectProxy("org.chromium.TestService",
                          ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  ObjectProxy* object_proxy2 =
      bus->GetObjectProxy("org.chromium.TestService",
                          ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  ObjectProxy* object_proxy3 =
      bus->GetObjectProxy(
          "org.chromium.TestService",
          ObjectPath("/org/chromium/DifferentTestObject"));
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);

  bus->ShutdownAndBlock();
}

TEST(BusTest, GetObjectProxyIgnoreUnknownService) {
  Bus::Options options;
  scoped_refptr<Bus> bus = new Bus(options);

  ObjectProxy* object_proxy1 =
      bus->GetObjectProxyWithOptions(
          "org.chromium.TestService",
          ObjectPath("/org/chromium/TestObject"),
          ObjectProxy::IGNORE_SERVICE_UNKNOWN_ERRORS);
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  ObjectProxy* object_proxy2 =
      bus->GetObjectProxyWithOptions(
          "org.chromium.TestService",
          ObjectPath("/org/chromium/TestObject"),
          ObjectProxy::IGNORE_SERVICE_UNKNOWN_ERRORS);
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  ObjectProxy* object_proxy3 =
      bus->GetObjectProxyWithOptions(
          "org.chromium.TestService",
          ObjectPath("/org/chromium/DifferentTestObject"),
          ObjectProxy::IGNORE_SERVICE_UNKNOWN_ERRORS);
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);

  bus->ShutdownAndBlock();
}

TEST(BusTest, RemoveObjectProxy) {
  // Setup the current thread's MessageLoop.
  base::MessageLoop message_loop;

  // Start the D-Bus thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  base::Thread dbus_thread("D-Bus thread");
  dbus_thread.StartWithOptions(thread_options);

  // Create the bus.
  Bus::Options options;
  options.dbus_task_runner = dbus_thread.message_loop_proxy();
  scoped_refptr<Bus> bus = new Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  // Try to remove a non existant object proxy should return false.
  ASSERT_FALSE(
      bus->RemoveObjectProxy("org.chromium.TestService",
                             ObjectPath("/org/chromium/TestObject"),
                             base::Bind(&base::DoNothing)));

  ObjectProxy* object_proxy1 =
      bus->GetObjectProxy("org.chromium.TestService",
                          ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // Increment the reference count to the object proxy to avoid destroying it
  // while removing the object.
  object_proxy1->AddRef();

  // Remove the object from the bus. This will invalidate any other usage of
  // object_proxy1 other than destroy it. We keep this object for a comparison
  // at a later time.
  ASSERT_TRUE(
      bus->RemoveObjectProxy("org.chromium.TestService",
                             ObjectPath("/org/chromium/TestObject"),
                             base::Bind(&base::DoNothing)));

  // This should return a different object because the first object was removed
  // from the bus, but not deleted from memory.
  ObjectProxy* object_proxy2 =
      bus->GetObjectProxy("org.chromium.TestService",
                          ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);

  // Compare the new object with the first object. The first object still exists
  // thanks to the increased reference.
  EXPECT_NE(object_proxy1, object_proxy2);

  // Release object_proxy1.
  object_proxy1->Release();

  // Shut down synchronously.
  bus->ShutdownOnDBusThreadAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
  dbus_thread.Stop();
}

TEST(BusTest, GetExportedObject) {
  Bus::Options options;
  scoped_refptr<Bus> bus = new Bus(options);

  ExportedObject* object_proxy1 =
      bus->GetExportedObject(ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // This should return the same object.
  ExportedObject* object_proxy2 =
      bus->GetExportedObject(ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);
  EXPECT_EQ(object_proxy1, object_proxy2);

  // This should not.
  ExportedObject* object_proxy3 =
      bus->GetExportedObject(
          ObjectPath("/org/chromium/DifferentTestObject"));
  ASSERT_TRUE(object_proxy3);
  EXPECT_NE(object_proxy1, object_proxy3);

  bus->ShutdownAndBlock();
}

TEST(BusTest, UnregisterExportedObject) {
  // Start the D-Bus thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  base::Thread dbus_thread("D-Bus thread");
  dbus_thread.StartWithOptions(thread_options);

  // Create the bus.
  Bus::Options options;
  options.dbus_task_runner = dbus_thread.message_loop_proxy();
  scoped_refptr<Bus> bus = new Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  ExportedObject* object_proxy1 =
      bus->GetExportedObject(ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy1);

  // Increment the reference count to the object proxy to avoid destroying it
  // calling UnregisterExportedObject. This ensures the dbus::ExportedObject is
  // not freed from memory. See http://crbug.com/137846 for details.
  object_proxy1->AddRef();

  bus->UnregisterExportedObject(ObjectPath("/org/chromium/TestObject"));

  // This should return a new object because the object_proxy1 is still in
  // alloc'ed memory.
  ExportedObject* object_proxy2 =
      bus->GetExportedObject(ObjectPath("/org/chromium/TestObject"));
  ASSERT_TRUE(object_proxy2);
  EXPECT_NE(object_proxy1, object_proxy2);

  // Release the incremented reference.
  object_proxy1->Release();

  // Shut down synchronously.
  bus->ShutdownOnDBusThreadAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
  dbus_thread.Stop();
}

TEST(BusTest, ShutdownAndBlock) {
  Bus::Options options;
  scoped_refptr<Bus> bus = new Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  // Shut down synchronously.
  bus->ShutdownAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
}

TEST(BusTest, ShutdownAndBlockWithDBusThread) {
  // Start the D-Bus thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  base::Thread dbus_thread("D-Bus thread");
  dbus_thread.StartWithOptions(thread_options);

  // Create the bus.
  Bus::Options options;
  options.dbus_task_runner = dbus_thread.message_loop_proxy();
  scoped_refptr<Bus> bus = new Bus(options);
  ASSERT_FALSE(bus->shutdown_completed());

  // Shut down synchronously.
  bus->ShutdownOnDBusThreadAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
  dbus_thread.Stop();
}

TEST(BusTest, AddFilterFunction) {
  Bus::Options options;
  scoped_refptr<Bus> bus = new Bus(options);
  // Should connect before calling AddFilterFunction().
  bus->Connect();

  int data1 = 100;
  int data2 = 200;
  ASSERT_TRUE(bus->AddFilterFunction(&DummyHandler, &data1));
  // Cannot add the same function with the same data.
  ASSERT_FALSE(bus->AddFilterFunction(&DummyHandler, &data1));
  // Can add the same function with different data.
  ASSERT_TRUE(bus->AddFilterFunction(&DummyHandler, &data2));

  ASSERT_TRUE(bus->RemoveFilterFunction(&DummyHandler, &data1));
  ASSERT_FALSE(bus->RemoveFilterFunction(&DummyHandler, &data1));
  ASSERT_TRUE(bus->RemoveFilterFunction(&DummyHandler, &data2));

  bus->ShutdownAndBlock();
}

TEST(BusTest, DoubleAddAndRemoveMatch) {
  Bus::Options options;
  scoped_refptr<Bus> bus = new Bus(options);
  ScopedDBusError error;

  bus->Connect();

  // Adds the same rule twice.
  bus->AddMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get());
  ASSERT_FALSE(error.is_set());

  bus->AddMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get());
  ASSERT_FALSE(error.is_set());

  // Removes the same rule twice.
  ASSERT_TRUE(bus->RemoveMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get()));
  ASSERT_FALSE(error.is_set());

  // The rule should be still in the bus since it was removed only once.
  // A second removal shouldn't give an error.
  ASSERT_TRUE(bus->RemoveMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get()));
  ASSERT_FALSE(error.is_set());

  // A third attemp to remove the same rule should fail.
  ASSERT_FALSE(bus->RemoveMatch(
      "type='signal',interface='org.chromium.TestService',path='/'",
      error.get()));

  bus->ShutdownAndBlock();
}

TEST(BusTest, ListenForServiceOwnerChange) {
  // Setup the current thread's MessageLoop. Must be of TYPE_IO for the
  // listeners to work.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);
  RunLoopWithExpectedCount run_loop_state;

  // Create the bus.
  Bus::Options bus_options;
  scoped_refptr<Bus> bus = new Bus(bus_options);

  // Add a listener.
  std::string service_owner1;
  int num_of_owner_changes1 = 0;
  Bus::GetServiceOwnerCallback callback1 =
      base::Bind(&OnServiceOwnerChanged,
                 &run_loop_state,
                 &service_owner1,
                 &num_of_owner_changes1);
  bus->ListenForServiceOwnerChange("org.chromium.TestService", callback1);
  // This should be a no-op.
  bus->ListenForServiceOwnerChange("org.chromium.TestService", callback1);
  base::RunLoop().RunUntilIdle();

  // Nothing has happened yet. Check initial state.
  EXPECT_TRUE(service_owner1.empty());
  EXPECT_EQ(0, num_of_owner_changes1);

  // Make an ownership change.
  ASSERT_TRUE(bus->RequestOwnershipAndBlock("org.chromium.TestService",
                                            Bus::REQUIRE_PRIMARY));
  run_loop_state.Run(1);

  {
    // Get the current service owner and check to make sure the listener got
    // the right value.
    std::string current_service_owner =
        bus->GetServiceOwnerAndBlock("org.chromium.TestService",
                                     Bus::REPORT_ERRORS);
    ASSERT_FALSE(current_service_owner.empty());

    // Make sure the listener heard about the new owner.
    EXPECT_EQ(current_service_owner, service_owner1);

    // Test the second ListenForServiceOwnerChange() above is indeed a no-op.
    EXPECT_EQ(1, num_of_owner_changes1);
  }

  // Add a second listener.
  std::string service_owner2;
  int num_of_owner_changes2 = 0;
  Bus::GetServiceOwnerCallback callback2 =
      base::Bind(&OnServiceOwnerChanged,
                 &run_loop_state,
                 &service_owner2,
                 &num_of_owner_changes2);
  bus->ListenForServiceOwnerChange("org.chromium.TestService", callback2);
  base::RunLoop().RunUntilIdle();

  // Release the ownership and make sure the service owner listeners fire with
  // the right values and the right number of times.
  ASSERT_TRUE(bus->ReleaseOwnership("org.chromium.TestService"));
  run_loop_state.Run(2);

  EXPECT_TRUE(service_owner1.empty());
  EXPECT_TRUE(service_owner2.empty());
  EXPECT_EQ(2, num_of_owner_changes1);
  EXPECT_EQ(1, num_of_owner_changes2);

  // Unlisten so shutdown can proceed correctly.
  bus->UnlistenForServiceOwnerChange("org.chromium.TestService", callback1);
  bus->UnlistenForServiceOwnerChange("org.chromium.TestService", callback2);
  base::RunLoop().RunUntilIdle();

  // Shut down synchronously.
  bus->ShutdownAndBlock();
  EXPECT_TRUE(bus->shutdown_completed());
}

}  // namespace dbus
