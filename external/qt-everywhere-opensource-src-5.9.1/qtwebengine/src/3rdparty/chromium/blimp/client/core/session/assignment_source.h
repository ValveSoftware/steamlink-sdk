// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SESSION_ASSIGNMENT_SOURCE_H_
#define BLIMP_CLIENT_CORE_SESSION_ASSIGNMENT_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "blimp/client/public/session/assignment.h"
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
}

namespace blimp {
namespace client {

// AssignmentSource provides functionality to find out how a client should
// connect to an engine.
class AssignmentSource : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(AssignmentRequestResult, const Assignment&)>
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

#endif  // BLIMP_CLIENT_CORE_SESSION_ASSIGNMENT_SOURCE_H_
