// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPATCHER_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPATCHER_CLIENT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/dispatcher_client.h"

namespace views {

// TODO(erg): I won't lie to you; I have no idea what this is or what it does.
class VIEWS_EXPORT DesktopDispatcherClient
    : public aura::client::DispatcherClient {
 public:
  DesktopDispatcherClient();
  virtual ~DesktopDispatcherClient();

  virtual void PrepareNestedLoopClosures(
      base::MessagePumpDispatcher* dispatcher,
      base::Closure* run_closure,
      base::Closure* quit_closure) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopDispatcherClient);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPATCHER_CLIENT_H_
