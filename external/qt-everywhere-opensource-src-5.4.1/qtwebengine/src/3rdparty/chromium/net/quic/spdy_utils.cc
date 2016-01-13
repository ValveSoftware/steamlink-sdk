// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/spdy_utils.h"

#include "base/memory/scoped_ptr.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

using std::string;

namespace net {

// static
string SpdyUtils::SerializeUncompressedHeaders(const SpdyHeaderBlock& headers) {
  int length = SpdyFramer::GetSerializedLength(SPDY3, &headers);
  SpdyFrameBuilder builder(length, SPDY3);
  SpdyFramer::WriteHeaderBlock(&builder, SPDY3, &headers);
  scoped_ptr<SpdyFrame> block(builder.take());
  return string(block->data(), length);
}

}  // namespace net
