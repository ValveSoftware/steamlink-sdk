// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"

namespace {
IPC::AttachmentBroker* g_attachment_broker = nullptr;
}

namespace IPC {

// static
void AttachmentBroker::SetGlobal(AttachmentBroker* broker) {
  g_attachment_broker = broker;
}

// static
AttachmentBroker* AttachmentBroker::GetGlobal() {
  return g_attachment_broker;
}

AttachmentBroker::AttachmentBroker() : last_unique_id_(0) {}
AttachmentBroker::~AttachmentBroker() {}

bool AttachmentBroker::GetAttachmentWithId(
    BrokerableAttachment::AttachmentId id,
    scoped_refptr<BrokerableAttachment>* out_attachment) {
  base::AutoLock auto_lock(*get_lock());
  for (AttachmentVector::iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    if ((*it)->GetIdentifier() == id) {
      *out_attachment = *it;
      attachments_.erase(it);
      return true;
    }
  }
  return false;
}

void AttachmentBroker::AddObserver(
    AttachmentBroker::Observer* observer,
    const scoped_refptr<base::SequencedTaskRunner>& runner) {
  base::AutoLock auto_lock(*get_lock());
  auto it = std::find_if(observers_.begin(), observers_.end(),
                      [observer](const ObserverInfo& info) {
                        return info.observer == observer;
                      });
  if (it == observers_.end()) {
    ObserverInfo info;
    info.observer = observer;
    info.runner = runner;
    info.unique_id = ++last_unique_id_;
    observers_.push_back(info);

    // Give the observer a chance to handle attachments that arrived while the
    // observer was handling the message that caused it to register, but our
    // mutex was not yet locked.
    for (const auto& attachment : attachments_) {
      info.runner->PostTask(
          FROM_HERE,
          base::Bind(&AttachmentBroker::NotifyObserver, base::Unretained(this),
                     info.unique_id, attachment->GetIdentifier()));
    }
  }
}

void AttachmentBroker::RemoveObserver(AttachmentBroker::Observer* observer) {
  base::AutoLock auto_lock(*get_lock());
  auto it = std::find_if(observers_.begin(), observers_.end(),
                      [observer](const ObserverInfo& info) {
                        return info.observer == observer;
                      });
  if (it != observers_.end())
    observers_.erase(it);
}

void AttachmentBroker::RegisterCommunicationChannel(
    Endpoint* endpoint,
    scoped_refptr<base::SingleThreadTaskRunner> runner) {
  NOTREACHED();
}

void AttachmentBroker::DeregisterCommunicationChannel(Endpoint* endpoint) {
  NOTREACHED();
}

void AttachmentBroker::RegisterBrokerCommunicationChannel(Endpoint* endpoint) {
  NOTREACHED();
}

void AttachmentBroker::DeregisterBrokerCommunicationChannel(
    Endpoint* endpoint) {
  NOTREACHED();
}

void AttachmentBroker::ReceivedPeerPid(base::ProcessId peer_pid) {
  NOTREACHED();
}

bool AttachmentBroker::IsPrivilegedBroker() {
  NOTREACHED();
  return false;
}

void AttachmentBroker::HandleReceivedAttachment(
    const scoped_refptr<BrokerableAttachment>& attachment) {
  {
    base::AutoLock auto_lock(*get_lock());
    attachments_.push_back(attachment);
  }
  NotifyObservers(attachment->GetIdentifier());
}

void AttachmentBroker::NotifyObservers(
    const BrokerableAttachment::AttachmentId& id) {
  base::AutoLock auto_lock(*get_lock());

  // Dispatch notifications onto the appropriate task runners. This has two
  // effects:
  //   1. Ensures that the notification is posted from the right task runner.
  //   2. Avoids any complications from re-entrant functions, since one of the
  //   observers may be halfway through processing some messages.
  for (const auto& info : observers_) {
    info.runner->PostTask(
        FROM_HERE, base::Bind(&AttachmentBroker::NotifyObserver,
                              base::Unretained(this), info.unique_id, id));
  }
}

void AttachmentBroker::NotifyObserver(
    int unique_id,
    const BrokerableAttachment::AttachmentId& id) {
  Observer* observer = nullptr;
  {
    // Check that the same observer is still registered.
    base::AutoLock auto_lock(*get_lock());
    auto it = std::find_if(observers_.begin(), observers_.end(),
                           [unique_id](const ObserverInfo& info) {
                             return info.unique_id == unique_id;
                           });
    if (it == observers_.end())
      return;
    observer = it->observer;
  }

  observer->ReceivedBrokerableAttachmentWithId(id);
}

AttachmentBroker::ObserverInfo::ObserverInfo() {}
AttachmentBroker::ObserverInfo::ObserverInfo(const ObserverInfo& other) =
    default;
AttachmentBroker::ObserverInfo::~ObserverInfo() {}

}  // namespace IPC
