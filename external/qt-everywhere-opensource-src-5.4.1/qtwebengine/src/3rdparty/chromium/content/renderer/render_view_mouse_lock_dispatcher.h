// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_MOUSE_LOCK_DISPATCHER_H_
#define CONTENT_RENDERER_RENDER_VIEW_MOUSE_LOCK_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/mouse_lock_dispatcher.h"

namespace content {
class RenderViewImpl;

// RenderViewMouseLockDispatcher is owned by RenderViewImpl.
class RenderViewMouseLockDispatcher : public MouseLockDispatcher,
                                      public RenderViewObserver {
 public:
  explicit RenderViewMouseLockDispatcher(RenderViewImpl* render_view_impl);
  virtual ~RenderViewMouseLockDispatcher();

 private:
  // MouseLockDispatcher implementation.
  virtual void SendLockMouseRequest(bool unlocked_by_target) OVERRIDE;
  virtual void SendUnlockMouseRequest() OVERRIDE;

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnLockMouseACK(bool succeeded);

  RenderViewImpl* render_view_impl_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewMouseLockDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_MOUSE_LOCK_DISPATCHER_H_
