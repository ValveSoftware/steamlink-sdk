// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_STREAM_PROVIDER_H_
#define MEDIA_BASE_DEMUXER_STREAM_PROVIDER_H_

#include "base/macros.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "url/gurl.h"

namespace media {

// Abstract class that defines how to retrieve "media sources" in DemuxerStream
// form (for most cases) or URL form (for the MediaPlayerRenderer case).
//
// The sub-classes do not stricly provide demuxer streams, but because all
// sub-classes are for the moment Demuxers, this class has not been renamed to
// "MediaProvider". This class would be a good candidate for renaming, if
// ever Pipeline were to support this class directly, instead of the Demuxer
// interface.
//
// The derived classes must return a non-null value for the getter method
// associated with their type, and return a null/empty value for other getters.
class MEDIA_EXPORT DemuxerStreamProvider {
 public:
  enum Type {
    STREAM,  // Indicates GetStream() should be used
    URL,     // Indicates GetUrl() should be used
  };

  DemuxerStreamProvider();
  virtual ~DemuxerStreamProvider();

  // For Type::STREAM:
  //   Returns the first stream of the given stream type (which is not allowed
  //   to be DemuxerStream::TEXT), or NULL if that type of stream is not
  //   present.
  // Other types:
  //   Should not be called.
  virtual DemuxerStream* GetStream(DemuxerStream::Type type) = 0;

  // For Type::URL:
  //   Returns the URL of the media to play. This might be an empty URL, and
  //   should be handled appropriately by the caller.
  // Other types:
  //   Should not be called.
  virtual GURL GetUrl() const;

  virtual DemuxerStreamProvider::Type GetType() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DemuxerStreamProvider);
};

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_STREAM_PROVIDER_H_
