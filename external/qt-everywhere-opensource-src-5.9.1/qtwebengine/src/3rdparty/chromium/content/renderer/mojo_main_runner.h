// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_MAIN_RUNNER_H_
#define CONTENT_RENDERER_MOJO_MAIN_RUNNER_H_

#include "base/macros.h"
#include "gin/runner.h"

namespace blink {
class WebFrame;
}

namespace content {

// Implementation of gin::Runner that forwards Runner functions to WebFrame.
class MojoMainRunner : public gin::Runner {
 public:
  // Does not take ownership of ContextHolder.
  MojoMainRunner(blink::WebFrame* frame, gin::ContextHolder* context_holder);
  ~MojoMainRunner() override;

  // Runner overrides:
  void Run(const std::string& source,
           const std::string& resource_name) override;
  v8::Local<v8::Value> Call(v8::Local<v8::Function> function,
                             v8::Local<v8::Value> receiver,
                             int argc,
                             v8::Local<v8::Value> argv[]) override;
  gin::ContextHolder* GetContextHolder() override;

 private:
  // Frame to execute script in.
  blink::WebFrame* frame_;

  // Created by blink bindings to V8.
  gin::ContextHolder* context_holder_;

  DISALLOW_COPY_AND_ASSIGN(MojoMainRunner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_MAIN_RUNNER_H_
