// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_PIPELINE_CONTROLLER_H_
#define MEDIA_FILTERS_PIPELINE_CONTROLLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/pipeline.h"
#include "media/base/renderer.h"

namespace media {

class Demuxer;

// PipelineController wraps a Pipeline to expose the one-at-a-time operations
// (Seek(), Suspend(), and Resume()) with a simpler API. Internally it tracks
// pending operations and dispatches them when possible. Duplicate requests
// (such as seeking twice to the same time) may be elided.
//
// TODO(sandersd):
//   - Expose an operation that restarts via suspend+resume.
//   - Block invalid calls after an error occurs.
class MEDIA_EXPORT PipelineController {
 public:
  enum class State {
    CREATED,
    STARTING,
    PLAYING,
    SEEKING,
    SUSPENDING,
    SUSPENDED,
    RESUMING,
  };

  using RendererFactoryCB = base::Callback<std::unique_ptr<Renderer>(void)>;
  using SeekedCB = base::Callback<void(bool time_updated)>;
  using SuspendedCB = base::Callback<void()>;

  // Construct a PipelineController wrapping |pipeline_|. |pipeline_| must
  // outlive the resulting PipelineController. The callbacks are:
  //   - |renderer_factory_cb| is called by PipelineController to create new
  //     renderers when starting and resuming.
  //   - |seeked_cb| is called upon reaching a stable state if a seek occured.
  //   - |suspended_cb| is called immediately after suspendeding.
  //   - |error_cb| is called if any operation on |pipeline_| does not result
  //     in PIPELINE_OK or its error callback is called.
  PipelineController(Pipeline* pipeline,
                     const RendererFactoryCB& renderer_factory_cb,
                     const SeekedCB& seeked_cb,
                     const SuspendedCB& suspended_cb,
                     const PipelineStatusCB& error_cb);
  ~PipelineController();

  // Start |pipeline_|. |demuxer| will be retained and StartWaitingForSeek()/
  // CancelPendingSeek() will be issued to it as necessary.
  //
  // When |is_streaming| is true, Resume() will always start at the
  // beginning of the stream, rather than attempting to seek to the current
  // time.
  //
  // When |is_static| is true, seeks to the current time may be elided.
  // Otherwise it is assumed that the media data may have changed.
  //
  // The remaining parameters are just passed directly to pipeline_.Start().
  void Start(Demuxer* demuxer,
             Pipeline::Client* client,
             bool is_streaming,
             bool is_static);

  // Request a seek to |time|. If |time_updated| is true, then the eventual
  // |seeked_cb| callback will also have |time_updated| set to true; it
  // indicates that the seek was requested by Blink and a time update is
  // expected so that Blink can fire the seeked event.
  void Seek(base::TimeDelta time, bool time_updated);

  // Request that |pipeline_| be suspended. This is a no-op if |pipeline_| has
  // been suspended.
  void Suspend();

  // Request that |pipeline_| be resumed. This is a no-op if |pipeline_| has not
  // been suspended.
  void Resume();

  // Returns true if the current state is stable. This means that |state_| is
  // PLAYING and there are no pending operations. Requests are processed
  // immediately when the state is stable, otherwise they are queued.
  //
  // Exceptions to the above:
  //   - Start() is processed immediately while in the CREATED state.
  //   - Resume() is processed immediately while in the SUSPENDED state.
  bool IsStable();

  // Returns true if the current target state is suspended.
  bool IsSuspended();

  // Returns true if |pipeline_| is suspended.
  bool IsPipelineSuspended();

 private:
  // Attempts to make progress from the current state to the target state.
  void Dispatch();

  // PipelineStaus callback that also carries the target state.
  void OnPipelineStatus(State state, PipelineStatus pipeline_status);

  // The Pipeline we are managing state for.
  Pipeline* pipeline_ = nullptr;

  // Factory for Renderers, used for Start() and Resume().
  RendererFactoryCB renderer_factory_cb_;

  // Called after seeks (which includes Start()) upon reaching a stable state.
  // Multiple seeks result in only one callback if no stable state occurs
  // between them.
  SeekedCB seeked_cb_;

  // Called immediately when |pipeline_| completes a suspend operation.
  SuspendedCB suspended_cb_;

  // Called immediately when any operation on |pipeline_| results in an error.
  PipelineStatusCB error_cb_;

  // State for handling StartWaitingForSeek()/CancelPendingSeek().
  Demuxer* demuxer_ = nullptr;
  bool waiting_for_seek_ = false;

  // When true, Resume() will start at time zero instead of seeking to the
  // current time.
  bool is_streaming_ = false;

  // When true, seeking to the current time may be elided.
  bool is_static_ = true;

  // Tracks the current state of |pipeline_|.
  State state_ = State::CREATED;

  // Indicates that a seek has occurred. When set, a seeked callback will be
  // issued at the next stable state.
  bool pending_seeked_cb_ = false;

  // Indicates that time has been changed by a seek, which will be reported at
  // the next seeked callback.
  bool pending_time_updated_ = false;

  // The target time of the active seek; valid while SEEKING or RESUMING.
  base::TimeDelta seek_time_;

  // Target state which we will work to achieve. |pending_seek_time_| is only
  // valid when |pending_seek_| is true.
  bool pending_seek_ = false;
  base::TimeDelta pending_seek_time_;
  bool pending_suspend_ = false;
  bool pending_resume_ = false;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<PipelineController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PipelineController);
};

}  // namespace media

#endif  // MEDIA_FILTERS_PIPELINE_CONTROLLER_H_
