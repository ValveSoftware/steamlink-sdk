// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_token_service.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "net/url_request/url_request_context_getter.h"

int OAuth2TokenService::max_fetch_retry_num_ = 5;

OAuth2TokenService::RequestParameters::RequestParameters(
    const std::string& client_id,
    const std::string& account_id,
    const ScopeSet& scopes)
    : client_id(client_id),
      account_id(account_id),
      scopes(scopes) {
}

OAuth2TokenService::RequestParameters::~RequestParameters() {
}

bool OAuth2TokenService::RequestParameters::operator<(
    const RequestParameters& p) const {
  if (client_id < p.client_id)
    return true;
  else if (p.client_id < client_id)
    return false;

  if (account_id < p.account_id)
    return true;
  else if (p.account_id < account_id)
    return false;

  return scopes < p.scopes;
}

OAuth2TokenService::RequestImpl::RequestImpl(
    const std::string& account_id,
    OAuth2TokenService::Consumer* consumer)
    : account_id_(account_id),
      consumer_(consumer) {
}

OAuth2TokenService::RequestImpl::~RequestImpl() {
  DCHECK(CalledOnValidThread());
}

std::string OAuth2TokenService::RequestImpl::GetAccountId() const {
  return account_id_;
}

std::string OAuth2TokenService::RequestImpl::GetConsumerId() const {
  return consumer_->id();
}

void OAuth2TokenService::RequestImpl::InformConsumer(
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration_date) {
  DCHECK(CalledOnValidThread());
  if (error.state() == GoogleServiceAuthError::NONE)
    consumer_->OnGetTokenSuccess(this, access_token, expiration_date);
  else
    consumer_->OnGetTokenFailure(this, error);
}

// Class that fetches an OAuth2 access token for a given account id and set of
// scopes.
//
// It aims to meet OAuth2TokenService's requirements on token fetching. Retry
// mechanism is used to handle failures.
//
// To use this class, call CreateAndStart() to create and start a Fetcher.
//
// The Fetcher will call back the service by calling
// OAuth2TokenService::OnFetchComplete() when it completes fetching, if it is
// not destructed before it completes fetching; if the Fetcher is destructed
// before it completes fetching, the service will never be called back. The
// Fetcher destructs itself after calling back the service when finishes
// fetching.
//
// Requests that are waiting for the fetching results of this Fetcher can be
// added to the Fetcher by calling
// OAuth2TokenService::Fetcher::AddWaitingRequest() before the Fetcher
// completes fetching.
//
// The waiting requests are taken as weak pointers and they can be deleted.
// The waiting requests will be called back with fetching results if they are
// not deleted
// - when the Fetcher completes fetching, if the Fetcher is not destructed
//   before it completes fetching, or
// - when the Fetcher is destructed if the Fetcher is destructed before it
//   completes fetching (in this case, the waiting requests will be called
//   back with error).
class OAuth2TokenService::Fetcher : public OAuth2AccessTokenConsumer {
 public:
  // Creates a Fetcher and starts fetching an OAuth2 access token for
  // |account_id| and |scopes| in the request context obtained by |getter|.
  // The given |oauth2_token_service| will be informed when fetching is done.
  static Fetcher* CreateAndStart(OAuth2TokenService* oauth2_token_service,
                                 const std::string& account_id,
                                 net::URLRequestContextGetter* getter,
                                 const std::string& client_id,
                                 const std::string& client_secret,
                                 const ScopeSet& scopes,
                                 base::WeakPtr<RequestImpl> waiting_request);
  virtual ~Fetcher();

  // Add a request that is waiting for the result of this Fetcher.
  void AddWaitingRequest(base::WeakPtr<RequestImpl> waiting_request);

  // Returns count of waiting requests.
  size_t GetWaitingRequestCount() const;

  const std::vector<base::WeakPtr<RequestImpl> >& waiting_requests() const {
    return waiting_requests_;
  }

  void Cancel();

  const ScopeSet& GetScopeSet() const;
  const std::string& GetClientId() const;
  const std::string& GetAccountId() const;

