// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BINDINGS_CONTROLLER_H_
#define CONTENT_RENDERER_MOJO_BINDINGS_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/system/core.h"

namespace gin {
class PerContextData;
}

namespace content {

class MojoContextState;

// MojoBindingsController is responsible for enabling the renderer side of mojo
// bindings. It creates (and destroys) a MojoContextState at the appropriate
// times and handles the necessary browser messages. MojoBindingsController
// destroys itself when the RendererFrame it is created with is destroyed.
class MojoBindingsController
    : public RenderFrameObserver,
      public RenderFrameObserverTracker<MojoBindingsController> {
 public:
  MojoBindingsController(RenderFrame* render_frame, bool for_layout_tests);
  void RunScriptsAtDocumentStart();
  void RunScriptsAtDocumentReady();

 private:
  ~MojoBindingsController() override;

  void CreateContextState();
  void DestroyContextState(v8::Local<v8::Context> context);
  MojoContextState* GetContextState();

  // RenderFrameObserver overrides:
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int world_id) override;
  void DidClearWindowObject() override;
  void OnDestruct() override;

  const bool for_layout_tests_;

  DISALLOW_COPY_AND_ASSIGN(MojoBindingsController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BINDINGS_CONTROLLER_H_
