// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_control_message.h"

#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/node_controller.h"
#include "mojo/edk/system/ports_message.h"

namespace mojo {
namespace edk {

void SendDataPipeControlMessage(NodeController* node_controller,
                                const ports::PortRef& port,
                                DataPipeCommand command,
                                uint32_t num_bytes) {
  std::unique_ptr<PortsMessage> message =
      PortsMessage::NewUserMessage(sizeof(DataPipeControlMessage), 0, 0);
  CHECK(message);

  DataPipeControlMessage* data =
      static_cast<DataPipeControlMessage*>(message->mutable_payload_bytes());
  data->command = command;
  data->num_bytes = num_bytes;

  int rv = node_controller->SendMessage(port, std::move(message));
  if (rv != ports::OK && rv != ports::ERROR_PORT_PEER_CLOSED) {
    DLOG(ERROR) << "Unexpected failure sending data pipe control message: "
                << rv;
  }
}

}  // namespace edk
}  // namespace mojo
