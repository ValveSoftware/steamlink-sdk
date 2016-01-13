// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_token_service_request.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"

OAuth2TokenServiceRequest::TokenServiceProvider::TokenServiceProvider() {
}

OAuth2TokenServiceRequest::TokenServiceProvider::~TokenServiceProvider() {
}

// Core serves as the base class for OAuth2TokenService operations.  Each
// operation should be modeled as a derived type.
//
// Core is used like this:
//
// 1. Constructed on owner thread.
//
// 2. Start() is called on owner thread, which calls StartOnTokenServiceThread()
// on token service thread.
//
// 3. Request is executed.
//
// 4. Stop() is called on owner thread, which calls StopOnTokenServiceThread()
// on token service thread.
//
// 5. Core is destroyed on owner thread.
class OAuth2TokenServiceRequest::Core
    : public base::NonThreadSafe,
      public base::RefCountedThreadSafe<OAuth2TokenServiceRequest::Core> {
 public:
  // Note the thread where an instance of Core is constructed is referred to as
  // the "owner thread" here.
  Core(OAuth2TokenServiceRequest* owner, TokenServiceProvider* provider);

  // Starts the core.  Must be called on the owner thread.
  void Start();

  // Stops the core.  Must be called on the owner thread.
  void Stop();

  // Returns true if this object has been stopped.  Must be called on the owner
  // thread.
  bool IsStopped() const;

 protected:
  // Core must be destroyed on the owner thread.  If data members must be
  // cleaned up or destroyed on the token service thread, do so in the
  // StopOnTokenServiceThread method.
  virtual ~Core();

  // Called on the token service thread.
  virtual void StartOnTokenServiceThread() = 0;

  // Called on the token service thread.
  virtual void StopOnTokenServiceThread() = 0;

  base::SingleThreadTaskRunner* token_service_task_runner();
  OAuth2TokenService* token_service();
  OAuth2TokenServiceRequest* owner();

 private:
  friend class base::RefCountedThreadSafe<OAuth2TokenServiceRequest::Core>;

  void DoNothing();

  scoped_refptr<base::SingleThreadTaskRunner> token_service_task_runner_;
  OAuth2TokenServiceRequest* owner_;
  TokenServiceProvider* provider_;
  DISALLOW_COPY_AND_ASSIGN(Core);
};

OAuth2TokenServiceRequest::Core::Core(OAuth2TokenServiceRequest* owner,
                                      TokenServiceProvider* provider)
    : owner_(owner), provider_(provider) {
  DCHECK(owner_);
  DCHECK(provider_);
  token_service_task_runner_ = provider_->GetTokenServiceTaskRunner();
  DCHECK(token_service_task_runner_);
}

OAuth2TokenServiceRequest::Core::~Core() {
}

void OAuth2TokenServiceRequest::Core::Start() {
  DCHECK(CalledOnValidThread());
  token_service_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OAuth2TokenServiceRequest::Core::StartOnTokenServiceThread,
                 this));
}

void OAuth2TokenServiceRequest::Core::Stop() {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsStopped());

  // Detaches |owner_| from this instance so |owner_| will be called back only
  // if |Stop()| has never been called.
  owner_ = NULL;

  // We are stopping and will likely be destroyed soon.  Use a reply closure
  // (DoNothing) to retain "this" and ensure we are destroyed in the owner
  // thread, not the task runner thread.  PostTaskAndReply guarantees that the
  // reply closure will execute after StopOnTokenServiceThread has completed.
  token_service_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&OAuth2TokenServiceRequest::Core::StopOnTokenServiceThread,
                 this),
      base::Bind(&OAuth2TokenServiceRequest::Core::DoNothing, this));
}

bool OAuth2TokenServiceRequest::Core::IsStopped() const {
  DCHECK(CalledOnValidThread());
  return owner_ == NULL;
}

base::SingleThreadTaskRunner*
OAuth2TokenServiceRequest::Core::token_service_task_runner() {
  return token_service_task_runner_;
}

OAuth2TokenService* OAuth2TokenServiceRequest::Core::token_service() {
  DCHECK(token_service_task_runner_->BelongsToCurrentThread());
  return provider_->GetTokenService();
}

OAuth2TokenServiceRequest* OAuth2TokenServiceRequest::Core::owner() {
  DCHECK(CalledOnValidThread());
  return owner_;
}

void OAuth2TokenServiceRequest::Core::DoNothing() {
  DCHECK(CalledOnValidThread());
}

