// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_GAIA_TOKEN_PRE_XMPP_AUTH_H_
#define JINGLE_NOTIFIER_BASE_GAIA_TOKEN_PRE_XMPP_AUTH_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "talk/xmpp/prexmppauth.h"

namespace notifier {

// This class implements buzz::PreXmppAuth interface for token-based
// authentication in GTalk. It looks for the X-GOOGLE-TOKEN auth mechanism
// and uses that instead of the default auth mechanism (PLAIN).
class GaiaTokenPreXmppAuth : public buzz::PreXmppAuth {
 public:
  GaiaTokenPreXmppAuth(const std::string& username, const std::string& token,
                       const std::string& token_service,
                       const std::string& auth_mechanism);

  virtual ~GaiaTokenPreXmppAuth();

  // buzz::PreXmppAuth (-buzz::SaslHandler) implementation.  We stub
  // all the methods out as we don't actually do any authentication at
  // this point.
  virtual void StartPreXmppAuth(const buzz::Jid& jid,
                                const talk_base::SocketAddress& server,
                                const talk_base::CryptString& pass,
                                const std::string& auth_mechanism,
                                const std::string& auth_token) OVERRIDE;

  virtual bool IsAuthDone() const OVERRIDE;

  virtual bool IsAuthorized() const OVERRIDE;

  virtual bool HadError() const OVERRIDE;

  virtual int GetError() const OVERRIDE;

  virtual buzz::CaptchaChallenge GetCaptchaChallenge() const OVERRIDE;

  virtual std::string GetAuthToken() const OVERRIDE;

  virtual std::string GetAuthMechanism() const OVERRIDE;

  // buzz::SaslHandler implementation.

  virtual std::string ChooseBestSaslMechanism(
      const std::vector<std::string>& mechanisms, bool encrypted) OVERRIDE;

  virtual buzz::SaslMechanism* CreateSaslMechanism(
      const std::string& mechanism) OVERRIDE;

 private:
  std::string username_;
  std::string token_;
  std::string token_service_;
  std::string auth_mechanism_;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_GAIA_TOKEN_PRE_XMPP_AUTH_H_
