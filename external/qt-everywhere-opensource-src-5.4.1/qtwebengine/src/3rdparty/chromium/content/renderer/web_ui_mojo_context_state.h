// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_MOJO_CONTEXT_STATE_H_
#define CONTENT_RENDERER_WEB_UI_MOJO_CONTEXT_STATE_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "gin/modules/module_registry_observer.h"
#include "mojo/public/cpp/system/core.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
class WebURLResponse;
}

namespace gin {
class ContextHolder;
struct PendingModule;
}

namespace content {

class ResourceFetcher;
class WebUIRunner;

// WebUIMojoContextState manages the modules needed for mojo bindings. It does
// this by way of gin. Non-builtin modules are downloaded by way of
// ResourceFetchers.
class WebUIMojoContextState : public gin::ModuleRegistryObserver {
 public:
  WebUIMojoContextState(blink::WebFrame* frame,
                        v8::Handle<v8::Context> context);
  virtual ~WebUIMojoContextState();

  // Called once the mojo::Handle is available.
  void SetHandle(mojo::ScopedMessagePipeHandle handle);

  // Returns true if at least one module was added.
  bool module_added() const { return module_added_; }

 private:
  class Loader;

  // Invokes FetchModule() for any modules that have not already been
  // downloaded.
  void FetchModules(const std::vector<std::string>& ids);

  // Creates a ResourceFetcher to download |module|.
  void FetchModule(const std::string& module);

  // Callback once a module has finished downloading. Passes data to |runner_|.
  void OnFetchModuleComplete(ResourceFetcher* fetcher,
                             const blink::WebURLResponse& response,
                             const std::string& data);

  // gin::ModuleRegistryObserver overrides:
  virtual void OnDidAddPendingModule(
      const std::string& id,
      const std::vector<std::string>& dependencies) OVERRIDE;

  // Frame script is executed in. Also used to download resources.
  blink::WebFrame* frame_;

  // See description above getter.
  bool module_added_;

  // Executes the script from gin.
  scoped_ptr<WebUIRunner> runner_;

  // Set of fetchers we're waiting on to download script.
  ScopedVector<ResourceFetcher> module_fetchers_;

  // Set of modules we've fetched script from.
  std::set<std::string> fetched_modules_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMojoContextState);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_MOJO_CONTEXT_STATE_H_
