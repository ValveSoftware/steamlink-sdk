// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_RENDERER_FRAME_SCHEDULER_H_
#define BLIMP_ENGINE_RENDERER_FRAME_SCHEDULER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"

namespace blimp {
namespace engine {

class FrameSchedulerClient {
 public:
  virtual ~FrameSchedulerClient() {}

  // Used to notify the SchedulerClient to start the requested frame update.
  // Calling this method marks the completion of the frame update request made
  // to the scheduler. Any calls to schedule a frame after this will result in
  // a new frame update.
  // The frame update must be run synchronously, and if it results in any
  // content changes sent to the blimp client, the scheduler must be informed
  // using DidSendFrameUpdateToClient.
  virtual void StartFrameUpdate() = 0;
};

// Responsible for scheduling frame updates sent to the client.
// The Scheduler uses the state of frames sent to the client and the
// acknowledgements for these frames from the client to make decisions regarding
// when a frame should be produced on the engine.
// The caller provides a FrameSchedulerClient to be notified when a frame update
// should be produced. The FrameSchedulerClient must outlive this class.
class FrameScheduler {
 public:
  FrameScheduler(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 FrameSchedulerClient* client);
  virtual ~FrameScheduler();

  // Called when the |client| wants to start a frame update on the engine.
  // |client->StartFrameUpdate() will be called back asynchronously when the
  // scheduler is ready for the next frame to be started.
  void ScheduleFrameUpdate();

  // Called when a frame update is sent to the client. This must be called only
  // when the |client| is producing a frame update in
  // FrameSchedulerClient::StartFrameUpdate and must be followed with a
  // DidReceiveFrameUpdateAck when the frame sent is ack-ed by the client.
  void DidSendFrameUpdateToClient();

  // Called when an Ack is received for a frame sent to the client.
  void DidReceiveFrameUpdateAck();

  bool needs_frame_update() const { return needs_frame_update_; }

  void set_frame_delay_for_testing(base::TimeDelta frame_delay) {
    frame_delay_ = frame_delay;
  }

 private:
  void ScheduleFrameUpdateIfNecessary();

  // Returns true if a frame update can be started. The Scheduler can not
  // produce new frames either if there is no request for frames pending or we
  // are waiting for an ack for a frame previously sent to the client.
  bool ShouldProduceFrameUpdates() const;

  void StartFrameUpdate();

  // Set to true if the |client_| has requested us to schedule frames.
  bool needs_frame_update_ = false;

  // Set to true if a frame update was sent to the client and the ack is
  // pending.
  bool frame_ack_pending_ = false;

  bool in_frame_update_ = false;

  // The time at which the next main frame update can be run.
  base::TimeTicks next_frame_time_;

  // The delay to use between consecutive frames.
  base::TimeDelta frame_delay_;

  base::OneShotTimer frame_tick_timer_;

  FrameSchedulerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(FrameScheduler);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_RENDERER_FRAME_SCHEDULER_H_
