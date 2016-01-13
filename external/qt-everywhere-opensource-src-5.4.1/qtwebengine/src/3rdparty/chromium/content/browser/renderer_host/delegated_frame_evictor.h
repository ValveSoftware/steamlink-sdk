// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_EVICTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_EVICTOR_H_

#include "content/browser/renderer_host/renderer_frame_manager.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT DelegatedFrameEvictorClient {
 public:
  virtual ~DelegatedFrameEvictorClient() {}
  virtual void EvictDelegatedFrame() = 0;
};

class CONTENT_EXPORT DelegatedFrameEvictor : public RendererFrameManagerClient {
 public:
  // |client| must outlive |this|.
  explicit DelegatedFrameEvictor(DelegatedFrameEvictorClient* client);
  virtual ~DelegatedFrameEvictor();

  void SwappedFrame(bool visible);
  void DiscardedFrame();
  void SetVisible(bool visible);
  void LockFrame();
  void UnlockFrame();
  bool HasFrame() { return has_frame_; }

 private:
  // RendererFrameManagerClient implementation.
  virtual void EvictCurrentFrame() OVERRIDE;

  DelegatedFrameEvictorClient* client_;
  bool has_frame_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameEvictor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_EVICTOR_H_
