// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "jingle/notifier/listener/notification_defines.h"

namespace notifier {

struct NotifierOptions;
class PushClientObserver;

// A PushClient is an interface for classes that implement a push
// mechanism, where a client can push notifications to and receive
// notifications from other clients.
class PushClient {
 public:
  virtual ~PushClient();

  // Creates a default non-blocking PushClient implementation from the
  // given options.
  static scoped_ptr<PushClient> CreateDefault(
      const NotifierOptions& notifier_options);

  // Creates a default blocking PushClient implementation from the
  // given options.  Must be called from the IO thread (according to
  // |notifier_options|).
  static scoped_ptr<PushClient> CreateDefaultOnIOThread(
      const NotifierOptions& notifier_options);

  // Manage the list of observers for incoming notifications.
  virtual void AddObserver(PushClientObserver* observer) = 0;
  virtual void RemoveObserver(PushClientObserver* observer) = 0;

  // Implementors are required to have this take effect only on the
  // next (re-)connection.  Therefore, clients should call this before
  // UpdateCredentials().
  virtual void UpdateSubscriptions(const SubscriptionList& subscriptions) = 0;

  // If not connected, connects with the given credentials.  If
  // already connected, the next connection attempt will use the given
  // credentials.
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) = 0;

  // Sends a notification (with no reliability guarantees).
  virtual void SendNotification(const Notification& notification) = 0;

  // Sends a ping (with no reliability guarantees).
  virtual void SendPing() = 0;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_H_