  // The error result from this fetcher.
  const GoogleServiceAuthError& error() const { return error_; }

 protected:
   // OAuth2AccessTokenConsumer
  virtual void OnGetTokenSuccess(const std::string& access_token,
                                 const base::Time& expiration_date) OVERRIDE;
  virtual void OnGetTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

 private:
  Fetcher(OAuth2TokenService* oauth2_token_service,
          const std::string& account_id,
          net::URLRequestContextGetter* getter,
          const std::string& client_id,
          const std::string& client_secret,
          const OAuth2TokenService::ScopeSet& scopes,
          base::WeakPtr<RequestImpl> waiting_request);
  void Start();
  void InformWaitingRequests();
  void InformWaitingRequestsAndDelete();
  static bool ShouldRetry(const GoogleServiceAuthError& error);
  int64 ComputeExponentialBackOffMilliseconds(int retry_num);

  // |oauth2_token_service_| remains valid for the life of this Fetcher, since
  // this Fetcher is destructed in the dtor of the OAuth2TokenService or is
  // scheduled for deletion at the end of OnGetTokenFailure/OnGetTokenSuccess
  // (whichever comes first).
  OAuth2TokenService* const oauth2_token_service_;
  scoped_refptr<net::URLRequestContextGetter> getter_;
  const std::string account_id_;
  const ScopeSet scopes_;
  std::vector<base::WeakPtr<RequestImpl> > waiting_requests_;

  int retry_number_;
  base::OneShotTimer<Fetcher> retry_timer_;
  scoped_ptr<OAuth2AccessTokenFetcher> fetcher_;

  // Variables that store fetch results.
  // Initialized to be GoogleServiceAuthError::SERVICE_UNAVAILABLE to handle
  // destruction.
  GoogleServiceAuthError error_;
  std::string access_token_;
  base::Time expiration_date_;

  // OAuth2 client id and secret.
  std::string client_id_;
  std::string client_secret_;

  DISALLOW_COPY_AND_ASSIGN(Fetcher);
};

// static
OAuth2TokenService::Fetcher* OAuth2TokenService::Fetcher::CreateAndStart(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    base::WeakPtr<RequestImpl> waiting_request) {
  OAuth2TokenService::Fetcher* fetcher = new Fetcher(
      oauth2_token_service,
      account_id,
      getter,
      client_id,
      client_secret,
      scopes,
      waiting_request);
  fetcher->Start();
  return fetcher;
}

OAuth2TokenService::Fetcher::Fetcher(
    OAuth2TokenService* oauth2_token_service,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    base::WeakPtr<RequestImpl> waiting_request)
    : oauth2_token_service_(oauth2_token_service),
      getter_(getter),
      account_id_(account_id),
      scopes_(scopes),
      retry_number_(0),
      error_(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
      client_id_(client_id),
      client_secret_(client_secret) {
  DCHECK(oauth2_token_service_);
  DCHECK(getter_.get());
  waiting_requests_.push_back(waiting_request);
}

OAuth2TokenService::Fetcher::~Fetcher() {
  // Inform the waiting requests if it has not done so.
  if (waiting_requests_.size())
    InformWaitingRequests();
}

void OAuth2TokenService::Fetcher::Start() {
  fetcher_.reset(oauth2_token_service_->CreateAccessTokenFetcher(
      account_id_, getter_.get(), this));
  DCHECK(fetcher_);
  fetcher_->Start(client_id_,
                  client_secret_,
                  std::vector<std::string>(scopes_.begin(), scopes_.end()));
  retry_timer_.Stop();
}

void OAuth2TokenService::Fetcher::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_date) {
  fetcher_.reset();

  // Fetch completes.
  error_ = GoogleServiceAuthError::AuthErrorNone();
  access_token_ = access_token;
  expiration_date_ = expiration_date;

  // Subclasses may override this method to skip caching in some cases, but
  // we still inform all waiting Consumers of a successful token fetch below.
  // This is intentional -- some consumers may need the token for cleanup
  // tasks. https://chromiumcodereview.appspot.com/11312124/
  oauth2_token_service_->RegisterCacheEntry(client_id_,
                                            account_id_,
                                            scopes_,
                                            access_token_,
                                            expiration_date_);
  InformWaitingRequestsAndDelete();
}

