// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_CACA_EVENT_FACTORY_H_
#define UI_OZONE_PLATFORM_CACA_CACA_EVENT_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/public/event_factory_ozone.h"

namespace ui {

class CacaConnection;

class CacaEventFactory : public EventFactoryOzone,
                         public PlatformEventSource {
 public:
  CacaEventFactory(CacaConnection* connection);
  virtual ~CacaEventFactory();

  // ui::EventFactoryOzone:
  virtual void WarpCursorTo(gfx::AcceleratedWidget widget,
                            const gfx::PointF& location) OVERRIDE;

 private:
  // PlatformEventSource:
  virtual void OnDispatcherListChanged() OVERRIDE;

  void ScheduleEventProcessing();

  void TryProcessingEvent();

  CacaConnection* connection_;  // Not owned.

  base::WeakPtrFactory<CacaEventFactory> weak_ptr_factory_;

  // Delay between event polls.
  base::TimeDelta delay_;

  // Keep track of last cursor position to dispatch correct mouse push/release
  // events.
  gfx::PointF last_cursor_location_;

  int modifier_flags_;

  DISALLOW_COPY_AND_ASSIGN(CacaEventFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_CACA_EVENT_FACTORY_H_
