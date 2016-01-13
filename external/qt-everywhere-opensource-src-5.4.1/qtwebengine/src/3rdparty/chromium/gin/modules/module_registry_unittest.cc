// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/module_registry.h"

#include "base/message_loop/message_loop.h"
#include "gin/modules/module_registry_observer.h"
#include "gin/modules/module_runner_delegate.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8.h"

namespace gin {

namespace {

struct TestHelper {
  TestHelper(v8::Isolate* isolate)
      : delegate(std::vector<base::FilePath>()),
        runner(new ShellRunner(&delegate, isolate)),
        scope(runner.get()) {
  }

  base::MessageLoop message_loop;
  ModuleRunnerDelegate delegate;
  scoped_ptr<ShellRunner> runner;
  Runner::Scope scope;
};

class ModuleRegistryObserverImpl : public ModuleRegistryObserver {
 public:
  ModuleRegistryObserverImpl() : did_add_count_(0) {}

  virtual void OnDidAddPendingModule(
      const std::string& id,
      const std::vector<std::string>& dependencies) OVERRIDE {
    did_add_count_++;
    id_ = id;
    dependencies_ = dependencies;
  }

  int did_add_count() { return did_add_count_; }
  const std::string& id() const { return id_; }
  const std::vector<std::string>& dependencies() const { return dependencies_; }

 private:
  int did_add_count_;
  std::string id_;
  std::vector<std::string> dependencies_;

  DISALLOW_COPY_AND_ASSIGN(ModuleRegistryObserverImpl);
};

}  // namespace

typedef V8Test ModuleRegistryTest;

// Verifies ModuleRegistry is not available after ContextHolder has been
// deleted.
TEST_F(ModuleRegistryTest, DestroyedWithContext) {
  v8::Isolate::Scope isolate_scope(instance_->isolate());
  v8::HandleScope handle_scope(instance_->isolate());
  v8::Handle<v8::Context> context = v8::Context::New(
      instance_->isolate(), NULL, v8::Handle<v8::ObjectTemplate>());
  {
    ContextHolder context_holder(instance_->isolate());
    context_holder.SetContext(context);
    ModuleRegistry* registry = ModuleRegistry::From(context);
    EXPECT_TRUE(registry != NULL);
  }
  ModuleRegistry* registry = ModuleRegistry::From(context);
  EXPECT_TRUE(registry == NULL);
}

// Verifies ModuleRegistryObserver is notified appropriately.
TEST_F(ModuleRegistryTest, ModuleRegistryObserverTest) {
  TestHelper helper(instance_->isolate());
  std::string source =
     "define('id', ['dep1', 'dep2'], function() {"
     "  return function() {};"
     "});";

  ModuleRegistryObserverImpl observer;
  ModuleRegistry::From(helper.runner->GetContextHolder()->context())->
      AddObserver(&observer);
  helper.runner->Run(source, "script");
  ModuleRegistry::From(helper.runner->GetContextHolder()->context())->
      RemoveObserver(&observer);
  EXPECT_EQ(1, observer.did_add_count());
  EXPECT_EQ("id", observer.id());
  ASSERT_EQ(2u, observer.dependencies().size());
  EXPECT_EQ("dep1", observer.dependencies()[0]);
  EXPECT_EQ("dep2", observer.dependencies()[1]);
}

}  // namespace gin
