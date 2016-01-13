// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Methods for sending the update stanza to notify peers via xmpp.

#ifndef JINGLE_NOTIFIER_LISTENER_SEND_PING_TASK_H_
#define JINGLE_NOTIFIER_LISTENER_SEND_PING_TASK_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {
class XmlElement;
}  // namespace

namespace notifier {

class SendPingTask : public buzz::XmppTask {
 public:
  class Delegate {
   public:
    virtual void OnPingResponseReceived() = 0;

   protected:
    virtual ~Delegate();
  };

  SendPingTask(buzz::XmppTaskParentInterface* parent, Delegate* delegate);
  virtual ~SendPingTask();

  // Overridden from buzz::XmppTask.
  virtual int ProcessStart() OVERRIDE;
  virtual int ProcessResponse() OVERRIDE;
  virtual bool HandleStanza(const buzz::XmlElement* stanza) OVERRIDE;

 private:
  static buzz::XmlElement* MakePingStanza(const std::string& task_id);

  FRIEND_TEST_ALL_PREFIXES(SendPingTaskTest, MakePingStanza);

  std::string ping_task_id_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SendPingTask);
};

typedef SendPingTask::Delegate SendPingTaskDelegate;

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_SEND_PING_TASK_H_