namespace {

// An implementation of Core for getting an access token.
class RequestCore : public OAuth2TokenServiceRequest::Core,
                    public OAuth2TokenService::Consumer {
 public:
  RequestCore(OAuth2TokenServiceRequest* owner,
              OAuth2TokenServiceRequest::TokenServiceProvider* provider,
              OAuth2TokenService::Consumer* consumer,
              const std::string& account_id,
              const OAuth2TokenService::ScopeSet& scopes);

  // OAuth2TokenService::Consumer.  Must be called on the token service thread.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<RequestCore>;

  // Must be destroyed on the owner thread.
  virtual ~RequestCore();

  // Core implementation.
  virtual void StartOnTokenServiceThread() OVERRIDE;
  virtual void StopOnTokenServiceThread() OVERRIDE;

  void InformOwnerOnGetTokenSuccess(std::string access_token,
                                    base::Time expiration_time);
  void InformOwnerOnGetTokenFailure(GoogleServiceAuthError error);

  scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner_;
  OAuth2TokenService::Consumer* const consumer_;
  std::string account_id_;
  OAuth2TokenService::ScopeSet scopes_;

  // OAuth2TokenService request for fetching OAuth2 access token; it should be
  // created, reset and accessed only on the token service thread.
  scoped_ptr<OAuth2TokenService::Request> request_;

  DISALLOW_COPY_AND_ASSIGN(RequestCore);
};

RequestCore::RequestCore(
    OAuth2TokenServiceRequest* owner,
    OAuth2TokenServiceRequest::TokenServiceProvider* provider,
    OAuth2TokenService::Consumer* consumer,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes)
    : OAuth2TokenServiceRequest::Core(owner, provider),
      OAuth2TokenService::Consumer("oauth2_token_service"),
      owner_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      consumer_(consumer),
      account_id_(account_id),
      scopes_(scopes) {
  DCHECK(consumer_);
  DCHECK(!account_id_.empty());
  DCHECK(!scopes_.empty());
}

RequestCore::~RequestCore() {
}

void RequestCore::StartOnTokenServiceThread() {
  DCHECK(token_service_task_runner()->BelongsToCurrentThread());
  request_ = token_service()->StartRequest(account_id_, scopes_, this).Pass();
}

void RequestCore::StopOnTokenServiceThread() {
  DCHECK(token_service_task_runner()->BelongsToCurrentThread());
  request_.reset();
}

void RequestCore::OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                    const std::string& access_token,
                                    const base::Time& expiration_time) {
  DCHECK(token_service_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(request_.get(), request);
  owner_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestCore::InformOwnerOnGetTokenSuccess,
                 this,
                 access_token,
                 expiration_time));
  request_.reset();
}

void RequestCore::OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                    const GoogleServiceAuthError& error) {
  DCHECK(token_service_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(request_.get(), request);
  owner_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestCore::InformOwnerOnGetTokenFailure, this, error));
  request_.reset();
}

void RequestCore::InformOwnerOnGetTokenSuccess(std::string access_token,
                                               base::Time expiration_time) {
  DCHECK(CalledOnValidThread());
  if (!IsStopped()) {
    consumer_->OnGetTokenSuccess(owner(), access_token, expiration_time);
  }
}

void RequestCore::InformOwnerOnGetTokenFailure(GoogleServiceAuthError error) {
  DCHECK(CalledOnValidThread());
  if (!IsStopped()) {
    consumer_->OnGetTokenFailure(owner(), error);
  }
}

// An implementation of Core for invalidating an access token.
class InvalidateCore : public OAuth2TokenServiceRequest::Core {
 public:
  InvalidateCore(OAuth2TokenServiceRequest* owner,
                 OAuth2TokenServiceRequest::TokenServiceProvider* provider,
                 const std::string& access_token,
                 const std::string& account_id,
                 const OAuth2TokenService::ScopeSet& scopes);

 private:
  friend class base::RefCountedThreadSafe<InvalidateCore>;

  // Must be destroyed on the owner thread.
  virtual ~InvalidateCore();

  // Core implementation.
  virtual void StartOnTokenServiceThread() OVERRIDE;
  virtual void StopOnTokenServiceThread() OVERRIDE;

  std::string access_token_;
  std::string account_id_;
  OAuth2TokenService::ScopeSet scopes_;

  DISALLOW_COPY_AND_ASSIGN(InvalidateCore);
};

InvalidateCore::InvalidateCore(
    OAuth2TokenServiceRequest* owner,
    OAuth2TokenServiceRequest::TokenServiceProvider* provider,
    const std::string& access_token,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes)
    : OAuth2TokenServiceRequest::Core(owner, provider),
      access_token_(access_token),
      account_id_(account_id),
      scopes_(scopes) {
  DCHECK(!access_token_.empty());
  DCHECK(!account_id_.empty());
  DCHECK(!scopes.empty());
}

InvalidateCore::~InvalidateCore() {
}

void InvalidateCore::StartOnTokenServiceThread() {
  DCHECK(token_service_task_runner()->BelongsToCurrentThread());
  token_service()->InvalidateToken(account_id_, scopes_, access_token_);
}

void InvalidateCore::StopOnTokenServiceThread() {
  DCHECK(token_service_task_runner()->BelongsToCurrentThread());
  // Nothing to do.
}

}  // namespace

// static
scoped_ptr<OAuth2TokenServiceRequest> OAuth2TokenServiceRequest::CreateAndStart(
    TokenServiceProvider* provider,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  scoped_ptr<OAuth2TokenServiceRequest> request(
      new OAuth2TokenServiceRequest(account_id));
  scoped_refptr<Core> core(
      new RequestCore(request.get(), provider, consumer, account_id, scopes));
  request->StartWithCore(core);
  return request.Pass();
}

// static
void OAuth2TokenServiceRequest::InvalidateToken(
    OAuth2TokenServiceRequest::TokenServiceProvider* provider,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  scoped_ptr<OAuth2TokenServiceRequest> request(
      new OAuth2TokenServiceRequest(account_id));
  scoped_refptr<Core> core(new InvalidateCore(
      request.get(), provider, access_token, account_id, scopes));
  request->StartWithCore(core);
}

OAuth2TokenServiceRequest::~OAuth2TokenServiceRequest() {
  core_->Stop();
}

std::string OAuth2TokenServiceRequest::GetAccountId() const {
  return account_id_;
}

OAuth2TokenServiceRequest::OAuth2TokenServiceRequest(
    const std::string& account_id)
    : account_id_(account_id) {
  DCHECK(!account_id_.empty());
}

void OAuth2TokenServiceRequest::StartWithCore(const scoped_refptr<Core>& core) {
  DCHECK(core);
  core_ = core;
  core_->Start();
}
