// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/view_http_cache_job_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/view_cache_helper.h"

namespace content {
namespace {

// A job subclass that dumps an HTTP cache entry.
class ViewHttpCacheJob : public net::URLRequestJob {
 public:
  ViewHttpCacheJob(net::URLRequest* request,
                   net::NetworkDelegate* network_delegate)
      : net::URLRequestJob(request, network_delegate),
        core_(new Core),
        weak_factory_(this),
        callback_(base::Bind(&ViewHttpCacheJob::OnStartCompleted,
                             base::Unretained(this))) {
  }

  // net::URLRequestJob implementation.
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE{
    return core_->GetMimeType(mime_type);
  }
  virtual bool GetCharset(std::string* charset) OVERRIDE{
    return core_->GetCharset(charset);
  }
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size, int *bytes_read) OVERRIDE{
    return core_->ReadRawData(buf, buf_size, bytes_read);
  }

 private:
  class Core : public base::RefCounted<Core> {
   public:
    Core()
        : data_offset_(0),
          callback_(base::Bind(&Core::OnIOComplete, this)) {
    }

    int Start(const net::URLRequest& request, const base::Closure& callback);

    // Prevents it from invoking its callback. It will self-delete.
    void Orphan() {
      user_callback_.Reset();
    }

    bool GetMimeType(std::string* mime_type) const;
    bool GetCharset(std::string* charset);
    bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

   private:
    friend class base::RefCounted<Core>;

    ~Core() {}

    // Called when ViewCacheHelper completes the operation.
    void OnIOComplete(int result);

    std::string data_;
    int data_offset_;
    net::ViewCacheHelper cache_helper_;
    net::CompletionCallback callback_;
    base::Closure user_callback_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  virtual ~ViewHttpCacheJob() {}

  void StartAsync();
  void OnStartCompleted();

  scoped_refptr<Core> core_;
  base::WeakPtrFactory<ViewHttpCacheJob> weak_factory_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(ViewHttpCacheJob);
};

void ViewHttpCacheJob::Start() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ViewHttpCacheJob::StartAsync, weak_factory_.GetWeakPtr()));
}

void ViewHttpCacheJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  if (core_.get()) {
    core_->Orphan();
    core_ = NULL;
  }
  net::URLRequestJob::Kill();
}

void ViewHttpCacheJob::StartAsync() {
  DCHECK(request());

  if (!request())
    return;

  int rv = core_->Start(*request(), callback_);
  if (rv != net::ERR_IO_PENDING) {
    DCHECK_EQ(net::OK, rv);
    OnStartCompleted();
  }
}

void ViewHttpCacheJob::OnStartCompleted() {
  NotifyHeadersComplete();
}

int ViewHttpCacheJob::Core::Start(const net::URLRequest& request,
                                  const base::Closure& callback) {
  DCHECK(!callback.is_null());
  DCHECK(user_callback_.is_null());

  AddRef();  // Released on OnIOComplete().
  std::string cache_key =
      request.url().spec().substr(strlen(kChromeUINetworkViewCacheURL));

  int rv;
  if (cache_key.empty()) {
    rv = cache_helper_.GetContentsHTML(request.context(),
                                       kChromeUINetworkViewCacheURL,
                                       &data_, callback_);
  } else {
    rv = cache_helper_.GetEntryInfoHTML(cache_key, request.context(),
                                        &data_, callback_);
  }

  if (rv == net::ERR_IO_PENDING)
    user_callback_ = callback;

  return rv;
}

bool ViewHttpCacheJob::Core::GetMimeType(std::string* mime_type) const {
  mime_type->assign("text/html");
  return true;
}

bool ViewHttpCacheJob::Core::GetCharset(std::string* charset) {
  charset->assign("UTF-8");
  return true;
}

bool ViewHttpCacheJob::Core::ReadRawData(net::IOBuffer* buf,
                                         int buf_size,
                                         int* bytes_read) {
  DCHECK(bytes_read);
  int remaining = static_cast<int>(data_.size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  memcpy(buf->data(), data_.data() + data_offset_, buf_size);
  data_offset_ += buf_size;
  *bytes_read = buf_size;
  return true;
}

void ViewHttpCacheJob::Core::OnIOComplete(int result) {
  DCHECK_EQ(net::OK, result);

  if (!user_callback_.is_null())
    user_callback_.Run();

  // We may be holding the last reference to this job. Do not access |this|
  // after Release().
  Release();  // Acquired on Start().
}

}  // namespace.

// Static.
bool ViewHttpCacheJobFactory::IsSupportedURL(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme) &&
         url.host() == kChromeUINetworkViewCacheHost;
}

// Static.
net::URLRequestJob* ViewHttpCacheJobFactory::CreateJobForRequest(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
  return new ViewHttpCacheJob(request, network_delegate);
}

}  // namespace content
