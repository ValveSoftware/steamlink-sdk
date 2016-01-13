// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MHTML_GENERATOR_H_
#define CONTENT_RENDERER_MHTML_GENERATOR_H_

#include "base/files/file.h"
#include "content/public/renderer/render_view_observer.h"
#include "ipc/ipc_platform_file.h"

namespace content {
class RenderViewImpl;

class MHTMLGenerator : public RenderViewObserver {
 public:
  explicit MHTMLGenerator(RenderViewImpl* render_view);
  virtual ~MHTMLGenerator();

 private:
  // RenderViewObserver implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnSavePageAsMHTML(int job_id,
                         IPC::PlatformFileForTransit file_for_transit);

  void NotifyBrowser(int job_id, int64 data_size);
  // Returns the size of the generated MHTML, -1 if it failed.
  int64 GenerateMHTML();

  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MHTML_GENERATOR_H_
