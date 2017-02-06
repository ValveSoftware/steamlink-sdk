// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_resource_tracker.h"

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/public/cast_media_shlib.h"

namespace chromecast {
namespace media {

MediaResourceTracker::MediaResourceTracker(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner)
    : media_use_count_(0),
      media_lib_initialized_(false),
      delete_on_finalize_(false),
      ui_task_runner_(ui_task_runner),
      media_task_runner_(media_task_runner) {
  DCHECK(ui_task_runner);
  DCHECK(media_task_runner);
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
}

MediaResourceTracker::~MediaResourceTracker() {}

void MediaResourceTracker::InitializeMediaLib() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MediaResourceTracker::CallInitializeOnMediaThread,
                            base::Unretained(this)));
}

void MediaResourceTracker::FinalizeMediaLib(
    const base::Closure& completion_cb) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!completion_cb.is_null());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MediaResourceTracker::MaybeCallFinalizeOnMediaThread,
                 base::Unretained(this), completion_cb));
}

void MediaResourceTracker::FinalizeAndDestroy() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &MediaResourceTracker::MaybeCallFinalizeOnMediaThreadAndDeleteSelf,
          base::Unretained(this)));
}

void MediaResourceTracker::IncrementUsageCount() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(media_lib_initialized_);
  DCHECK(finalize_completion_cb_.is_null());
  media_use_count_++;
}

void MediaResourceTracker::DecrementUsageCount() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  media_use_count_--;

  if (media_use_count_ == 0 &&
      (delete_on_finalize_ || !finalize_completion_cb_.is_null())) {
      CallFinalizeOnMediaThread();
  }
}

void MediaResourceTracker::CallInitializeOnMediaThread() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (media_lib_initialized_)
    return;

  DoInitializeMediaLib();
  media_lib_initialized_ = true;
}

void MediaResourceTracker::MaybeCallFinalizeOnMediaThread(
    const base::Closure& completion_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(finalize_completion_cb_.is_null());

  finalize_completion_cb_ = BindToTaskRunner(ui_task_runner_, completion_cb);
  if (!media_lib_initialized_) {
    if (!finalize_completion_cb_.is_null())
      base::ResetAndReturn(&finalize_completion_cb_).Run();
    return;
  }

  // If there are things using media, we must wait for them to stop.
  // CallFinalize will get called later from DecrementUsageCount when
  // usage count drops to 0.
  if (media_use_count_ == 0)
    CallFinalizeOnMediaThread();
}

void MediaResourceTracker::MaybeCallFinalizeOnMediaThreadAndDeleteSelf() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (!media_lib_initialized_) {
    ui_task_runner_->DeleteSoon(FROM_HERE, this);
    return;
  }

  delete_on_finalize_ = true;

  // If there are things using media, we must wait for them to stop.
  // CallFinalize will get called later from DecrementUsageCount when
  // usage count drops to 0.
  if (media_use_count_ == 0)
    CallFinalizeOnMediaThread();
}

void MediaResourceTracker::CallFinalizeOnMediaThread() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(media_use_count_, 0ul);
  DCHECK(media_lib_initialized_);

  DoFinalizeMediaLib();
  media_lib_initialized_ = false;

  if (!finalize_completion_cb_.is_null())
    base::ResetAndReturn(&finalize_completion_cb_).Run();

  if (delete_on_finalize_)
    ui_task_runner_->DeleteSoon(FROM_HERE, this);
}

void MediaResourceTracker::DoInitializeMediaLib() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  media::CastMediaShlib::Initialize(cmd_line->argv());
}

void MediaResourceTracker::DoFinalizeMediaLib() {
  CastMediaShlib::Finalize();
}

}  // namespace media
}  // namespace chromecast
