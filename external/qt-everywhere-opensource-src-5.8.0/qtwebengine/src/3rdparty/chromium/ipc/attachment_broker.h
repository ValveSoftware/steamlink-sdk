// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_H_
#define IPC_ATTACHMENT_BROKER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_listener.h"

// If the platform has no attachments that need brokering, then it shouldn't
// compile any code that calls member functions of AttachmentBroker. This
// prevents symbols only used by AttachmentBroker and its subclasses from
// making it into the binary.
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
#define USE_ATTACHMENT_BROKER 1
#else
#define USE_ATTACHMENT_BROKER 0
#endif  // defined(OS_WIN)

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
};

namespace IPC {

class AttachmentBroker;
class Endpoint;

// Classes that inherit from this abstract base class are capable of
// communicating with a broker to send and receive attachments to Chrome IPC
// messages.
class IPC_EXPORT SupportsAttachmentBrokering {
 public:
  // Returns an AttachmentBroker used to broker attachments of IPC messages to
  // other processes. There must be exactly one AttachmentBroker per process.
  virtual AttachmentBroker* GetAttachmentBroker() = 0;
};

// Responsible for brokering attachments to Chrome IPC messages. On platforms
// that support attachment brokering, every IPC channel should have a reference
// to a AttachmentBroker.
// This class is not thread safe. The implementation of this class assumes that
// it is only ever used on the same thread as its consumers.
class IPC_EXPORT AttachmentBroker : public Listener {
 public:
  // A standard observer interface that allows consumers of the AttachmentBroker
  // to be notified when a new attachment has been received.
  class Observer {
   public:
    virtual void ReceivedBrokerableAttachmentWithId(
        const BrokerableAttachment::AttachmentId& id) = 0;
  };

  // Each process has at most one attachment broker. The process is responsible
  // for ensuring that |broker| stays alive for as long as the process is
  // sending/receiving ipc messages.
  static void SetGlobal(AttachmentBroker* broker);
  static AttachmentBroker* GetGlobal();

  AttachmentBroker();
  ~AttachmentBroker() override;

  // Sends |attachment| to |destination_process|. The implementation uses an
  // IPC::Channel to communicate with the broker process. This may be the same
  // IPC::Channel that is requesting the brokering of an attachment.
  // Returns true on success and false otherwise.
  virtual bool SendAttachmentToProcess(
      const scoped_refptr<BrokerableAttachment>& attachment,
      base::ProcessId destination_process) = 0;

  // Returns whether the attachment was available. If the attachment was
  // available, populates the output parameter |attachment|.
  bool GetAttachmentWithId(BrokerableAttachment::AttachmentId id,
                           scoped_refptr<BrokerableAttachment>* attachment);

  // Any given observer should only ever add itself once to the observer list.
  // Notifications to |observer| will be posted to |runner|.
  // The |observer| is expected to call RemoveObserver() before being destroyed.
  void AddObserver(Observer* observer,
                   const scoped_refptr<base::SequencedTaskRunner>& runner);
  void RemoveObserver(Observer* observer);

  // These two methods should only be called by the broker process.
  //
  // Each unprivileged process should have one IPC channel on which it
  // communicates attachment information with the broker process. In the broker
  // process, these channels must be registered and deregistered with the
  // Attachment Broker as they are created and destroyed.
  //
  // Invocations of Send() on |endpoint| will occur on thread bound to |runner|.
  virtual void RegisterCommunicationChannel(
      Endpoint* endpoint,
      scoped_refptr<base::SingleThreadTaskRunner> runner);
  virtual void DeregisterCommunicationChannel(Endpoint* endpoint);

  // In each unprivileged process, exactly one channel should be used to
  // communicate brokerable attachments with the broker process.
  virtual void RegisterBrokerCommunicationChannel(Endpoint* endpoint);
  virtual void DeregisterBrokerCommunicationChannel(Endpoint* endpoint);

  // Informs the attachment broker that a channel endpoint has received its
  // peer's PID.
  virtual void ReceivedPeerPid(base::ProcessId peer_pid);

  // True if and only if this broker is privileged.
  virtual bool IsPrivilegedBroker();

 protected:
  using AttachmentVector = std::vector<scoped_refptr<BrokerableAttachment>>;

  // Adds |attachment| to |attachments_|, and notifies the observers.
  void HandleReceivedAttachment(
      const scoped_refptr<BrokerableAttachment>& attachment);

  // Informs the observers that a new BrokerableAttachment has been received.
  void NotifyObservers(const BrokerableAttachment::AttachmentId& id);

  // Informs the observer identified by |unique_id| that a new
  // BrokerableAttachment has been received.
  void NotifyObserver(int unique_id,
                      const BrokerableAttachment::AttachmentId& id);

  // This method is exposed for testing only.
  AttachmentVector* get_attachments() { return &attachments_; }

  base::Lock* get_lock() { return &lock_; }

 private:
#if defined(OS_WIN)
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerUnprivilegedWinTest,
                           ReceiveValidMessage);
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerUnprivilegedWinTest,
                           ReceiveInvalidMessage);
#endif  // defined(OS_WIN)

  // A vector of BrokerableAttachments that have been received, but not yet
  // consumed.
  // A std::vector is used instead of a std::map because this container is
  // expected to have few elements, for which a std::vector is expected to have
  // better performance.
  AttachmentVector attachments_;

  struct ObserverInfo {
    ObserverInfo();
    ObserverInfo(const ObserverInfo& other);
    ~ObserverInfo();

    Observer* observer;
    int unique_id;

    // Notifications must be dispatched onto |runner|.
    scoped_refptr<base::SequencedTaskRunner> runner;
  };
  std::vector<ObserverInfo> observers_;

  // This member holds the last id given to an ObserverInfo.
  int last_unique_id_;

  // The AttachmentBroker can be accessed from any thread, so modifications to
  // internal state must be guarded by a lock.
  base::Lock lock_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBroker);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_H_
