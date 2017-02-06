// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/cast_cdm_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"

namespace chromecast {
namespace media {

namespace {

// media::CdmPromiseTemplate implementation that wraps a promise so as to
// allow passing to other threads.
template <typename... T>
class CdmPromiseInternal : public ::media::CdmPromiseTemplate<T...> {
 public:
  CdmPromiseInternal(std::unique_ptr<::media::CdmPromiseTemplate<T...>> promise)
      : task_runner_(base::ThreadTaskRunnerHandle::Get()),
        promise_(std::move(promise)) {}

  ~CdmPromiseInternal() final {
    if (IsPromiseSettled())
      return;

    DCHECK(promise_);
    RejectPromiseOnDestruction();
  }

  // CdmPromiseTemplate<> implementation.
  void resolve(const T&... result) final;

  void reject(::media::MediaKeys::Exception exception,
              uint32_t system_code,
              const std::string& error_message) final {
    MarkPromiseSettled();
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&::media::CdmPromiseTemplate<T...>::reject,
                              base::Owned(promise_.release()), exception,
                              system_code, error_message));
  }

 private:
  using ::media::CdmPromiseTemplate<T...>::IsPromiseSettled;
  using ::media::CdmPromiseTemplate<T...>::MarkPromiseSettled;
  using ::media::CdmPromiseTemplate<T...>::RejectPromiseOnDestruction;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<::media::CdmPromiseTemplate<T...>> promise_;
};

template <typename... T>
void CdmPromiseInternal<T...>::resolve(const T&... result) {
  MarkPromiseSettled();
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&::media::CdmPromiseTemplate<T...>::resolve,
                            base::Owned(promise_.release()), result...));
}

template <typename... T>
std::unique_ptr<CdmPromiseInternal<T...>> BindPromiseToCurrentLoop(
    std::unique_ptr<::media::CdmPromiseTemplate<T...>> promise) {
  return base::WrapUnique(new CdmPromiseInternal<T...>(std::move(promise)));
}

}  // namespace

// A macro runs current member function on |task_runner_| thread.
#define FORWARD_ON_CDM_THREAD(param_fn, ...)    \
  task_runner_->PostTask(                       \
      FROM_HERE, base::Bind(&CastCdm::param_fn, \
                            base::Unretained(cast_cdm_.get()), ##__VA_ARGS__))

CastCdmProxy::CastCdmProxy(
    const scoped_refptr<CastCdm>& cast_cdm,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : cast_cdm_(cast_cdm), task_runner_(task_runner) {}

CastCdmProxy::~CastCdmProxy() {
  DCHECK(thread_checker_.CalledOnValidThread());
  cast_cdm_->AddRef();
  CastCdm* raw_cdm = cast_cdm_.get();
  cast_cdm_ = nullptr;
  task_runner_->ReleaseSoon(FROM_HERE, raw_cdm);
}

CastCdm* CastCdmProxy::cast_cdm() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return cast_cdm_.get();
}

void CastCdmProxy::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      SetServerCertificate, certificate,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void CastCdmProxy::CreateSessionAndGenerateRequest(
    ::media::MediaKeys::SessionType session_type,
    ::media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      CreateSessionAndGenerateRequest, session_type, init_data_type, init_data,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void CastCdmProxy::LoadSession(
    ::media::MediaKeys::SessionType session_type,
    const std::string& session_id,
    std::unique_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      LoadSession, session_type, session_id,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void CastCdmProxy::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      UpdateSession, session_id, response,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void CastCdmProxy::CloseSession(
    const std::string& session_id,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      CloseSession, session_id,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void CastCdmProxy::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      RemoveSession, session_id,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

::media::CdmContext* CastCdmProxy::GetCdmContext() {
  // This will be recast as a CastCdmService pointer before being passed to the
  // media pipeline. The returned object should only be called on the CMA
  // renderer thread.
  return cast_cdm_->GetCdmContext();
}

}  // namespace media
}  // namespace chromecast
