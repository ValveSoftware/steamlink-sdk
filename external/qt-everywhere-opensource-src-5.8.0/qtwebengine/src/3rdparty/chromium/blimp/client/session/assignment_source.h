// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_
#define BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
class Value;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
class X509Certificate;
}

namespace blimp {
namespace client {

// An Assignment contains the configuration data needed for a client
// to connect to the engine.
struct Assignment {
  enum TransportProtocol {
    UNKNOWN = 0,
    SSL = 1,
    TCP = 2,
  };

  Assignment();
  Assignment(const Assignment& other);
  ~Assignment();

  // Returns true if the net::IPEndPoint has an unspecified IP, port, or
  // transport protocol.
  bool IsValid() const;

  // Specifies the transport to use to connect to the engine.
  TransportProtocol transport_protocol;

  // Specifies the IP address and port on which to reach the engine.
  net::IPEndPoint engine_endpoint;

  // Used to authenticate to the specified engine.
  std::string client_token;

  // Specifies the X.509 certificate that the engine must report.
  scoped_refptr<net::X509Certificate> cert;
};

// AssignmentSource provides functionality to find out how a client should
// connect to an engine.
class AssignmentSource : public net::URLFetcherDelegate {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blimp.assignment
  enum Result {
    RESULT_UNKNOWN = 0,
    RESULT_OK = 1,
    RESULT_BAD_REQUEST = 2,
    RESULT_BAD_RESPONSE = 3,
    RESULT_INVALID_PROTOCOL_VERSION = 4,
    RESULT_EXPIRED_ACCESS_TOKEN = 5,
    RESULT_USER_INVALID = 6,
    RESULT_OUT_OF_VMS = 7,
    RESULT_SERVER_ERROR = 8,
    RESULT_SERVER_INTERRUPTED = 9,
    RESULT_NETWORK_FAILURE = 10,
    RESULT_INVALID_CERT = 11,
  };

  typedef base::Callback<void(AssignmentSource::Result, const Assignment&)>
      AssignmentCallback;

  // |assigner_endpoint|: The URL of the Assigner service to query.
  // |network_task_runner|: The task runner to use for querying the Assigner API
  // over the network.
  // |file_task_runner|: The task runner to use for reading cert files from disk
  // (specified on the command line.)
  AssignmentSource(
      const GURL& assigner_endpoint,
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner);

  ~AssignmentSource() override;

  // Retrieves a valid assignment for the client and posts the result to the
  // given callback.  |client_auth_token| is the OAuth2 access token to send to
  // the assigner when requesting an assignment.  If this is called before a
  // previous call has completed, the old callback will be called with
  // RESULT_SERVER_INTERRUPTED and no Assignment.
  void GetAssignment(const std::string& client_auth_token,
                     const AssignmentCallback& callback);

 private:
  void OnGetAssignmentFromCommandLineDone(const std::string& client_auth_token,
                                          Assignment parsed_assignment);
  void QueryAssigner(const std::string& client_auth_token);
  void ParseAssignerResponse();
  void OnJsonParsed(std::unique_ptr<base::Value> json);
  void OnJsonParseError(const std::string& error);

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // GetAssignment() callback, invoked after URLFetcher completion.
  AssignmentCallback callback_;

  const GURL assigner_endpoint_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  base::WeakPtrFactory<AssignmentSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssignmentSource);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_
