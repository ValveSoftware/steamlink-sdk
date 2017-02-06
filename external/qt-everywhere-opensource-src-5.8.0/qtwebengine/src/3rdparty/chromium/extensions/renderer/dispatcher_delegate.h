// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
#define EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_

#include <set>
#include <string>

namespace blink {
class WebFrame;
}

namespace extensions {
class Dispatcher;
class Extension;
class ModuleSystem;
class ResourceBundleSourceMap;
class ScriptContext;
class URLPatternSet;

// Base class and default implementation for an extensions::Dispacher delegate.
// DispatcherDelegate can be used to override and extend the behavior of the
// extensions system's renderer side.
class DispatcherDelegate {
 public:
  virtual ~DispatcherDelegate() {}

  // Initializes origin permissions for a newly created extension context.
  virtual void InitOriginPermissions(const Extension* extension,
                                     bool is_extension_active) {}

  // Includes additional native handlers in a ScriptContext's ModuleSystem.
  virtual void RegisterNativeHandlers(Dispatcher* dispatcher,
                                      ModuleSystem* module_system,
                                      ScriptContext* context) {}

  // Includes additional source resources into the resource map.
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) {}

  // Requires additional modules within an extension context's module system.
  virtual void RequireAdditionalModules(ScriptContext* context,
                                        bool is_within_platform_app) {}

  // Allows the delegate to respond to an updated set of active extensions in
  // the Dispatcher.
  virtual void OnActiveExtensionsUpdated(
      const std::set<std::string>& extension_ids) {}

  // Sets the current Chrome channel.
  // TODO(rockot): This doesn't belong in a generic extensions system interface.
  // See http://crbug.com/368431.
  virtual void SetChannel(int channel) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
