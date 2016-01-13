// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_KNOWN_LOGS_H_
#define NET_CERT_CT_KNOWN_LOGS_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

class CTLogVerifier;

namespace ct {

// Creates a CTLogVerifier instance for the Google Pilot CT Log
NET_EXPORT scoped_ptr<CTLogVerifier> CreateGooglePilotLogVerifier();

// Creates a CTLogVerifier instance for the Google "Aviator" Log
NET_EXPORT scoped_ptr<CTLogVerifier> CreateGoogleAviatorLogVerifier();

// Creates a CTLogVerifier instance for the Google "Rocketeer" Log
NET_EXPORT scoped_ptr<CTLogVerifier> CreateGoogleRocketeerLogVerifier();

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_KNOWN_LOGS_H_
