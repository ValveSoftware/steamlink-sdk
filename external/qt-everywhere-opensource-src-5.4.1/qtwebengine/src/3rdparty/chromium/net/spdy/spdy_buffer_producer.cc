// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_buffer_producer.h"

#include "base/logging.h"
#include "net/spdy/spdy_buffer.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

SpdyBufferProducer::SpdyBufferProducer() {}

SpdyBufferProducer::~SpdyBufferProducer() {}

SimpleBufferProducer::SimpleBufferProducer(scoped_ptr<SpdyBuffer> buffer)
    : buffer_(buffer.Pass()) {}

SimpleBufferProducer::~SimpleBufferProducer() {}

scoped_ptr<SpdyBuffer> SimpleBufferProducer::ProduceBuffer() {
  DCHECK(buffer_);
  return buffer_.Pass();
}

}  // namespace net