void OAuth2TokenService::Fetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  fetcher_.reset();

  if (ShouldRetry(error) && retry_number_ < max_fetch_retry_num_) {
    base::TimeDelta backoff = base::TimeDelta::FromMilliseconds(
        ComputeExponentialBackOffMilliseconds(retry_number_));
    ++retry_number_;
    retry_timer_.Stop();
    retry_timer_.Start(FROM_HERE,
                       backoff,
                       this,
                       &OAuth2TokenService::Fetcher::Start);
    return;
  }

  error_ = error;
  InformWaitingRequestsAndDelete();
}

// Returns an exponential backoff in milliseconds including randomness less than
// 1000 ms when retrying fetching an OAuth2 access token.
int64 OAuth2TokenService::Fetcher::ComputeExponentialBackOffMilliseconds(
    int retry_num) {
  DCHECK(retry_num < max_fetch_retry_num_);
  int64 exponential_backoff_in_seconds = 1 << retry_num;
  // Returns a backoff with randomness < 1000ms
  return (exponential_backoff_in_seconds + base::RandDouble()) * 1000;
}

// static
bool OAuth2TokenService::Fetcher::ShouldRetry(
    const GoogleServiceAuthError& error) {
  GoogleServiceAuthError::State error_state = error.state();
  return error_state == GoogleServiceAuthError::CONNECTION_FAILED ||
         error_state == GoogleServiceAuthError::REQUEST_CANCELED ||
         error_state == GoogleServiceAuthError::SERVICE_UNAVAILABLE;
}

void OAuth2TokenService::Fetcher::InformWaitingRequests() {
  std::vector<base::WeakPtr<RequestImpl> >::const_iterator iter =
      waiting_requests_.begin();
  for (; iter != waiting_requests_.end(); ++iter) {
    base::WeakPtr<RequestImpl> waiting_request = *iter;
    if (waiting_request.get())
      waiting_request->InformConsumer(error_, access_token_, expiration_date_);
  }
  waiting_requests_.clear();
}

