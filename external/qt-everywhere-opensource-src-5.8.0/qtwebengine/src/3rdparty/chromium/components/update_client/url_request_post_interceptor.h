// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_URL_REQUEST_POST_INTERCEPTOR_H_
#define COMPONENTS_UPDATE_CLIENT_URL_REQUEST_POST_INTERCEPTOR_H_

#include <stdint.h>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace net {
class URLRequest;
}

namespace update_client {

// Intercepts requests to a file path, counts them, and captures the body of
// the requests. Optionally, for each request, it can return a canned response
// from a given file. The class maintains a queue of expectations, and returns
// one and only one response for each request that matches and it is
// intercepted.
class URLRequestPostInterceptor {
 public:
  // Allows a generic string maching interface when setting up expectations.
  class RequestMatcher {
   public:
    virtual bool Match(const std::string& actual) const = 0;
    virtual ~RequestMatcher() {}
  };

  // Returns the url that is intercepted.
  GURL GetUrl() const;

  // Sets an expection for the body of the POST request and optionally,
  // provides a canned response identified by a |file_path| to be returned when
  // the expectation is met. If no |file_path| is provided, then an empty
  // response body is served. If |response_code| is provided, then an empty
  // response body with that response code is returned.
  // Returns |true| if the expectation was set. This class takes ownership of
  // the |request_matcher| object.
  bool ExpectRequest(class RequestMatcher* request_matcher);
  bool ExpectRequest(class RequestMatcher* request_matcher, int response_code);
  bool ExpectRequest(class RequestMatcher* request_matcher,
                     const base::FilePath& filepath);

  // Returns how many requests have been intercepted and matched by
  // an expectation. One expectation can only be matched by one request.
  int GetHitCount() const;

  // Returns how many requests in total have been captured by the interceptor.
  int GetCount() const;

  // Returns all requests that have been intercepted, matched or not.
  std::vector<std::string> GetRequests() const;

  // Returns all requests as a string for debugging purposes.
  std::string GetRequestsAsString() const;

  // Resets the state of the interceptor so that new expectations can be set.
  void Reset();

  class Delegate;

 private:
  friend class URLRequestPostInterceptorFactory;

  static const int kResponseCode200 = 200;

  struct ExpectationResponse {
    ExpectationResponse(int code, const std::string& body)
        : response_code(code), response_body(body) {}
    const int response_code;
    const std::string response_body;
  };
  typedef std::pair<const RequestMatcher*, ExpectationResponse> Expectation;

  URLRequestPostInterceptor(
      const GURL& url,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  ~URLRequestPostInterceptor();

  void ClearExpectations();

  const GURL url_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  mutable base::Lock interceptor_lock_;
  mutable int hit_count_;
  mutable std::vector<std::string> requests_;
  mutable std::queue<Expectation> expectations_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPostInterceptor);
};

class URLRequestPostInterceptorFactory {
 public:
  URLRequestPostInterceptorFactory(
      const std::string& scheme,
      const std::string& hostname,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  ~URLRequestPostInterceptorFactory();

  // Creates an interceptor object for the specified url path. Returns NULL
  // in case of errors or a valid interceptor object otherwise. The caller
  // does not own the returned object.
  URLRequestPostInterceptor* CreateInterceptor(const base::FilePath& filepath);

 private:
  const std::string scheme_;
  const std::string hostname_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // After creation, |delegate_| lives on the IO thread and it is owned by
  // a URLRequestFilter after registration. A task to unregister it and
  // implicitly destroy it is posted from ~URLRequestPostInterceptorFactory().
  URLRequestPostInterceptor::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPostInterceptorFactory);
};

// Intercepts HTTP POST requests sent to "localhost2".
class InterceptorFactory : public URLRequestPostInterceptorFactory {
 public:
  explicit InterceptorFactory(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  ~InterceptorFactory();

  // Creates an interceptor for the url path defined by POST_INTERCEPT_PATH.
  URLRequestPostInterceptor* CreateInterceptor();

  // Creates an interceptor for the given url path.
  URLRequestPostInterceptor* CreateInterceptorForPath(const char* url_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(InterceptorFactory);
};

class PartialMatch : public URLRequestPostInterceptor::RequestMatcher {
 public:
  explicit PartialMatch(const std::string& expected) : expected_(expected) {}
  bool Match(const std::string& actual) const override;

 private:
  const std::string expected_;

  DISALLOW_COPY_AND_ASSIGN(PartialMatch);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_URL_REQUEST_POST_INTERCEPTOR_H_
