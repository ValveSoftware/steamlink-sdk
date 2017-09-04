// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/view_http_cache_job_factory.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
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
        callback_(base::Bind(&ViewHttpCacheJob::OnStartCompleted,
                             base::Unretained(this))),
        weak_factory_(this) {
  }

  // net::URLRequestJob implementation.
  void Start() override;
  void Kill() override;
  bool GetMimeType(std::string* mime_type) const override {
    return core_->GetMimeType(mime_type);
  }
  bool GetCharset(std::string* charset) override {
    return core_->GetCharset(charset);
  }
  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    return core_->ReadRawData(buf, buf_size);
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
    int ReadRawData(net::IOBuffer* buf, int buf_size);

   private:
    friend class base::RefCounted<Core>;

    ~Core() {}

    // Called when ViewCacheHelper completes the operation.
    void OnIOComplete(int result);

    std::string data_;
    size_t data_offset_;
    net::ViewCacheHelper cache_helper_;
    net::CompletionCallback callback_;
    base::Closure user_callback_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  ~ViewHttpCacheJob() override {}

  void StartAsync();
  void OnStartCompleted();

  scoped_refptr<Core> core_;
  base::Closure callback_;

  base::WeakPtrFactory<ViewHttpCacheJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewHttpCacheJob);
};

void ViewHttpCacheJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

int ViewHttpCacheJob::Core::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK_LE(data_offset_, data_.size());
  int remaining = base::checked_cast<int>(data_.size() - data_offset_);
  if (buf_size > remaining)
    buf_size = remaining;
  memcpy(buf->data(), data_.data() + data_offset_, buf_size);
  data_offset_ += buf_size;
  return buf_size;
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
         url.host_piece() == kChromeUINetworkViewCacheHost;
}

// Static.
net::URLRequestJob* ViewHttpCacheJobFactory::CreateJobForRequest(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
  return new ViewHttpCacheJob(request, network_delegate);
}

}  // namespace content
