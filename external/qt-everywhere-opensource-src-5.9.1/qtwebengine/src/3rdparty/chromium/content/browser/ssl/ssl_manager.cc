// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include <set>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

const char kSSLManagerKeyName[] = "content_ssl_manager";

// Events for UMA. Do not reorder or change!
enum SSLGoodCertSeenEvent {
  NO_PREVIOUS_EXCEPTION = 0,
  HAD_PREVIOUS_EXCEPTION = 1,
  SSL_GOOD_CERT_SEEN_EVENT_MAX = 2
};

void OnAllowCertificate(SSLErrorHandler* handler,
                        SSLHostStateDelegate* state_delegate,
                        CertificateRequestResultType decision) {
  DCHECK(handler->ssl_info().is_valid());
  switch (decision) {
    case CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE:
      // Note that we should not call SetMaxSecurityStyle here, because
      // the active NavigationEntry has just been deleted (in
      // HideInterstitialPage) and the new NavigationEntry will not be
      // set until DidNavigate.  This is ok, because the new
      // NavigationEntry will have its max security style set within
      // DidNavigate.
      //
      // While AllowCert() executes synchronously on this thread,
      // ContinueRequest() gets posted to a different thread. Calling
      // AllowCert() first ensures deterministic ordering.
      if (state_delegate) {
        state_delegate->AllowCert(handler->request_url().host(),
                                  *handler->ssl_info().cert.get(),
                                  handler->cert_error());
      }
      handler->ContinueRequest();
      return;
    case CERTIFICATE_REQUEST_RESULT_TYPE_DENY:
      handler->DenyRequest();
      return;
    case CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL:
      handler->CancelRequest();
      return;
  }
}

class SSLManagerSet : public base::SupportsUserData::Data {
 public:
  SSLManagerSet() {
  }

  std::set<SSLManager*>& get() { return set_; }

 private:
  std::set<SSLManager*> set_;

  DISALLOW_COPY_AND_ASSIGN(SSLManagerSet);
};

void HandleSSLErrorOnUI(
    const base::Callback<WebContents*(void)>& web_contents_getter,
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    const ResourceType resource_type,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  content::WebContents* web_contents = web_contents_getter.Run();
  std::unique_ptr<SSLErrorHandler> handler(new SSLErrorHandler(
      web_contents, delegate, resource_type, url, ssl_info, fatal));

  if (!web_contents) {
    // Requests can fail to dispatch because they don't have a WebContents. See
    // https://crbug.com/86537. In this case we have to make a decision in this
    // function, so we ignore revocation check failures.
    if (net::IsCertStatusMinorError(ssl_info.cert_status)) {
      handler->ContinueRequest();
    } else {
      handler->CancelRequest();
    }
    return;
  }

  SSLManager* manager =
      static_cast<NavigationControllerImpl*>(&web_contents->GetController())
          ->ssl_manager();
  manager->OnCertError(std::move(handler));
}

}  // namespace

// static
void SSLManager::OnSSLCertificateError(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    const ResourceType resource_type,
    const GURL& url,
    const base::Callback<WebContents*(void)>& web_contents_getter,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(delegate.get());
  DVLOG(1) << "OnSSLCertificateError() cert_error: "
           << net::MapCertStatusToNetError(ssl_info.cert_status)
           << " resource_type: " << resource_type
           << " url: " << url.spec()
           << " cert_status: " << std::hex << ssl_info.cert_status;

  // A certificate error occurred. Construct a SSLErrorHandler object
  // on the UI thread for processing.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&HandleSSLErrorOnUI, web_contents_getter, delegate,
                 resource_type, url, ssl_info, fatal));
}

// static
void SSLManager::OnSSLCertificateSubresourceError(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    const GURL& url,
    int render_process_id,
    int render_frame_id,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  OnSSLCertificateError(delegate, RESOURCE_TYPE_SUB_RESOURCE, url,
                        base::Bind(&WebContentsImpl::FromRenderFrameHostID,
                                   render_process_id, render_frame_id),
                        ssl_info, fatal);
}

SSLManager::SSLManager(NavigationControllerImpl* controller)
    : controller_(controller),
      ssl_host_state_delegate_(
          controller->GetBrowserContext()->GetSSLHostStateDelegate()) {
  DCHECK(controller_);

  SSLManagerSet* managers = static_cast<SSLManagerSet*>(
      controller_->GetBrowserContext()->GetUserData(kSSLManagerKeyName));
  if (!managers) {
    managers = new SSLManagerSet;
    controller_->GetBrowserContext()->SetUserData(kSSLManagerKeyName, managers);
  }
  managers->get().insert(this);
}

