// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_EVENT_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_EVENT_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/event_client.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT DesktopEventClient : public aura::client::EventClient {
 public:
  DesktopEventClient();
  virtual ~DesktopEventClient();

  // Overridden from aura::client::EventClient:
  virtual bool CanProcessEventsWithinSubtree(
      const aura::Window* window) const OVERRIDE;
  virtual ui::EventTarget* GetToplevelEventTarget() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopEventClient);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_EVENT_CLIENT_H_
