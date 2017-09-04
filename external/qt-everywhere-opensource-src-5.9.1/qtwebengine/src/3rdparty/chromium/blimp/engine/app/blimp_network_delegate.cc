// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_network_delegate.h"

#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "net/url_request/url_request.h"

namespace blimp {
namespace engine {

namespace {
bool g_accept_all_cookies = true;

net::StaticCookiePolicy::Type GetPolicyType() {
  return g_accept_all_cookies
             ? net::StaticCookiePolicy::ALLOW_ALL_COOKIES
             : net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES;
}

}  // namespace

BlimpNetworkDelegate::BlimpNetworkDelegate() {}

BlimpNetworkDelegate::~BlimpNetworkDelegate() {}

void BlimpNetworkDelegate::SetAcceptAllCookies(bool accept) {
  g_accept_all_cookies = accept;
}

bool BlimpNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                           const net::CookieList& cookie_list) {
  net::StaticCookiePolicy policy(GetPolicyType());
  int rv =
      policy.CanGetCookies(request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

bool BlimpNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                          const std::string& cookie_line,
                                          net::CookieOptions* options) {
  net::StaticCookiePolicy policy(GetPolicyType());
  int rv =
      policy.CanSetCookie(request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

}  // namespace engine
}  // namespace blimp
