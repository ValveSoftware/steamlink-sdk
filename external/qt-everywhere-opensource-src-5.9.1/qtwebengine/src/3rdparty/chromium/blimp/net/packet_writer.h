// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_PACKET_WRITER_H_
#define BLIMP_NET_PACKET_WRITER_H_

#include <string>

#include "base/macros.h"
#include "blimp/net/blimp_net_export.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace blimp {

// Interface to describe a writer which can write variable-length data packets
// to a connection.
class BLIMP_NET_EXPORT PacketWriter {
 public:
  virtual ~PacketWriter() {}

  // Invokes |cb| with net::OK if the write operation executed successfully.
  // All other values indicate unrecoverable errors.
  // |callback| must not be invoked if |this| is deleted.
  virtual void WritePacket(const scoped_refptr<net::DrainableIOBuffer>& data,
                           const net::CompletionCallback& callback) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_PACKET_WRITER_H_
