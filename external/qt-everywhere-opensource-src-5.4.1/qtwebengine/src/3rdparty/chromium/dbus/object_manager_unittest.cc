// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_manager.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "dbus/test_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

// The object manager test exercises the asynchronous APIs in ObjectManager,
// and by extension PropertySet and Property<>.
class ObjectManagerTest
    : public testing::Test,
      public ObjectManager::Interface {
 public:
  ObjectManagerTest() {
  }

  struct Properties : public PropertySet {
    Property<std::string> name;
    Property<int16> version;
    Property<std::vector<std::string> > methods;
    Property<std::vector<ObjectPath> > objects;

    Properties(ObjectProxy* object_proxy,
               const std::string& interface_name,
               PropertyChangedCallback property_changed_callback)
        : PropertySet(object_proxy, interface_name, property_changed_callback) {
      RegisterProperty("Name", &name);
      RegisterProperty("Version", &version);
      RegisterProperty("Methods", &methods);
      RegisterProperty("Objects", &objects);
    }
  };

  virtual PropertySet* CreateProperties(
      ObjectProxy* object_proxy,
      const ObjectPath& object_path,
      const std::string& interface_name) OVERRIDE {
    Properties* properties = new Properties(
        object_proxy, interface_name,
        base::Bind(&ObjectManagerTest::OnPropertyChanged,
                   base::Unretained(this), object_path));
    return static_cast<PropertySet*>(properties);
  }

  virtual void SetUp() {
    // Make the main thread not to allow IO.
    base::ThreadRestrictions::SetIOAllowed(false);

    // Start the D-Bus thread.
    dbus_thread_.reset(new base::Thread("D-Bus Thread"));
    base::Thread::Options thread_options;
    thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
    ASSERT_TRUE(dbus_thread_->StartWithOptions(thread_options));

    // Start the test service, using the D-Bus thread.
    TestService::Options options;
    options.dbus_task_runner = dbus_thread_->message_loop_proxy();
    test_service_.reset(new TestService(options));
    ASSERT_TRUE(test_service_->StartService());
    ASSERT_TRUE(test_service_->WaitUntilServiceIsStarted());
    ASSERT_TRUE(test_service_->HasDBusThread());

    // Create the client, using the D-Bus thread.
    Bus::Options bus_options;
    bus_options.bus_type = Bus::SESSION;
    bus_options.connection_type = Bus::PRIVATE;
    bus_options.dbus_task_runner = dbus_thread_->message_loop_proxy();
    bus_ = new Bus(bus_options);
    ASSERT_TRUE(bus_->HasDBusThread());

    object_manager_ = bus_->GetObjectManager(
        "org.chromium.TestService",
        ObjectPath("/org/chromium/TestService"));
    object_manager_->RegisterInterface("org.chromium.TestInterface", this);

    object_manager_->GetManagedObjects();
    WaitForObject();
  }

  virtual void TearDown() {
    bus_->ShutdownOnDBusThreadAndBlock();

    // Shut down the service.
    test_service_->ShutdownAndBlock();

    // Reset to the default.
    base::ThreadRestrictions::SetIOAllowed(true);

    // Stopping a thread is considered an IO operation, so do this after
    // allowing IO.
    test_service_->Stop();
  }

  void MethodCallback(Response* response) {
    method_callback_called_ = true;
    message_loop_.Quit();
  }

 protected:
  // Called when an object is added.
  virtual void ObjectAdded(const ObjectPath& object_path,
                           const std::string& interface_name) OVERRIDE {
    added_objects_.push_back(std::make_pair(object_path, interface_name));
    message_loop_.Quit();
  }

  // Called when an object is removed.
  virtual void ObjectRemoved(const ObjectPath& object_path,
                             const std::string& interface_name) OVERRIDE {
    removed_objects_.push_back(std::make_pair(object_path, interface_name));
    message_loop_.Quit();
  }

  // Called when a property value is updated.
  void OnPropertyChanged(const ObjectPath& object_path,
                         const std::string& name) {
    updated_properties_.push_back(name);
    message_loop_.Quit();
  }

  static const size_t kExpectedObjects = 1;
  static const size_t kExpectedProperties = 4;

  void WaitForObject() {
    while (added_objects_.size() < kExpectedObjects ||
           updated_properties_.size() < kExpectedProperties)
      message_loop_.Run();
    for (size_t i = 0; i < kExpectedObjects; ++i)
      added_objects_.erase(added_objects_.begin());
    for (size_t i = 0; i < kExpectedProperties; ++i)
      updated_properties_.erase(updated_properties_.begin());
  }

  void WaitForRemoveObject() {
    while (removed_objects_.size() < kExpectedObjects)
      message_loop_.Run();
    for (size_t i = 0; i < kExpectedObjects; ++i)
      removed_objects_.erase(removed_objects_.begin());
  }

  void WaitForMethodCallback() {
    message_loop_.Run();
    method_callback_called_ = false;
  }

  void PerformAction(const std::string& action, const ObjectPath& object_path) {
    ObjectProxy* object_proxy = bus_->GetObjectProxy(
        "org.chromium.TestService",
        ObjectPath("/org/chromium/TestObject"));

    MethodCall method_call("org.chromium.TestInterface", "PerformAction");
    MessageWriter writer(&method_call);
    writer.AppendString(action);
    writer.AppendObjectPath(object_path);

    object_proxy->CallMethod(&method_call,
                             ObjectProxy::TIMEOUT_USE_DEFAULT,
                             base::Bind(&ObjectManagerTest::MethodCallback,
                                        base::Unretained(this)));
    WaitForMethodCallback();
  }

  base::MessageLoop message_loop_;
  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<Bus> bus_;
  ObjectManager* object_manager_;
  scoped_ptr<TestService> test_service_;

  std::vector<std::pair<ObjectPath, std::string> > added_objects_;
  std::vector<std::pair<ObjectPath, std::string> > removed_objects_;
  std::vector<std::string> updated_properties_;

  bool method_callback_called_;
};


