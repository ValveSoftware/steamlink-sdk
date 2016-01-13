// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_XMPP_PUSH_CLIENT_H_
#define JINGLE_NOTIFIER_LISTENER_XMPP_PUSH_CLIENT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/communicator/login.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "jingle/notifier/listener/push_client.h"
#include "jingle/notifier/listener/push_notifications_listen_task.h"
#include "jingle/notifier/listener/push_notifications_subscribe_task.h"
#include "jingle/notifier/listener/send_ping_task.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace notifier {

// This class implements a client for the XMPP google:push protocol.
//
// This class must be used on a single thread.
class XmppPushClient :
      public PushClient,
      public Login::Delegate,
      public PushNotificationsListenTaskDelegate,
      public PushNotificationsSubscribeTaskDelegate,
      public SendPingTaskDelegate {
 public:
  explicit XmppPushClient(const NotifierOptions& notifier_options);
  virtual ~XmppPushClient();

  // PushClient implementation.
  virtual void AddObserver(PushClientObserver* observer) OVERRIDE;
  virtual void RemoveObserver(PushClientObserver* observer) OVERRIDE;
  virtual void UpdateSubscriptions(
      const SubscriptionList& subscriptions) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void SendNotification(const Notification& notification) OVERRIDE;
  virtual void SendPing() OVERRIDE;

  // Login::Delegate implementation.
  virtual void OnConnect(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task) OVERRIDE;
  virtual void OnTransientDisconnection() OVERRIDE;
  virtual void OnCredentialsRejected() OVERRIDE;

  // PushNotificationsListenTaskDelegate implementation.
  virtual void OnNotificationReceived(
      const Notification& notification) OVERRIDE;

  // PushNotificationsSubscribeTaskDelegate implementation.
  virtual void OnSubscribed() OVERRIDE;
  virtual void OnSubscriptionError() OVERRIDE;

  // SendPingTaskDelegate implementation.
  virtual void OnPingResponseReceived() OVERRIDE;

 private:
  base::ThreadChecker thread_checker_;
  const NotifierOptions notifier_options_;
  ObserverList<PushClientObserver> observers_;

  // XMPP connection settings.
  SubscriptionList subscriptions_;
  buzz::XmppClientSettings xmpp_settings_;

  scoped_ptr<notifier::Login> login_;

  // The XMPP connection.
  base::WeakPtr<buzz::XmppTaskParentInterface> base_task_;

  std::vector<Notification> pending_notifications_to_send_;

  DISALLOW_COPY_AND_ASSIGN(XmppPushClient);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_XMPP_PUSH_CLIENT_H_
