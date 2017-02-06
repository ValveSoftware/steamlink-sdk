// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/url_request_post_interceptor.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "components/update_client/test_configurator.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_util.h"

namespace update_client {

// Returns a canned response.
class URLRequestMockJob : public net::URLRequestSimpleJob {
 public:
  URLRequestMockJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate,
                    int response_code,
                    const std::string& response_body)
      : net::URLRequestSimpleJob(request, network_delegate),
        response_code_(response_code),
        response_body_(response_body) {}

 protected:
  int GetResponseCode() const override { return response_code_; }

  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const net::CompletionCallback& callback) const override {
    mime_type->assign("text/plain");
    charset->assign("US-ASCII");
    data->assign(response_body_);
    return net::OK;
  }

 private:
  ~URLRequestMockJob() override {}

  int response_code_;
  std::string response_body_;
  DISALLOW_COPY_AND_ASSIGN(URLRequestMockJob);
};

URLRequestPostInterceptor::URLRequestPostInterceptor(
    const GURL& url,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : url_(url), io_task_runner_(io_task_runner), hit_count_(0) {
}

URLRequestPostInterceptor::~URLRequestPostInterceptor() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  ClearExpectations();
}

void URLRequestPostInterceptor::ClearExpectations() {
  while (!expectations_.empty()) {
    Expectation expectation(expectations_.front());
    delete expectation.first;
    expectations_.pop();
  }
}

GURL URLRequestPostInterceptor::GetUrl() const {
  return url_;
}

bool URLRequestPostInterceptor::ExpectRequest(
    class RequestMatcher* request_matcher) {
  expectations_.push(std::make_pair(request_matcher,
                                    ExpectationResponse(kResponseCode200, "")));
  return true;
}

bool URLRequestPostInterceptor::ExpectRequest(
    class RequestMatcher* request_matcher,
    int response_code) {
  expectations_.push(
      std::make_pair(request_matcher, ExpectationResponse(response_code, "")));
  return true;
}

bool URLRequestPostInterceptor::ExpectRequest(
    class RequestMatcher* request_matcher,
    const base::FilePath& filepath) {
  std::string response;
  if (filepath.empty() || !base::ReadFileToString(filepath, &response))
    return false;

  expectations_.push(std::make_pair(
      request_matcher, ExpectationResponse(kResponseCode200, response)));
  return true;
}

int URLRequestPostInterceptor::GetHitCount() const {
  base::AutoLock auto_lock(interceptor_lock_);
  return hit_count_;
}

int URLRequestPostInterceptor::GetCount() const {
  base::AutoLock auto_lock(interceptor_lock_);
  return static_cast<int>(requests_.size());
}

std::vector<std::string> URLRequestPostInterceptor::GetRequests() const {
  base::AutoLock auto_lock(interceptor_lock_);
  return requests_;
}

std::string URLRequestPostInterceptor::GetRequestsAsString() const {
  std::vector<std::string> requests(GetRequests());

  std::string s = "Requests are:";

  int i = 0;
  for (std::vector<std::string>::const_iterator it = requests.begin();
       it != requests.end(); ++it) {
    s.append(base::StringPrintf("\n  (%d): %s", ++i, it->c_str()));
  }

  return s;
}

void URLRequestPostInterceptor::Reset() {
  base::AutoLock auto_lock(interceptor_lock_);
  hit_count_ = 0;
  requests_.clear();
  ClearExpectations();
}

class URLRequestPostInterceptor::Delegate : public net::URLRequestInterceptor {
 public:
  Delegate(const std::string& scheme,
           const std::string& hostname,
           const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
      : scheme_(scheme), hostname_(hostname), io_task_runner_(io_task_runner) {}

  void Register() {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        scheme_, hostname_, std::unique_ptr<net::URLRequestInterceptor>(this));
  }

  void Unregister() {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    for (InterceptorMap::iterator it = interceptors_.begin();
         it != interceptors_.end(); ++it)
      delete (*it).second;
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(scheme_,
                                                                hostname_);
  }