TEST_F(ObjectManagerTest, InitialObject) {
  ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      ObjectPath("/org/chromium/TestObject"));
  EXPECT_TRUE(object_proxy != NULL);

  Properties* properties = static_cast<Properties*>(
      object_manager_->GetProperties(ObjectPath("/org/chromium/TestObject"),
                                     "org.chromium.TestInterface"));
  EXPECT_TRUE(properties != NULL);

  EXPECT_EQ("TestService", properties->name.value());
  EXPECT_EQ(10, properties->version.value());

  std::vector<std::string> methods = properties->methods.value();
  ASSERT_EQ(4U, methods.size());
  EXPECT_EQ("Echo", methods[0]);
  EXPECT_EQ("SlowEcho", methods[1]);
  EXPECT_EQ("AsyncEcho", methods[2]);
  EXPECT_EQ("BrokenMethod", methods[3]);

  std::vector<ObjectPath> objects = properties->objects.value();
  ASSERT_EQ(1U, objects.size());
  EXPECT_EQ(ObjectPath("/TestObjectPath"), objects[0]);
}

TEST_F(ObjectManagerTest, UnknownObjectProxy) {
  ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      ObjectPath("/org/chromium/UnknownObject"));
  EXPECT_TRUE(object_proxy == NULL);
}

TEST_F(ObjectManagerTest, UnknownObjectProperties) {
  Properties* properties = static_cast<Properties*>(
      object_manager_->GetProperties(ObjectPath("/org/chromium/UnknownObject"),
                                     "org.chromium.TestInterface"));
  EXPECT_TRUE(properties == NULL);
}

TEST_F(ObjectManagerTest, UnknownInterfaceProperties) {
  Properties* properties = static_cast<Properties*>(
      object_manager_->GetProperties(ObjectPath("/org/chromium/TestObject"),
                                     "org.chromium.UnknownService"));
  EXPECT_TRUE(properties == NULL);
}

TEST_F(ObjectManagerTest, GetObjects) {
  std::vector<ObjectPath> object_paths = object_manager_->GetObjects();
  ASSERT_EQ(1U, object_paths.size());
  EXPECT_EQ(ObjectPath("/org/chromium/TestObject"), object_paths[0]);
}

TEST_F(ObjectManagerTest, GetObjectsWithInterface) {
  std::vector<ObjectPath> object_paths =
      object_manager_->GetObjectsWithInterface("org.chromium.TestInterface");
  ASSERT_EQ(1U, object_paths.size());
  EXPECT_EQ(ObjectPath("/org/chromium/TestObject"), object_paths[0]);
}