SSLManager::~SSLManager() {
  SSLManagerSet* managers = static_cast<SSLManagerSet*>(
      controller_->GetBrowserContext()->GetUserData(kSSLManagerKeyName));
  managers->get().erase(this);
}

void SSLManager::DidCommitProvisionalLoad(const LoadCommittedDetails& details) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  int content_status_flags = 0;
  if (!details.is_main_frame) {
    // If it wasn't a main-frame navigation, then carry over content
    // status flags. (For example, the mixed content flag shouldn't
    // clear because of a frame navigation.)
    NavigationEntryImpl* previous_entry =
        controller_->GetEntryAtIndex(details.previous_entry_index);
    if (previous_entry) {
      content_status_flags = previous_entry->GetSSL().content_status;
    }
  }
  UpdateEntry(entry, content_status_flags, 0);
  // Always notify the WebContents that the SSL state changed when a
  // load is committed, in case the active navigation entry has changed.
  NotifyDidChangeVisibleSSLState();
}

void SSLManager::DidDisplayMixedContent() {
  UpdateLastCommittedEntry(SSLStatus::DISPLAYED_INSECURE_CONTENT, 0);
}

void SSLManager::DidDisplayContentWithCertErrors() {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;
  // Only record information about subresources with cert errors if the
  // main page is HTTPS with a certificate.
  if (entry->GetURL().SchemeIsCryptographic() && entry->GetSSL().certificate) {
    UpdateLastCommittedEntry(SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS, 0);
  }
}

void SSLManager::DidShowPasswordInputOnHttp() {
  UpdateLastCommittedEntry(SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP, 0);
}

void SSLManager::DidHideAllPasswordInputsOnHttp() {
  UpdateLastCommittedEntry(0, SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}

void SSLManager::DidShowCreditCardInputOnHttp() {
  UpdateLastCommittedEntry(SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP, 0);
}

void SSLManager::DidRunMixedContent(const GURL& security_origin) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
    return;

  if (ssl_host_state_delegate_) {
    ssl_host_state_delegate_->HostRanInsecureContent(
        security_origin.host(), site_instance->GetProcess()->GetID(),
        SSLHostStateDelegate::MIXED_CONTENT);
  }
  UpdateEntry(entry, 0, 0);
  NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

void SSLManager::DidRunContentWithCertErrors(const GURL& security_origin) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
    return;

  if (ssl_host_state_delegate_) {
    ssl_host_state_delegate_->HostRanInsecureContent(
        security_origin.host(), site_instance->GetProcess()->GetID(),
        SSLHostStateDelegate::CERT_ERRORS_CONTENT);
  }
  UpdateEntry(entry, 0, 0);
  NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

void SSLManager::OnCertError(std::unique_ptr<SSLErrorHandler> handler) {
  bool expired_previous_decision = false;
  // First we check if we know the policy for this error.
  DCHECK(handler->ssl_info().is_valid());
  SSLHostStateDelegate::CertJudgment judgment =
      ssl_host_state_delegate_
          ? ssl_host_state_delegate_->QueryPolicy(
                handler->request_url().host(), *handler->ssl_info().cert.get(),
                handler->cert_error(), &expired_previous_decision)
          : SSLHostStateDelegate::DENIED;

  if (judgment == SSLHostStateDelegate::ALLOWED) {
    handler->ContinueRequest();
    return;
  }

  // For all other hosts, which must be DENIED, a blocking page is shown to the
  // user every time they come back to the page.
  int options_mask = 0;
  switch (handler->cert_error()) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_WEAK_KEY:
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
    case net::ERR_CERT_VALIDITY_TOO_LONG:
    case net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
      if (!handler->fatal())
        options_mask |= OVERRIDABLE;
      else
        options_mask |= STRICT_ENFORCEMENT;
      if (expired_previous_decision)
        options_mask |= EXPIRED_PREVIOUS_DECISION;
      OnCertErrorInternal(std::move(handler), options_mask);
      break;
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
      // Ignore this error.
      handler->ContinueRequest();
      break;
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      // We ignore this error but will show a warning status in the location
      // bar.
      handler->ContinueRequest();
      break;
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
    case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      if (handler->fatal())
        options_mask |= STRICT_ENFORCEMENT;
      if (expired_previous_decision)
        options_mask |= EXPIRED_PREVIOUS_DECISION;
      OnCertErrorInternal(std::move(handler), options_mask);
      break;
    default:
      NOTREACHED();
      handler->CancelRequest();
      break;
  }
}

