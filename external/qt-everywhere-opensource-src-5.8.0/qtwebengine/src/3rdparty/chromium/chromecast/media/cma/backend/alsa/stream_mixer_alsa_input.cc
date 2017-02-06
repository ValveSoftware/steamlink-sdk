// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa_input.h"

#include <utility>

#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa_input_impl.h"

namespace chromecast {
namespace media {

StreamMixerAlsaInput::StreamMixerAlsaInput(Delegate* delegate,
                                           int samples_per_second,
                                           bool primary) {
  std::unique_ptr<StreamMixerAlsaInputImpl> impl(new StreamMixerAlsaInputImpl(
      delegate, samples_per_second, primary, StreamMixerAlsa::Get()));
  impl_ = impl.get();  // Store a pointer to the impl, but the mixer owns it.
  StreamMixerAlsa::Get()->AddInput(std::move(impl));
}

StreamMixerAlsaInput::~StreamMixerAlsaInput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->PreventDelegateCalls();
  StreamMixerAlsa::Get()->RemoveInput(impl_);
}

void StreamMixerAlsaInput::WritePcm(
    const scoped_refptr<DecoderBufferBase>& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->WritePcm(data);
}

void StreamMixerAlsaInput::SetPaused(bool paused) {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->SetPaused(paused);
}

void StreamMixerAlsaInput::SetVolumeMultiplier(float multiplier) {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->SetVolumeMultiplier(multiplier);
}

}  // namespace media
}  // namespace chromecast
