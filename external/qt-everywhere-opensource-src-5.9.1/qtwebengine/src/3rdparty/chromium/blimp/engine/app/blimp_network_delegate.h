// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_NETWORK_DELEGATE_H_
#define BLIMP_ENGINE_APP_BLIMP_NETWORK_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace blimp {
namespace engine {

class BlimpNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  BlimpNetworkDelegate();
  ~BlimpNetworkDelegate() override;

  // Affects subsequent cookie retrieval and setting.
  static void SetAcceptAllCookies(bool accept);

 private:
  // net::NetworkDelegate implementation.
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list) override;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const std::string& cookie_line,
                      net::CookieOptions* options) override;

  DISALLOW_COPY_AND_ASSIGN(BlimpNetworkDelegate);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_NETWORK_DELEGATE_H_
