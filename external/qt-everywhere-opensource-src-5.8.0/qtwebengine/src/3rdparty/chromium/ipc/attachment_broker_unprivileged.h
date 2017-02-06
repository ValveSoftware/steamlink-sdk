// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_
#define IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_

#include "base/macros.h"
#include "ipc/attachment_broker.h"
#include "ipc/ipc_export.h"

namespace IPC {

class Endpoint;
class Sender;

// This abstract subclass of AttachmentBroker is intended for use in
// non-privileged processes.
class IPC_EXPORT AttachmentBrokerUnprivileged : public IPC::AttachmentBroker {
 public:
  AttachmentBrokerUnprivileged();
  ~AttachmentBrokerUnprivileged() override;

  // If there is no global attachment broker, makes a new
  // AttachmentBrokerUnprivileged and sets it as the global attachment broker.
  // This method is thread safe.
  static void CreateBrokerIfNeeded();

  // AttachmentBroker:
  void RegisterBrokerCommunicationChannel(Endpoint* endpoint) override;
  void DeregisterBrokerCommunicationChannel(Endpoint* endpoint) override;
  bool IsPrivilegedBroker() override;

 protected:
  IPC::Sender* get_sender() { return sender_; }

  // Errors that can be reported by subclasses.
  // These match tools/metrics/histograms/histograms.xml.
  // This enum is append-only.
  enum UMAError {
    // The brokerable attachment was successfully processed.
    SUCCESS = 0,
    // The brokerable attachment's destination was not the process that received
    // the attachment.
    WRONG_DESTINATION = 1,
    // An error occurred while trying to receive a Mach port with mach_msg().
    ERR_RECEIVE_MACH_MESSAGE = 2,
    ERROR_MAX
  };

  // Emits an UMA metric.
  void LogError(UMAError error);

 private:
  // |sender_| is used to send Messages to the privileged broker process.
  // |sender_| must live at least as long as this instance.
  IPC::Sender* sender_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerUnprivileged);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_
