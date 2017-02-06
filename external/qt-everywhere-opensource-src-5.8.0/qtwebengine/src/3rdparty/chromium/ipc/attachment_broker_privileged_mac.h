// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_PRIVILEGED_MAC_H_
#define IPC_ATTACHMENT_BROKER_PRIVILEGED_MAC_H_

#include <mach/mach.h>
#include <stdint.h>

#include <map>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/process/port_provider_mac.h"
#include "base/synchronization/lock.h"
#include "ipc/attachment_broker_privileged.h"
#include "ipc/ipc_export.h"
#include "ipc/mach_port_attachment_mac.h"

namespace base {
class PortProvider;
}  // namespace base

namespace IPC {

// This class is a concrete subclass of AttachmentBrokerPrivileged for the
// OSX platform.
//
// An example of the typical process by which a Mach port gets brokered.
// Definitions:
//   1. Let there be three processes P1, U2, U3. P1 is privileged.
//   2. U2 wants to send a Mach port M2 to U3. If this port is inserted into P1,
//   it will be called M1. If it is inserted into U3, it will be called M3.
//   3. name() returns a serializable representation of a Mach port that can be
//   passed over chrome IPC.
//   4. pid() returns the process id of a process.
//
// Process:
//   1. U2 sends a AttachmentBrokerMsg_DuplicateMachPort message to P1. The
//   message contains name(M2), and pid(U3).
//   2. P1 extracts M2 into its own namespace, making M1.
//   3. P1 makes a new Mach port R in U3.
//   4. P1 sends a mach_msg with M1 to R.
//   5. P1 sends name(R) to U3.
//   6. U3 retrieves the queued message from R. The kernel automatically
//   translates M1 into the namespace of U3, making M3.
//
// The logic of this class is a little bit more complex becauese any or all of
// P1, U2 and U3 may be the same, and depending on the exact configuration,
// the creation of R may not be necessary.
//
// For the rest of this file, and the corresponding implementation file, R will
// be called the "intermediate Mach port" and M3 the "final Mach port".
class IPC_EXPORT AttachmentBrokerPrivilegedMac
    : public AttachmentBrokerPrivileged,
      public base::PortProvider::Observer {
 public:
  explicit AttachmentBrokerPrivilegedMac(base::PortProvider* port_provider);
  ~AttachmentBrokerPrivilegedMac() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(
      const scoped_refptr<IPC::BrokerableAttachment>& attachment,
      base::ProcessId destination_process) override;
  void DeregisterCommunicationChannel(Endpoint* endpoint) override;
  void ReceivedPeerPid(base::ProcessId peer_pid) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

  // base::PortProvider::Observer override.
  void OnReceivedTaskPort(base::ProcessHandle process) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerPrivilegedMacMultiProcessTest,
                           InsertRight);
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerPrivilegedMacMultiProcessTest,
                           InsertSameRightTwice);
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerPrivilegedMacMultiProcessTest,
                           InsertTwoRights);
  using MachPortWireFormat = internal::MachPortAttachmentMac::WireFormat;

  // Contains all the information necessary to broker an attachment into a
  // destination process. The only thing that prevents an AttachmentPrecusor
  // from being immediately processed is if |port_provider_| does not yet have a
  // task port for |pid|.
  class IPC_EXPORT AttachmentPrecursor {
   public:
    AttachmentPrecursor(const base::ProcessId& pid,
                        base::mac::ScopedMachSendRight port_to_insert,
                        const BrokerableAttachment::AttachmentId& id);
    ~AttachmentPrecursor();

    // Caller takes ownership of |port_|.
    base::mac::ScopedMachSendRight TakePort();

    base::ProcessId pid() const { return pid_; }
    const BrokerableAttachment::AttachmentId id() const { return id_; }

   private:
    // The pid of the destination process.
    const base::ProcessId pid_;
    // The final Mach port, as per definition at the top of this file.
    base::mac::ScopedMachSendRight port_;
    // The id of the attachment.
    const BrokerableAttachment::AttachmentId id_;
    DISALLOW_COPY_AND_ASSIGN(AttachmentPrecursor);
  };

  // Contains all the information necessary to extract a send right and create
  // an AttachmentPrecursor. The only thing that prevents an AttachmentExtractor
  // from being immediately processed is if |port_provider_| does not yet have a
  // task port for |source_pid|.
  class IPC_EXPORT AttachmentExtractor {
   public:
    AttachmentExtractor(const base::ProcessId& source_pid,
                        const base::ProcessId& dest_pid,
                        mach_port_name_t port,
                        const BrokerableAttachment::AttachmentId& id);
    ~AttachmentExtractor();

    base::ProcessId source_pid() const { return source_pid_; }
    base::ProcessId dest_pid() const { return dest_pid_; }
    mach_port_name_t port() const { return port_to_extract_; }
    const BrokerableAttachment::AttachmentId id() const { return id_; }

   private:
    const base::ProcessId source_pid_;
    const base::ProcessId dest_pid_;
    mach_port_name_t port_to_extract_;
    const BrokerableAttachment::AttachmentId id_;
  };

  // IPC message handlers.
  void OnDuplicateMachPort(const Message& message);

  // Duplicates the Mach port referenced from |wire_format| from
  // |source_process| into |wire_format|'s destination process.
  MachPortWireFormat DuplicateMachPort(const MachPortWireFormat& wire_format,
                                       base::ProcessId source_process);

  // Extracts a copy of the send right to |named_right| from |task_port|.
  // Returns MACH_PORT_NULL on error.
  base::mac::ScopedMachSendRight ExtractNamedRight(
      mach_port_t task_port,
      mach_port_name_t named_right);

  // Copies an existing |wire_format|, but substitutes in a different mach port.
  MachPortWireFormat CopyWireFormat(const MachPortWireFormat& wire_format,
                                    uint32_t mach_port);

  // |wire_format.destination_process| must be this process.
  // |wire_format.mach_port| must be the final Mach port.
  // Consumes a reference to |wire_format.mach_port|, as ownership is implicitly
  // passed to the consumer of the Chrome IPC message.
  // Makes an attachment, queues it, and notifies the observers.
  void RoutePrecursorToSelf(AttachmentPrecursor* precursor);

  // |wire_format.destination_process| must be another process.
  // |wire_format.mach_port| must be the intermediate Mach port.
  // Ownership of |wire_format.mach_port| is implicitly passed to the process
  // that receives the Chrome IPC message.
  // Returns |false| on irrecoverable error.
  bool RouteWireFormatToAnother(const MachPortWireFormat& wire_format);

  // Atempts to broker all precursors whose destination is |pid|. Has no effect
  // if |port_provider_| does not have the task port for |pid|.
  // If a communication channel has not been established from the destination
  // process, and |store_on_failure| is true, then the precursor is kept for
  // later reuse. If |store_on_failure| is false, then the precursor is deleted.
  void SendPrecursorsForProcess(base::ProcessId pid, bool store_on_failure);

  // Brokers a single precursor into the task represented by |task_port|.
  // Returns |false| on irrecoverable error.
  bool SendPrecursor(AttachmentPrecursor* precursor, mach_port_t task_port);

  // Add a precursor to |precursors_|. Takes ownership of |port|.
  void AddPrecursor(base::ProcessId pid,
                    base::mac::ScopedMachSendRight port,
                    const BrokerableAttachment::AttachmentId& id);

  // Atempts to process all extractors whose source is |pid|. Has no effect
  // if |port_provider_| does not have the task port for |pid|.
  // If a communication channel has not been established from the source
  // process, and |store_on_failure| is true, then the extractor is kept for
  // later reuse. If |store_on_failure| is false, then the extractor is deleted.
  void ProcessExtractorsForProcess(base::ProcessId pid, bool store_on_failure);

  // Processes a single extractor whose source pid is represented by
  // |task_port|.
  void ProcessExtractor(AttachmentExtractor* extractor, mach_port_t task_port);

  // Add an extractor to |extractors_|.
  void AddExtractor(base::ProcessId source_pid,
                    base::ProcessId dest_pid,
                    mach_port_name_t port,
                    const BrokerableAttachment::AttachmentId& id);

  // The port provider must live at least as long as the AttachmentBroker.
  base::PortProvider* port_provider_;

  // For each ProcessId, a vector of precursors that are waiting to be
  // sent.
  std::map<base::ProcessId, ScopedVector<AttachmentPrecursor>*> precursors_;
  base::Lock precursors_lock_;

  // For each ProcessId, a vector of extractors that are waiting to be
  // processed.
  std::map<base::ProcessId, ScopedVector<AttachmentExtractor>*> extractors_;
  base::Lock extractors_lock_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivilegedMac);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_MAC_H_
