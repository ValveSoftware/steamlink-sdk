// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_MODULES_MODULE_RUNNER_DELEGATE_H_
#define GIN_MODULES_MODULE_RUNNER_DELEGATE_H_

#include <map>

#include "base/compiler_specific.h"
#include "gin/gin_export.h"
#include "gin/modules/file_module_provider.h"
#include "gin/shell_runner.h"

namespace gin {

typedef v8::Local<v8::Value> (*ModuleGetter)(v8::Isolate* isolate);

// Emebedders that use AMD modules will probably want to use a RunnerDelegate
// that inherits from ModuleRunnerDelegate. ModuleRunnerDelegate lets embedders
// register built-in modules and routes module requests to FileModuleProvider.
class GIN_EXPORT ModuleRunnerDelegate : public ShellRunnerDelegate {
 public:
  explicit ModuleRunnerDelegate(
      const std::vector<base::FilePath>& search_paths);
  virtual ~ModuleRunnerDelegate();

  void AddBuiltinModule(const std::string& id, ModuleGetter getter);

 protected:
  void AttemptToLoadMoreModules(Runner* runner);

 private:
  typedef std::map<std::string, ModuleGetter> BuiltinModuleMap;

  // From ShellRunnerDelegate:
  virtual v8::Handle<v8::ObjectTemplate> GetGlobalTemplate(
      ShellRunner* runner,
      v8::Isolate* isolate) OVERRIDE;
  virtual void DidCreateContext(ShellRunner* runner) OVERRIDE;
  virtual void DidRunScript(ShellRunner* runner) OVERRIDE;

  BuiltinModuleMap builtin_modules_;
  FileModuleProvider module_provider_;

  DISALLOW_COPY_AND_ASSIGN(ModuleRunnerDelegate);
};

}  // namespace gin

#endif  // GIN_MODULES_MODULE_RUNNER_DELEGATE_H_
