// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_SESSION_ASSIGNMENT_H_
#define BLIMP_CLIENT_PUBLIC_SESSION_ASSIGNMENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/ip_endpoint.h"

namespace net {
class X509Certificate;
}

namespace blimp {
namespace client {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blimp_public.session
enum AssignmentRequestResult {
  ASSIGNMENT_REQUEST_RESULT_UNKNOWN = 0,
  ASSIGNMENT_REQUEST_RESULT_OK = 1,
  ASSIGNMENT_REQUEST_RESULT_BAD_REQUEST = 2,
  ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE = 3,
  ASSIGNMENT_REQUEST_RESULT_INVALID_PROTOCOL_VERSION = 4,
  ASSIGNMENT_REQUEST_RESULT_EXPIRED_ACCESS_TOKEN = 5,
  ASSIGNMENT_REQUEST_RESULT_USER_INVALID = 6,
  ASSIGNMENT_REQUEST_RESULT_OUT_OF_VMS = 7,
  ASSIGNMENT_REQUEST_RESULT_SERVER_ERROR = 8,
  ASSIGNMENT_REQUEST_RESULT_SERVER_INTERRUPTED = 9,
  ASSIGNMENT_REQUEST_RESULT_NETWORK_FAILURE = 10,
  ASSIGNMENT_REQUEST_RESULT_INVALID_CERT = 11,
};

// An Assignment contains the configuration data needed for a client
// to connect to the engine.
struct Assignment {
  enum TransportProtocol {
    UNKNOWN = 0,
    SSL = 1,
    TCP = 2,
    GRPC = 3,
  };

  Assignment();
  Assignment(const Assignment& other);
  ~Assignment();

  // Returns false if the net::IPEndPoint has an unspecified IP, port, or
  // transport protocol.
  bool IsValid() const;

  // Specifies the transport to use to connect to the engine.
  TransportProtocol transport_protocol;

  // Specifies the IP address and port on which to reach the engine.
  net::IPEndPoint engine_endpoint;

  // Used to authenticate to the specified engine.
  std::string client_auth_token;

  // Specifies the engine's X.509 certificate.
  scoped_refptr<net::X509Certificate> cert;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_SESSION_ASSIGNMENT_H_
