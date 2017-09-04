// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkHintsInterface_h
#define NetworkHintsInterface_h

#include "platform/network/NetworkHints.h"

namespace blink {

class NetworkHintsInterface {
 public:
  virtual void dnsPrefetchHost(const String&) const = 0;
  virtual void preconnectHost(const KURL&,
                              const CrossOriginAttributeValue) const = 0;
};

class NetworkHintsInterfaceImpl : public NetworkHintsInterface {
  void dnsPrefetchHost(const String& host) const override { prefetchDNS(host); }

  void preconnectHost(
      const KURL& host,
      const CrossOriginAttributeValue crossOrigin) const override {
    preconnect(host, crossOrigin);
  }
};

}  // namespace blink

#endif
