// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_DEMUXER_STREAM_PROVIDER_SHIM_H_
#define MEDIA_MOJO_SERVICES_DEMUXER_STREAM_PROVIDER_SHIM_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream_provider.h"
#include "media/mojo/services/mojo_demuxer_stream_adapter.h"

namespace media {

// DemuxerStreamProvider shim for mojom::DemuxerStreams.
class DemuxerStreamProviderShim : public DemuxerStreamProvider {
 public:
  // Constructs the shim; at least a single audio or video stream must be
  // provided.  |demuxer_ready_cb| will be called once the streams have been
  // initialized.  Calling any method before then is an error.
  DemuxerStreamProviderShim(mojom::DemuxerStreamPtr audio,
                            mojom::DemuxerStreamPtr video,
                            const base::Closure& demuxer_ready_cb);
  ~DemuxerStreamProviderShim() override;

  // DemuxerStreamProvider interface.
  DemuxerStream* GetStream(DemuxerStream::Type type) override;

 private:
  // Called as each mojom::DemuxerStream becomes ready.  Once all streams
  // are ready it will fire the |demuxer_ready_cb_| provided during
  // construction.
  void OnStreamReady();

  // Stored copy the ready callback provided during construction; cleared once
  // all streams are ready.
  base::Closure demuxer_ready_cb_;

  // Scoped container for demuxer stream adapters which interface with the
  // mojo level demuxer streams.  |streams_ready_| tracks how many streams are
  // ready and is used by OnStreamReady() to know when |demuxer_ready_cb_|
  // should be fired.
  ScopedVector<MojoDemuxerStreamAdapter> streams_;
  size_t streams_ready_;

  // WeakPtrFactorys must always be the last member variable.
  base::WeakPtrFactory<DemuxerStreamProviderShim> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemuxerStreamProviderShim);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_DEMUXER_STREAM_PROVIDER_SHIM_H_
