// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IN_PROCESS_RENDERER_THREAD_H_
#define CONTENT_RENDERER_IN_PROCESS_RENDERER_THREAD_H_

#include <string>

#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {
class RenderProcess;

// This class creates the IO thread for the renderer when running in
// single-process mode.  It's not used in multi-process mode.
class InProcessRendererThread : public base::Thread {
 public:
  explicit InProcessRendererThread(const std::string& channel_id);
  virtual ~InProcessRendererThread();

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  std::string channel_id_;
  scoped_ptr<RenderProcess> render_process_;

  DISALLOW_COPY_AND_ASSIGN(InProcessRendererThread);
};

CONTENT_EXPORT base::Thread* CreateInProcessRendererThread(
    const std::string& channel_id);

}  // namespace content

#endif  // CONTENT_RENDERER_IN_PROCESS_RENDERER_THREAD_H_
