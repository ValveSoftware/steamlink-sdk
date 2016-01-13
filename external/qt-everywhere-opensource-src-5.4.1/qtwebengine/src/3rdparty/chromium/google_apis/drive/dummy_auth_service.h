// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DUMMY_AUTH_SERVICE_H_
#define GOOGLE_APIS_DRIVE_DUMMY_AUTH_SERVICE_H_

#include "base/compiler_specific.h"
#include "google_apis/drive/auth_service_interface.h"

namespace google_apis {

// Dummy implementation of AuthServiceInterface that always return a dummy
// access token.
class DummyAuthService : public AuthServiceInterface {
 public:
  // The constructor presets non-empty tokens. When a test for checking auth
  // failure case (i.e., empty tokens) is needed, explicitly clear them by the
  // Clear{Access, Refresh}Token methods.
  DummyAuthService();

  void set_access_token(const std::string& token) { access_token_ = token; }
  void set_refresh_token(const std::string& token) { refresh_token_ = token; }
  const std::string& refresh_token() const { return refresh_token_; }

  // AuthServiceInterface overrides.
  virtual void AddObserver(AuthServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(AuthServiceObserver* observer) OVERRIDE;
  virtual void StartAuthentication(const AuthStatusCallback& callback) OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual const std::string& access_token() const OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;

 private:
  std::string access_token_;
  std::string refresh_token_;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DUMMY_AUTH_SERVICE_H_