void SSLManager::DidStartResourceResponse(const GURL& url,
                                          bool has_certificate,
                                          net::CertStatus ssl_cert_status) {
  if (has_certificate && url.SchemeIsCryptographic() &&
      !net::IsCertStatusError(ssl_cert_status)) {
    // If the scheme is https: or wss: *and* the security info for the
    // cert has been set (i.e. the cert id is not 0) and the cert did
    // not have any errors, revoke any previous decisions that
    // have occurred. If the cert info has not been set, do nothing since it
    // isn't known if the connection was actually a valid connection or if it
    // had a cert error.
    SSLGoodCertSeenEvent event = NO_PREVIOUS_EXCEPTION;
    if (ssl_host_state_delegate_ &&
        ssl_host_state_delegate_->HasAllowException(url.host())) {
      // If there's no certificate error, a good certificate has been seen, so
      // clear out any exceptions that were made by the user for bad
      // certificates. This intentionally does not apply to cached resources
      // (see https://crbug.com/634553 for an explanation).
      ssl_host_state_delegate_->RevokeUserAllowExceptions(url.host());
      event = HAD_PREVIOUS_EXCEPTION;
    }
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.good_cert_seen", event,
                              SSL_GOOD_CERT_SEEN_EVENT_MAX);
  }
}

void SSLManager::OnCertErrorInternal(std::unique_ptr<SSLErrorHandler> handler,
                                     int options_mask) {
  bool overridable = (options_mask & OVERRIDABLE) != 0;
  bool strict_enforcement = (options_mask & STRICT_ENFORCEMENT) != 0;
  bool expired_previous_decision =
      (options_mask & EXPIRED_PREVIOUS_DECISION) != 0;

  WebContents* web_contents = handler->web_contents();
  int cert_error = handler->cert_error();
  const net::SSLInfo& ssl_info = handler->ssl_info();
  const GURL& request_url = handler->request_url();
  ResourceType resource_type = handler->resource_type();
  GetContentClient()->browser()->AllowCertificateError(
      web_contents, cert_error, ssl_info, request_url, resource_type,
      overridable, strict_enforcement, expired_previous_decision,
      base::Bind(&OnAllowCertificate, base::Owned(handler.release()),
                 ssl_host_state_delegate_));
}

void SSLManager::UpdateEntry(NavigationEntryImpl* entry,
                             int add_content_status_flags,
                             int remove_content_status_flags) {
  // We don't always have a navigation entry to update, for example in the
  // case of the Web Inspector.
  if (!entry)
    return;

  SSLStatus original_ssl_status = entry->GetSSL();  // Copy!
  entry->GetSSL().initialized = true;
  entry->GetSSL().content_status |= add_content_status_flags;
  entry->GetSSL().content_status &= ~remove_content_status_flags;

  SiteInstance* site_instance = entry->site_instance();
  // Note that |site_instance| can be NULL here because NavigationEntries don't
  // necessarily have site instances.  Without a process, the entry can't
  // possibly have insecure content.  See bug https://crbug.com/12423.
  if (site_instance && ssl_host_state_delegate_) {
    std::string host = entry->GetURL().host();
    int process_id = site_instance->GetProcess()->GetID();
    if (ssl_host_state_delegate_->DidHostRunInsecureContent(
            host, process_id, SSLHostStateDelegate::MIXED_CONTENT)) {
      entry->GetSSL().content_status |= SSLStatus::RAN_INSECURE_CONTENT;
    }

    // Only record information about subresources with cert errors if the
    // main page is HTTPS with a certificate.
    if (entry->GetURL().SchemeIsCryptographic() &&
        entry->GetSSL().certificate &&
        ssl_host_state_delegate_->DidHostRunInsecureContent(
            host, process_id, SSLHostStateDelegate::CERT_ERRORS_CONTENT)) {
      entry->GetSSL().content_status |= SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS;
    }
  }

  if (!entry->GetSSL().Equals(original_ssl_status))
    NotifyDidChangeVisibleSSLState();
}

void SSLManager::UpdateLastCommittedEntry(int add_content_status_flags,
                                          int remove_content_status_flags) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();
  if (!entry)
    return;
  UpdateEntry(entry, add_content_status_flags, remove_content_status_flags);
}

void SSLManager::NotifyDidChangeVisibleSSLState() {
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(controller_->delegate()->GetWebContents());
  contents->DidChangeVisibleSecurityState();
}

// static
void SSLManager::NotifySSLInternalStateChanged(BrowserContext* context) {
  SSLManagerSet* managers =
      static_cast<SSLManagerSet*>(context->GetUserData(kSSLManagerKeyName));

  for (std::set<SSLManager*>::iterator i = managers->get().begin();
       i != managers->get().end(); ++i) {
    (*i)->UpdateEntry((*i)->controller()->GetLastCommittedEntry(), 0, 0);
  }
}

}  // namespace content