void OAuth2TokenService::Fetcher::InformWaitingRequestsAndDelete() {
  // Deregisters itself from the service to prevent more waiting requests to
  // be added when it calls back the waiting requests.
  oauth2_token_service_->OnFetchComplete(this);
  InformWaitingRequests();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void OAuth2TokenService::Fetcher::AddWaitingRequest(
    base::WeakPtr<OAuth2TokenService::RequestImpl> waiting_request) {
  waiting_requests_.push_back(waiting_request);
}

size_t OAuth2TokenService::Fetcher::GetWaitingRequestCount() const {
  return waiting_requests_.size();
}

void OAuth2TokenService::Fetcher::Cancel() {
  fetcher_.reset();
  retry_timer_.Stop();
  error_ = GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  InformWaitingRequestsAndDelete();
}

const OAuth2TokenService::ScopeSet& OAuth2TokenService::Fetcher::GetScopeSet()
    const {
  return scopes_;
}

const std::string& OAuth2TokenService::Fetcher::GetClientId() const {
  return client_id_;
}

const std::string& OAuth2TokenService::Fetcher::GetAccountId() const {
  return account_id_;
}

OAuth2TokenService::Request::Request() {
}

OAuth2TokenService::Request::~Request() {
}

OAuth2TokenService::Consumer::Consumer(const std::string& id)
    : id_(id) {}

OAuth2TokenService::Consumer::~Consumer() {
}

OAuth2TokenService::OAuth2TokenService() {
}

OAuth2TokenService::~OAuth2TokenService() {
  // Release all the pending fetchers.
  STLDeleteContainerPairSecondPointers(
      pending_fetchers_.begin(), pending_fetchers_.end());
}

void OAuth2TokenService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OAuth2TokenService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OAuth2TokenService::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void OAuth2TokenService::RemoveDiagnosticsObserver(
    DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

std::vector<std::string> OAuth2TokenService::GetAccounts() {
  return std::vector<std::string>();
}

scoped_ptr<OAuth2TokenService::Request> OAuth2TokenService::StartRequest(
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id,
      GetRequestContext(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      scopes,
      consumer);
}

scoped_ptr<OAuth2TokenService::Request>
OAuth2TokenService::StartRequestForClient(
    const std::string& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id,
      GetRequestContext(),
      client_id,
      client_secret,
      scopes,
      consumer);
}

scoped_ptr<OAuth2TokenService::Request>
OAuth2TokenService::StartRequestWithContext(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const ScopeSet& scopes,
    Consumer* consumer) {
  return StartRequestForClientWithContext(
      account_id,
      getter,
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      scopes,
      consumer);
}

scoped_ptr<OAuth2TokenService::Request>
OAuth2TokenService::StartRequestForClientWithContext(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes,
    Consumer* consumer) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<RequestImpl> request(new RequestImpl(account_id, consumer));
  FOR_EACH_OBSERVER(DiagnosticsObserver, diagnostics_observer_list_,
                    OnAccessTokenRequested(account_id,
                                           consumer->id(),
                                           scopes));

  if (!RefreshTokenIsAvailable(account_id)) {
    GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);

    FOR_EACH_OBSERVER(DiagnosticsObserver, diagnostics_observer_list_,
                      OnFetchAccessTokenComplete(
                          account_id, consumer->id(), scopes, error,
                          base::Time()));

    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &RequestImpl::InformConsumer,
        request->AsWeakPtr(),
        error,
        std::string(),
        base::Time()));
    return request.PassAs<Request>();
  }

  RequestParameters request_parameters(client_id,
                                       account_id,
                                       scopes);
  if (HasCacheEntry(request_parameters)) {
    StartCacheLookupRequest(request.get(), request_parameters, consumer);
  } else {
    FetchOAuth2Token(request.get(),
                     account_id,
                     getter,
                     client_id,
                     client_secret,
                     scopes);
  }
  return request.PassAs<Request>();
}

void OAuth2TokenService::FetchOAuth2Token(RequestImpl* request,
                                          const std::string& account_id,
                                          net::URLRequestContextGetter* getter,
                                          const std::string& client_id,
                                          const std::string& client_secret,
                                          const ScopeSet& scopes) {
  // If there is already a pending fetcher for |scopes| and |account_id|,
  // simply register this |request| for those results rather than starting
  // a new fetcher.
  RequestParameters request_parameters = RequestParameters(client_id,
                                                           account_id,
                                                           scopes);
  std::map<RequestParameters, Fetcher*>::iterator iter =
      pending_fetchers_.find(request_parameters);
  if (iter != pending_fetchers_.end()) {
    iter->second->AddWaitingRequest(request->AsWeakPtr());
    return;
  }

  pending_fetchers_[request_parameters] =
      Fetcher::CreateAndStart(this,
                              account_id,
                              getter,
                              client_id,
                              client_secret,
                              scopes,
                              request->AsWeakPtr());
}

void OAuth2TokenService::StartCacheLookupRequest(
    RequestImpl* request,
    const OAuth2TokenService::RequestParameters& request_parameters,
    OAuth2TokenService::Consumer* consumer) {
  CHECK(HasCacheEntry(request_parameters));
  const CacheEntry* cache_entry = GetCacheEntry(request_parameters);
  FOR_EACH_OBSERVER(DiagnosticsObserver, diagnostics_observer_list_,
                    OnFetchAccessTokenComplete(
                        request_parameters.account_id,
                        consumer->id(),
                        request_parameters.scopes,
                        GoogleServiceAuthError::AuthErrorNone(),
                        cache_entry->expiration_date));
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &RequestImpl::InformConsumer,
      request->AsWeakPtr(),
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      cache_entry->access_token,
      cache_entry->expiration_date));
}

void OAuth2TokenService::InvalidateToken(const std::string& account_id,
                                         const ScopeSet& scopes,
                                         const std::string& access_token) {
  InvalidateOAuth2Token(account_id,
                        GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                        scopes,
                        access_token);
}

