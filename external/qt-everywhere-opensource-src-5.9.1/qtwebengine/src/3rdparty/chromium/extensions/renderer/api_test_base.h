// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_TEST_BASE_H_
#define EXTENSIONS_RENDERER_API_TEST_BASE_H_

#include <map>
#include <string>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "extensions/renderer/module_system_test.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "gin/handle.h"
#include "gin/modules/module_registry.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/core.h"

namespace extensions {

class V8SchemaRegistry;

// An InterfaceProvider that provides access from JS modules to interfaces
// registered by AddInterface() calls.
class TestInterfaceProvider : public gin::Wrappable<TestInterfaceProvider> {
 public:
  static gin::Handle<TestInterfaceProvider> Create(v8::Isolate* isolate);
  ~TestInterfaceProvider() override;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  template <typename Interface>
  void AddInterface(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>
          factory_callback) {
    factories_.insert(std::make_pair(
        Interface::Name_,
        base::Bind(ForwardToInterfaceFactory<Interface>, factory_callback)));
  }

  // Ignore requests for Interface.
  template <typename Interface>
  void IgnoreInterfaceRequests() {
    factories_.insert(std::make_pair(
        Interface::Name_, base::Bind(&TestInterfaceProvider::IgnoreHandle)));
  }

  static gin::WrapperInfo kWrapperInfo;

 private:
   TestInterfaceProvider();

  mojo::Handle GetInterface(const std::string& interface_name);

  template <typename Interface>
  static void ForwardToInterfaceFactory(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>
          factory_callback,
      mojo::ScopedMessagePipeHandle handle) {
    factory_callback.Run(mojo::MakeRequest<Interface>(std::move(handle)));
  }

  static void IgnoreHandle(mojo::ScopedMessagePipeHandle handle);

  std::map<std::string, base::Callback<void(mojo::ScopedMessagePipeHandle)> >
      factories_;
};

// An environment for unit testing apps/extensions API custom bindings
// implemented on Mojo interfaces. This augments a ModuleSystemTestEnvironment
// with a TestInterfaceProvider and other modules available in a real extensions
// environment.
class ApiTestEnvironment {
 public:
  explicit ApiTestEnvironment(ModuleSystemTestEnvironment* environment);
  ~ApiTestEnvironment();
  void RunTest(const std::string& file_name, const std::string& test_name);
  TestInterfaceProvider* interface_provider() { return interface_provider_; }
  ModuleSystemTestEnvironment* env() { return env_; }

 private:
  void RegisterModules();
  void InitializeEnvironment();
  void RunTestInner(const std::string& test_name,
                    const base::Closure& quit_closure);
  void RunPromisesAgain();

  ModuleSystemTestEnvironment* env_;
  TestInterfaceProvider* interface_provider_;
  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;
};

// A base class for unit testing apps/extensions API custom bindings implemented
// on Mojo interfaces. To use:
// 1. Register test Mojo interface implementations on interface_provider().
// 2. Write JS tests in extensions/test/data/test_file.js.
// 3. Write one C++ test function for each JS test containing
//    RunTest("test_file.js", "testFunctionName").
// See extensions/renderer/api_test_base_unittest.cc and
// extensions/test/data/api_test_base_unittest.js for sample usage.
class ApiTestBase : public ModuleSystemTest {
 protected:
  ApiTestBase();
  ~ApiTestBase() override;
  void SetUp() override;
  void RunTest(const std::string& file_name, const std::string& test_name);

  ApiTestEnvironment* api_test_env() { return test_env_.get(); }
  TestInterfaceProvider* interface_provider() {
    return test_env_->interface_provider();
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<ApiTestEnvironment> test_env_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_TEST_BASE_H_
