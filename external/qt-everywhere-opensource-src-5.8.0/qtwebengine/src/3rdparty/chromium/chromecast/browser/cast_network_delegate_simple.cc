// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_delegate.h"

#include "base/macros.h"
#include "url/gurl.h"

namespace chromecast {
namespace shell {

namespace {

class CastNetworkDelegateSimple : public CastNetworkDelegate {
 public:
  CastNetworkDelegateSimple() {}

 private:
  // CastNetworkDelegate implementation:
  void Initialize(bool use_sync_signing) override {}
  bool IsWhitelisted(const GURL& gurl, int render_process_id,
                     bool for_device_auth) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(CastNetworkDelegateSimple);
};

}  // namespace

// static
CastNetworkDelegate* CastNetworkDelegate::Create() {
  return new CastNetworkDelegateSimple();
}

// static
net::X509Certificate* CastNetworkDelegate::DeviceCert() {
  return NULL;
}

}  // namespace shell
}  // namespace chromecast