void OAuth2TokenService::InvalidateTokenForClient(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  InvalidateOAuth2Token(account_id, client_id, scopes, access_token);
}

void OAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  DCHECK(CalledOnValidThread());
  RemoveCacheEntry(
      RequestParameters(client_id,
                        account_id,
                        scopes),
      access_token);
}

void OAuth2TokenService::OnFetchComplete(Fetcher* fetcher) {
  DCHECK(CalledOnValidThread());

  // Update the auth error state so auth errors are appropriately communicated
  // to the user.
  UpdateAuthError(fetcher->GetAccountId(), fetcher->error());

  // Note |fetcher| is recorded in |pending_fetcher_| mapped to its refresh
  // token and scope set. This is guaranteed as follows; here a Fetcher is said
  // to be uncompleted if it has not finished calling back
  // OAuth2TokenService::OnFetchComplete().
  //
  // (1) All the live Fetchers are created by this service.
  //     This is because (1) all the live Fetchers are created by a live
  //     service, as all the fetchers created by a service are destructed in the
  //     service's dtor.
  //
  // (2) All the uncompleted Fetchers created by this service are recorded in
  //     |pending_fetchers_|.
  //     This is because (1) all the created Fetchers are added to
  //     |pending_fetchers_| (in method StartRequest()) and (2) method
  //     OnFetchComplete() is the only place where a Fetcher is erased from
  //     |pending_fetchers_|. Note no Fetcher is erased in method
  //     StartRequest().
  //
  // (3) Each of the Fetchers recorded in |pending_fetchers_| is mapped to its
  //     refresh token and ScopeSet. This is guaranteed by Fetcher creation in
  //     method StartRequest().
  //
  // When this method is called, |fetcher| is alive and uncompleted.
  // By (1), |fetcher| is created by this service.
  // Then by (2), |fetcher| is recorded in |pending_fetchers_|.
  // Then by (3), |fetcher_| is mapped to its refresh token and ScopeSet.
  RequestParameters request_param(fetcher->GetClientId(),
                                  fetcher->GetAccountId(),
                                  fetcher->GetScopeSet());

  const OAuth2TokenService::CacheEntry* entry = GetCacheEntry(request_param);
  const std::vector<base::WeakPtr<RequestImpl> >& requests =
      fetcher->waiting_requests();
  for (size_t i = 0; i < requests.size(); ++i) {
    const RequestImpl* req = requests[i].get();
    if (req) {
      FOR_EACH_OBSERVER(DiagnosticsObserver, diagnostics_observer_list_,
                        OnFetchAccessTokenComplete(
                            req->GetAccountId(), req->GetConsumerId(),
                            fetcher->GetScopeSet(), fetcher->error(),
                            entry ? entry->expiration_date : base::Time()));
    }
  }

  std::map<RequestParameters, Fetcher*>::iterator iter =
    pending_fetchers_.find(request_param);
  DCHECK(iter != pending_fetchers_.end());
  DCHECK_EQ(fetcher, iter->second);
  pending_fetchers_.erase(iter);
}

bool OAuth2TokenService::HasCacheEntry(
    const RequestParameters& request_parameters) {
  const CacheEntry* cache_entry = GetCacheEntry(request_parameters);
  return cache_entry && cache_entry->access_token.length();
}

const OAuth2TokenService::CacheEntry* OAuth2TokenService::GetCacheEntry(
    const RequestParameters& request_parameters) {
  DCHECK(CalledOnValidThread());
  TokenCache::iterator token_iterator = token_cache_.find(request_parameters);
  if (token_iterator == token_cache_.end())
    return NULL;
  if (token_iterator->second.expiration_date <= base::Time::Now()) {
    token_cache_.erase(token_iterator);
    return NULL;
  }
  return &token_iterator->second;
}

