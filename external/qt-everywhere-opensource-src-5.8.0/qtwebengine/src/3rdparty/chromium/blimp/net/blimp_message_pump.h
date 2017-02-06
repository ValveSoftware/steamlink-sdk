// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_PUMP_H_
#define BLIMP_NET_BLIMP_MESSAGE_PUMP_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_net_export.h"
#include "net/base/completion_callback.h"

namespace net {
class GrowableIOBuffer;
}

namespace blimp {

class BlimpMessageProcessor;
class ConnectionErrorObserver;
class PacketReader;

// Reads and deserializes incoming packets from |reader_|, and forwards parsed
// BlimpMessages to |processor_|. When |processor_| is ready to take the next
// message, the BlimpMessagePump reads the next packet.
class BLIMP_NET_EXPORT BlimpMessagePump {
 public:
  // Caller ensures that |reader| outlives this object.
  explicit BlimpMessagePump(PacketReader* reader);

  ~BlimpMessagePump();

  // Sets the processor which will take BlimpMessages.
  // Caller retains the ownership of |processor|.
  // The processor can be unset by passing a null pointer when no reads are
  // inflight, ie. after |processor_|->ProcessMessage() is called, but before
  // ProcessMessage() invokes its completion callback.
  void SetMessageProcessor(BlimpMessageProcessor* processor);

  void set_error_observer(ConnectionErrorObserver* observer) {
    error_observer_ = observer;
  }

 private:
  // Read next packet from |reader_|.
  void ReadNextPacket();

  // Callback when next packet is ready in |buffer_|.
  void OnReadPacketComplete(int result);

  // Callback when |processor_| finishes processing a BlimpMessage.
  // Any values other than net::OK indicate that |processor_| has encountered an
  // error that should be handled. Currently all errors will cause the
  // connection to be dropped; in the future we will need to add more
  // sophisticated error handling logic here.
  // TODO(kmarshall): Improve error handling.
  void OnProcessMessageComplete(int result);

  PacketReader* reader_;
  ConnectionErrorObserver* error_observer_ = nullptr;
  BlimpMessageProcessor* processor_ = nullptr;
  scoped_refptr<net::GrowableIOBuffer> buffer_;
  bool read_inflight_ = false;

  // Used to abandon ProcessMessage completion callbacks if |this| is deleted.
  base::WeakPtrFactory<BlimpMessagePump> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessagePump);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_PUMP_H_
