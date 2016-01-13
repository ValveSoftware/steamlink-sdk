// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_policy.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/ssl/ssl_cert_error_handler.h"
#include "content/browser/ssl/ssl_request_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "net/ssl/ssl_info.h"
#include "webkit/common/resource_type.h"


namespace content {

SSLPolicy::SSLPolicy(SSLPolicyBackend* backend)
    : backend_(backend) {
  DCHECK(backend_);
}

void SSLPolicy::OnCertError(SSLCertErrorHandler* handler) {
  // First we check if we know the policy for this error.
  net::CertPolicy::Judgment judgment = backend_->QueryPolicy(
      handler->ssl_info().cert.get(),
      handler->request_url().host(),
      handler->cert_error());

  if (judgment == net::CertPolicy::ALLOWED) {
    handler->ContinueRequest();
    return;
  }

  // The judgment is either DENIED or UNKNOWN.
  // For now we handle the DENIED as the UNKNOWN, which means a blocking
  // page is shown to the user every time he comes back to the page.

  switch (handler->cert_error()) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_WEAK_KEY:
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
      OnCertErrorInternal(handler, !handler->fatal(), handler->fatal());
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
      OnCertErrorInternal(handler, false, handler->fatal());
      break;
    default:
      NOTREACHED();
      handler->CancelRequest();
      break;
  }
}

void SSLPolicy::DidRunInsecureContent(NavigationEntryImpl* entry,
                                      const std::string& security_origin) {
  if (!entry)
    return;

  SiteInstance* site_instance = entry->site_instance();
  if (!site_instance)
      return;

  backend_->HostRanInsecureContent(GURL(security_origin).host(),
                                   site_instance->GetProcess()->GetID());
}

void SSLPolicy::OnRequestStarted(SSLRequestInfo* info) {
  // TODO(abarth): This mechanism is wrong.  What we should be doing is sending
  // this information back through WebKit and out some FrameLoaderClient
  // methods.

  if (net::IsCertStatusError(info->ssl_cert_status()))
    backend_->HostRanInsecureContent(info->url().host(), info->child_id());
}

void SSLPolicy::UpdateEntry(NavigationEntryImpl* entry,
                            WebContentsImpl* web_contents) {
  DCHECK(entry);

  InitializeEntryIfNeeded(entry);

  if (!entry->GetURL().SchemeIsSecure())
    return;

  if (!web_contents->DisplayedInsecureContent())
    entry->GetSSL().content_status &= ~SSLStatus::DISPLAYED_INSECURE_CONTENT;

  // An HTTPS response may not have a certificate for some reason.  When that
  // happens, use the unauthenticated (HTTP) rather than the authentication
  // broken security style so that we can detect this error condition.
  if (!entry->GetSSL().cert_id) {
    entry->GetSSL().security_style = SECURITY_STYLE_UNAUTHENTICATED;
    return;
  }

  if (web_contents->DisplayedInsecureContent())
    entry->GetSSL().content_status |= SSLStatus::DISPLAYED_INSECURE_CONTENT;

  if (net::IsCertStatusError(entry->GetSSL().cert_status)) {
    // Minor errors don't lower the security style to
    // SECURITY_STYLE_AUTHENTICATION_BROKEN.
    if (!net::IsCertStatusMinorError(entry->GetSSL().cert_status)) {
      entry->GetSSL().security_style =
          SECURITY_STYLE_AUTHENTICATION_BROKEN;
    }
    return;
  }

  SiteInstance* site_instance = entry->site_instance();
  // Note that |site_instance| can be NULL here because NavigationEntries don't
  // necessarily have site instances.  Without a process, the entry can't
  // possibly have insecure content.  See bug http://crbug.com/12423.
  if (site_instance &&
      backend_->DidHostRunInsecureContent(
          entry->GetURL().host(), site_instance->GetProcess()->GetID())) {
    entry->GetSSL().security_style =
        SECURITY_STYLE_AUTHENTICATION_BROKEN;
    entry->GetSSL().content_status |= SSLStatus::RAN_INSECURE_CONTENT;
    return;
  }
}

void SSLPolicy::OnAllowCertificate(scoped_refptr<SSLCertErrorHandler> handler,
                                   bool allow) {
  if (allow) {
    // Default behavior for accepting a certificate.
    // Note that we should not call SetMaxSecurityStyle here, because the active
    // NavigationEntry has just been deleted (in HideInterstitialPage) and the
    // new NavigationEntry will not be set until DidNavigate.  This is ok,
    // because the new NavigationEntry will have its max security style set
    // within DidNavigate.
    //
    // While AllowCertForHost() executes synchronously on this thread,
    // ContinueRequest() gets posted to a different thread. Calling
    // AllowCertForHost() first ensures deterministic ordering.
    backend_->AllowCertForHost(handler->ssl_info().cert.get(),
                               handler->request_url().host(),
                               handler->cert_error());
    handler->ContinueRequest();
  } else {
    // Default behavior for rejecting a certificate.
    //
    // While DenyCertForHost() executes synchronously on this thread,
    // CancelRequest() gets posted to a different thread. Calling
    // DenyCertForHost() first ensures deterministic ordering.
    backend_->DenyCertForHost(handler->ssl_info().cert.get(),
                              handler->request_url().host(),
                              handler->cert_error());
    handler->CancelRequest();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Certificate Error Routines

void SSLPolicy::OnCertErrorInternal(SSLCertErrorHandler* handler,
                                    bool overridable,
                                    bool strict_enforcement) {
  CertificateRequestResultType result =
      CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE;
  GetContentClient()->browser()->AllowCertificateError(
      handler->render_process_id(),
      handler->render_frame_id(),
      handler->cert_error(),
      handler->ssl_info(),
      handler->request_url(),
      handler->resource_type(),
      overridable,
      strict_enforcement,
      base::Bind(&SSLPolicy::OnAllowCertificate, base::Unretained(this),
                 make_scoped_refptr(handler)),
      &result);
  switch (result) {
    case CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE:
      break;
    case CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL:
      handler->CancelRequest();
      break;
    case CERTIFICATE_REQUEST_RESULT_TYPE_DENY:
      handler->DenyRequest();
      break;
    default:
      NOTREACHED();
  }
}

void SSLPolicy::InitializeEntryIfNeeded(NavigationEntryImpl* entry) {
  if (entry->GetSSL().security_style != SECURITY_STYLE_UNKNOWN)
    return;

  entry->GetSSL().security_style = entry->GetURL().SchemeIsSecure() ?
      SECURITY_STYLE_AUTHENTICATED : SECURITY_STYLE_UNAUTHENTICATED;
}

void SSLPolicy::OriginRanInsecureContent(const std::string& origin, int pid) {
  GURL parsed_origin(origin);
  if (parsed_origin.SchemeIsSecure())
    backend_->HostRanInsecureContent(parsed_origin.host(), pid);
}

}  // namespace content
