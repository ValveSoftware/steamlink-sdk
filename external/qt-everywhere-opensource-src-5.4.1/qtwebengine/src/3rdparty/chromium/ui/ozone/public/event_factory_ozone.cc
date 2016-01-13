// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/event_factory_ozone.h"

#include "base/logging.h"

namespace ui {

// static
EventFactoryOzone* EventFactoryOzone::impl_ = NULL;

EventFactoryOzone::EventFactoryOzone() {
  CHECK(!impl_) << "There should only be a single EventFactoryOzone";
  impl_ = this;
}

EventFactoryOzone::~EventFactoryOzone() {
  CHECK_EQ(impl_, this);
  impl_ = NULL;
}

EventFactoryOzone* EventFactoryOzone::GetInstance() {
  CHECK(impl_) << "No EventFactoryOzone implementation set.";
  return impl_;
}

void EventFactoryOzone::WarpCursorTo(gfx::AcceleratedWidget widget,
                                     const gfx::PointF& location) {
  NOTIMPLEMENTED();
}

}  // namespace ui