TEST_F(ObjectManagerTest, GetObjectsWithUnknownInterface) {
  std::vector<ObjectPath> object_paths =
      object_manager_->GetObjectsWithInterface("org.chromium.UnknownService");
  EXPECT_EQ(0U, object_paths.size());
}

TEST_F(ObjectManagerTest, SameObject) {
  ObjectManager* object_manager = bus_->GetObjectManager(
      "org.chromium.TestService",
      ObjectPath("/org/chromium/TestService"));
  EXPECT_EQ(object_manager_, object_manager);
}

TEST_F(ObjectManagerTest, DifferentObjectForService) {
  ObjectManager* object_manager = bus_->GetObjectManager(
      "org.chromium.DifferentService",
      ObjectPath("/org/chromium/TestService"));
  EXPECT_NE(object_manager_, object_manager);
}

TEST_F(ObjectManagerTest, DifferentObjectForPath) {
  ObjectManager* object_manager = bus_->GetObjectManager(
      "org.chromium.TestService",
      ObjectPath("/org/chromium/DifferentService"));
  EXPECT_NE(object_manager_, object_manager);
}

TEST_F(ObjectManagerTest, SecondObject) {
  PerformAction("AddObject", ObjectPath("/org/chromium/SecondObject"));
  WaitForObject();

  ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      ObjectPath("/org/chromium/SecondObject"));
  EXPECT_TRUE(object_proxy != NULL);

  Properties* properties = static_cast<Properties*>(
      object_manager_->GetProperties(ObjectPath("/org/chromium/SecondObject"),
                                     "org.chromium.TestInterface"));
  EXPECT_TRUE(properties != NULL);

  std::vector<ObjectPath> object_paths = object_manager_->GetObjects();
  ASSERT_EQ(2U, object_paths.size());

  std::sort(object_paths.begin(), object_paths.end());
  EXPECT_EQ(ObjectPath("/org/chromium/SecondObject"), object_paths[0]);
  EXPECT_EQ(ObjectPath("/org/chromium/TestObject"), object_paths[1]);

  object_paths =
      object_manager_->GetObjectsWithInterface("org.chromium.TestInterface");
  ASSERT_EQ(2U, object_paths.size());

  std::sort(object_paths.begin(), object_paths.end());
  EXPECT_EQ(ObjectPath("/org/chromium/SecondObject"), object_paths[0]);
  EXPECT_EQ(ObjectPath("/org/chromium/TestObject"), object_paths[1]);
}

TEST_F(ObjectManagerTest, RemoveSecondObject) {
  PerformAction("AddObject", ObjectPath("/org/chromium/SecondObject"));
  WaitForObject();

  std::vector<ObjectPath> object_paths = object_manager_->GetObjects();
  ASSERT_EQ(2U, object_paths.size());

  PerformAction("RemoveObject", ObjectPath("/org/chromium/SecondObject"));
  WaitForRemoveObject();

  ObjectProxy* object_proxy = object_manager_->GetObjectProxy(
      ObjectPath("/org/chromium/SecondObject"));
  EXPECT_TRUE(object_proxy == NULL);

  Properties* properties = static_cast<Properties*>(
      object_manager_->GetProperties(ObjectPath("/org/chromium/SecondObject"),
                                     "org.chromium.TestInterface"));
  EXPECT_TRUE(properties == NULL);

  object_paths = object_manager_->GetObjects();
  ASSERT_EQ(1U, object_paths.size());
  EXPECT_EQ(ObjectPath("/org/chromium/TestObject"), object_paths[0]);

  object_paths =
      object_manager_->GetObjectsWithInterface("org.chromium.TestInterface");
  ASSERT_EQ(1U, object_paths.size());
  EXPECT_EQ(ObjectPath("/org/chromium/TestObject"), object_paths[0]);
}

TEST_F(ObjectManagerTest, OwnershipLost) {
  PerformAction("ReleaseOwnership", ObjectPath("/org/chromium/TestService"));
  WaitForRemoveObject();

  std::vector<ObjectPath> object_paths = object_manager_->GetObjects();
  ASSERT_EQ(0U, object_paths.size());
}

TEST_F(ObjectManagerTest, OwnershipLostAndRegained) {
  PerformAction("Ownership", ObjectPath("/org/chromium/TestService"));
  WaitForRemoveObject();
  WaitForObject();

  std::vector<ObjectPath> object_paths = object_manager_->GetObjects();
  ASSERT_EQ(1U, object_paths.size());
}

}  // namespace dbus