  void OnCreateInterceptor(URLRequestPostInterceptor* interceptor) {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    DCHECK(interceptors_.find(interceptor->GetUrl()) == interceptors_.end());

    interceptors_.insert(std::make_pair(interceptor->GetUrl(), interceptor));
  }

 private:
  ~Delegate() override {}

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

    // Only intercepts POST.
    if (!request->has_upload())
      return NULL;

    GURL url = request->url();
    if (url.has_query()) {
      GURL::Replacements replacements;
      replacements.ClearQuery();
      url = url.ReplaceComponents(replacements);
    }

    InterceptorMap::const_iterator it(interceptors_.find(url));
    if (it == interceptors_.end())
      return NULL;

    // There is an interceptor hooked up for this url. Read the request body,
    // check the existing expectations, and handle the matching case by
    // popping the expectation off the queue, counting the match, and
    // returning a mock object to serve the canned response.
    URLRequestPostInterceptor* interceptor(it->second);

    const net::UploadDataStream* stream = request->get_upload();
    const net::UploadBytesElementReader* reader =
        (*stream->GetElementReaders())[0]->AsBytesReader();
    const int size = reader->length();
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(size));
    const std::string request_body(reader->bytes());

    {
      base::AutoLock auto_lock(interceptor->interceptor_lock_);
      interceptor->requests_.push_back(request_body);
      if (interceptor->expectations_.empty())
        return NULL;
      const URLRequestPostInterceptor::Expectation& expectation(
          interceptor->expectations_.front());
      if (expectation.first->Match(request_body)) {
        const int response_code(expectation.second.response_code);
        const std::string response_body(expectation.second.response_body);
        delete expectation.first;
        interceptor->expectations_.pop();
        ++interceptor->hit_count_;

        return new URLRequestMockJob(request, network_delegate, response_code,
                                     response_body);
      }
    }

    return NULL;
  }

  typedef std::map<GURL, URLRequestPostInterceptor*> InterceptorMap;
  InterceptorMap interceptors_;

  const std::string scheme_;
  const std::string hostname_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

URLRequestPostInterceptorFactory::URLRequestPostInterceptorFactory(
    const std::string& scheme,
    const std::string& hostname,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : scheme_(scheme),
      hostname_(hostname),
      io_task_runner_(io_task_runner),
      delegate_(new URLRequestPostInterceptor::Delegate(scheme,
                                                        hostname,
                                                        io_task_runner)) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&URLRequestPostInterceptor::Delegate::Register,
                            base::Unretained(delegate_)));
}

URLRequestPostInterceptorFactory::~URLRequestPostInterceptorFactory() {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&URLRequestPostInterceptor::Delegate::Unregister,
                            base::Unretained(delegate_)));
}

URLRequestPostInterceptor* URLRequestPostInterceptorFactory::CreateInterceptor(
    const base::FilePath& filepath) {
  const GURL base_url(
      base::StringPrintf("%s://%s", scheme_.c_str(), hostname_.c_str()));
  GURL absolute_url(base_url.Resolve(filepath.MaybeAsASCII()));
  URLRequestPostInterceptor* interceptor(
      new URLRequestPostInterceptor(absolute_url, io_task_runner_));
  bool res = io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestPostInterceptor::Delegate::OnCreateInterceptor,
                 base::Unretained(delegate_), base::Unretained(interceptor)));
  if (!res) {
    delete interceptor;
    return NULL;
  }

  return interceptor;
}

bool PartialMatch::Match(const std::string& actual) const {
  return actual.find(expected_) != std::string::npos;
}

InterceptorFactory::InterceptorFactory(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : URLRequestPostInterceptorFactory(POST_INTERCEPT_SCHEME,
                                       POST_INTERCEPT_HOSTNAME,
                                       io_task_runner) {
}

InterceptorFactory::~InterceptorFactory() {
}

URLRequestPostInterceptor* InterceptorFactory::CreateInterceptor() {
  return CreateInterceptorForPath(POST_INTERCEPT_PATH);
}

URLRequestPostInterceptor* InterceptorFactory::CreateInterceptorForPath(
    const char* url_path) {
  return URLRequestPostInterceptorFactory::CreateInterceptor(
      base::FilePath::FromUTF8Unsafe(url_path));
}

}  // namespace update_client
