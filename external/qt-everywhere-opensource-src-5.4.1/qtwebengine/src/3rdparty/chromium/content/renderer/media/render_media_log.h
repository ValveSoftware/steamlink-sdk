// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_

#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/base/media_log.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// RenderMediaLog is an implementation of MediaLog that forwards events to the
// browser process, throttling as necessary.
//
// To minimize the number of events sent over the wire, only the latest event
// added is sent for high frequency events (e.g., BUFFERED_EXTENTS_CHANGED).
class CONTENT_EXPORT RenderMediaLog : public media::MediaLog {
 public:
  RenderMediaLog();

  // MediaLog implementation.
  virtual void AddEvent(scoped_ptr<media::MediaLogEvent> event) OVERRIDE;

  // Will reset |last_ipc_send_time_| with the value of NowTicks().
  void SetTickClockForTesting(scoped_ptr<base::TickClock> tick_clock);

 private:
  virtual ~RenderMediaLog();

  scoped_refptr<base::MessageLoopProxy> render_loop_;
  scoped_ptr<base::TickClock> tick_clock_;
  base::TimeTicks last_ipc_send_time_;
  std::vector<media::MediaLogEvent> queued_media_events_;

  // Limits the number buffered extents changed events we send over IPC to one.
  scoped_ptr<media::MediaLogEvent> last_buffered_extents_changed_event_;

  DISALLOW_COPY_AND_ASSIGN(RenderMediaLog);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