bool OAuth2TokenService::RemoveCacheEntry(
    const RequestParameters& request_parameters,
    const std::string& token_to_remove) {
  DCHECK(CalledOnValidThread());
  TokenCache::iterator token_iterator = token_cache_.find(request_parameters);
  if (token_iterator != token_cache_.end() &&
      token_iterator->second.access_token == token_to_remove) {
    FOR_EACH_OBSERVER(DiagnosticsObserver, diagnostics_observer_list_,
                      OnTokenRemoved(request_parameters.account_id,
                                     request_parameters.scopes));
    token_cache_.erase(token_iterator);
    return true;
  }
  return false;
}

void OAuth2TokenService::RegisterCacheEntry(
    const std::string& client_id,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token,
    const base::Time& expiration_date) {
  DCHECK(CalledOnValidThread());

  CacheEntry& token = token_cache_[RequestParameters(client_id,
                                                     account_id,
                                                     scopes)];
  token.access_token = access_token;
  token.expiration_date = expiration_date;
}

void OAuth2TokenService::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // Default implementation does nothing.
}

void OAuth2TokenService::ClearCache() {
  DCHECK(CalledOnValidThread());
  for (TokenCache::iterator iter = token_cache_.begin();
       iter != token_cache_.end(); ++iter) {
    FOR_EACH_OBSERVER(DiagnosticsObserver, diagnostics_observer_list_,
                      OnTokenRemoved(iter->first.account_id,
                                     iter->first.scopes));
  }

  token_cache_.clear();
}

void OAuth2TokenService::ClearCacheForAccount(const std::string& account_id) {
  DCHECK(CalledOnValidThread());
  for (TokenCache::iterator iter = token_cache_.begin();
       iter != token_cache_.end();
       /* iter incremented in body */) {
    if (iter->first.account_id == account_id) {
      FOR_EACH_OBSERVER(DiagnosticsObserver, diagnostics_observer_list_,
                        OnTokenRemoved(account_id, iter->first.scopes));
      token_cache_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

void OAuth2TokenService::CancelAllRequests() {
  std::vector<Fetcher*> fetchers_to_cancel;
  for (std::map<RequestParameters, Fetcher*>::iterator iter =
           pending_fetchers_.begin();
       iter != pending_fetchers_.end();
       ++iter) {
    fetchers_to_cancel.push_back(iter->second);
  }
  CancelFetchers(fetchers_to_cancel);
}

void OAuth2TokenService::CancelRequestsForAccount(
    const std::string& account_id) {
  std::vector<Fetcher*> fetchers_to_cancel;
  for (std::map<RequestParameters, Fetcher*>::iterator iter =
           pending_fetchers_.begin();
       iter != pending_fetchers_.end();
       ++iter) {
    if (iter->first.account_id == account_id)
      fetchers_to_cancel.push_back(iter->second);
  }
  CancelFetchers(fetchers_to_cancel);
}

void OAuth2TokenService::CancelFetchers(
    std::vector<Fetcher*> fetchers_to_cancel) {
  for (std::vector<OAuth2TokenService::Fetcher*>::iterator iter =
           fetchers_to_cancel.begin();
       iter != fetchers_to_cancel.end();
       ++iter) {
    (*iter)->Cancel();
  }
}

void OAuth2TokenService::FireRefreshTokenAvailable(
    const std::string& account_id) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnRefreshTokenAvailable(account_id));
}

void OAuth2TokenService::FireRefreshTokenRevoked(
    const std::string& account_id) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnRefreshTokenRevoked(account_id));
}

void OAuth2TokenService::FireRefreshTokensLoaded() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnRefreshTokensLoaded());
}

int OAuth2TokenService::cache_size_for_testing() const {
  return token_cache_.size();
}

void OAuth2TokenService::set_max_authorization_token_fetch_retries_for_testing(
    int max_retries) {
  DCHECK(CalledOnValidThread());
  max_fetch_retry_num_ = max_retries;
}

size_t OAuth2TokenService::GetNumPendingRequestsForTesting(
    const std::string& client_id,
    const std::string& account_id,
    const ScopeSet& scopes) const {
  PendingFetcherMap::const_iterator iter = pending_fetchers_.find(
      OAuth2TokenService::RequestParameters(
          client_id,
          account_id,
          scopes));
  return iter == pending_fetchers_.end() ?
             0 : iter->second->GetWaitingRequestCount();
}
