// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "jingle/notifier/base/server_information.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "jingle/notifier/communicator/single_login_attempt.h"
#include "net/base/network_change_notifier.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class XmppClient;
class XmppClientSettings;
class XmppTaskParentInterface;
}  // namespace buzz

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace notifier {

class LoginSettings;

// Does the login, keeps it alive (with refreshing cookies and
// reattempting login when disconnected), and figures out what actions
// to take on the various errors that may occur.
//
// TODO(akalin): Make this observe proxy config changes also.
class Login : public net::NetworkChangeNotifier::IPAddressObserver,
              public net::NetworkChangeNotifier::ConnectionTypeObserver,
              public net::NetworkChangeNotifier::DNSObserver,
              public SingleLoginAttempt::Delegate {
 public:
  class Delegate {
   public:
    // Called when a connection has been successfully established.
    virtual void OnConnect(
        base::WeakPtr<buzz::XmppTaskParentInterface> base_task) = 0;

    // Called when there's no connection to the server but we expect
    // it to come back come back eventually.  The connection will be
    // retried with exponential backoff.
    virtual void OnTransientDisconnection() = 0;

    // Called when the current login credentials have been rejected.
    // The connection will still be retried with exponential backoff;
    // it's up to the delegate to stop connecting and/or prompt for
    // new credentials.
    virtual void OnCredentialsRejected() = 0;

   protected:
    virtual ~Delegate();
  };

  // Does not take ownership of |delegate|, which must not be NULL.
  Login(Delegate* delegate,
        const buzz::XmppClientSettings& user_settings,
        const scoped_refptr<net::URLRequestContextGetter>&
            request_context_getter,
        const ServerList& servers,
        bool try_ssltcp_first,
        const std::string& auth_mechanism);
  virtual ~Login();

  // Starts connecting (or forces a reconnection if we're backed off).
  void StartConnection();

  // The updated settings take effect only the next time when a
  // connection is attempted (either via reconnection or a call to
  // StartConnection()).
  void UpdateXmppSettings(const buzz::XmppClientSettings& user_settings);

  // net::NetworkChangeNotifier::IPAddressObserver implementation.
  virtual void OnIPAddressChanged() OVERRIDE;

  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // net::NetworkChangeNotifier::DNSObserver implementation.
  virtual void OnDNSChanged() OVERRIDE;

  // SingleLoginAttempt::Delegate implementation.
  virtual void OnConnect(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task) OVERRIDE;
  virtual void OnRedirect(const ServerInformation& redirect_server) OVERRIDE;
  virtual void OnCredentialsRejected() OVERRIDE;
  virtual void OnSettingsExhausted() OVERRIDE;

 private:
  // Called by the various network notifications.
  void OnNetworkEvent();

  // Stops any existing reconnect timer and sets an initial reconnect
  // interval.
  void ResetReconnectState();

  // Tries to reconnect in some point in the future.  If called
  // repeatedly, will wait longer and longer until reconnecting.
  void TryReconnect();

  // The actual function (called by |reconnect_timer_|) that does the
  // reconnection.
  void DoReconnect();

  Delegate* const delegate_;
  LoginSettings login_settings_;
  scoped_ptr<SingleLoginAttempt> single_attempt_;

  // reconnection state.
  base::TimeDelta reconnect_interval_;
  base::OneShotTimer<Login> reconnect_timer_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
