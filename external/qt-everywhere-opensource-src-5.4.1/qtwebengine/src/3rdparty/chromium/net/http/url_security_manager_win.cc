// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "net/http/http_auth_filter.h"
#include "url/gurl.h"

// The Windows implementation of URLSecurityManager uses WinINet/IE's
// URL security zone manager.  See the MSDN page "URL Security Zones" at
// http://msdn.microsoft.com/en-us/library/ms537021(VS.85).aspx for more
// info on the Internet Security Manager and Internet Zone Manager objects.
//
// On Windows, we honor the WinINet/IE settings and group policy related to
// URL Security Zones.  See the Microsoft Knowledge Base article 182569
// "Internet Explorer security zones registry entries for advanced users"
// (http://support.microsoft.com/kb/182569) for more info on these registry
// keys.

namespace net {

class URLSecurityManagerWin : public URLSecurityManager {
 public:
  explicit URLSecurityManagerWin(const HttpAuthFilter* whitelist_delegate);

  // URLSecurityManager methods:
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const;
  virtual bool CanDelegate(const GURL& auth_origin) const;

 private:
  bool EnsureSystemSecurityManager();

  base::win::ScopedComPtr<IInternetSecurityManager> security_manager_;
  scoped_ptr<const HttpAuthFilter> whitelist_delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLSecurityManagerWin);
};

URLSecurityManagerWin::URLSecurityManagerWin(
    const HttpAuthFilter* whitelist_delegate)
    : whitelist_delegate_(whitelist_delegate) {
}

bool URLSecurityManagerWin::CanUseDefaultCredentials(
    const GURL& auth_origin) const {
  if (!const_cast<URLSecurityManagerWin*>(this)->EnsureSystemSecurityManager())
    return false;

  std::wstring url_w = base::ASCIIToWide(auth_origin.spec());
  DWORD policy = 0;
  HRESULT hr;
  hr = security_manager_->ProcessUrlAction(url_w.c_str(),
                                           URLACTION_CREDENTIALS_USE,
                                           reinterpret_cast<BYTE*>(&policy),
                                           sizeof(policy), NULL, 0,
                                           PUAF_NOUI, 0);
  if (FAILED(hr)) {
    LOG(ERROR) << "IInternetSecurityManager::ProcessUrlAction failed: " << hr;
    return false;
  }

  // Four possible policies for URLACTION_CREDENTIALS_USE.  See the MSDN page
  // "About URL Security Zones" at
  // http://msdn.microsoft.com/en-us/library/ms537183(VS.85).aspx
  switch (policy) {
    case URLPOLICY_CREDENTIALS_SILENT_LOGON_OK:
      return true;
    case URLPOLICY_CREDENTIALS_CONDITIONAL_PROMPT: {
      // This policy means "prompt the user for permission if the resource is
      // not located in the Intranet zone".  TODO(wtc): Note that it's
      // prompting for permission (to use the default credentials), as opposed
      // to prompting the user to enter a user name and password.

      // URLZONE_LOCAL_MACHINE 0
      // URLZONE_INTRANET      1
      // URLZONE_TRUSTED       2
      // URLZONE_INTERNET      3
      // URLZONE_UNTRUSTED     4
      DWORD zone = 0;
      hr = security_manager_->MapUrlToZone(url_w.c_str(), &zone, 0);
      if (FAILED(hr)) {
        LOG(ERROR) << "IInternetSecurityManager::MapUrlToZone failed: " << hr;
        return false;
      }
      return zone <= URLZONE_INTRANET;
    }
    case URLPOLICY_CREDENTIALS_MUST_PROMPT_USER:
      return false;
    case URLPOLICY_CREDENTIALS_ANONYMOUS_ONLY:
      // TODO(wtc): we should fail the authentication.
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool URLSecurityManagerWin::CanDelegate(const GURL& auth_origin) const {
  // TODO(cbentzel): Could this just use the security zone as well? Apparently
  //                 this is what IE does as well.
  if (whitelist_delegate_.get())
    return whitelist_delegate_->IsValid(auth_origin, HttpAuth::AUTH_SERVER);
  return false;
}

bool URLSecurityManagerWin::EnsureSystemSecurityManager() {
  if (!security_manager_) {
    HRESULT hr = CoInternetCreateSecurityManager(NULL,
                                                 security_manager_.Receive(),
                                                 NULL);
    if (FAILED(hr) || !security_manager_) {
      LOG(ERROR) << "Unable to create the Windows Security Manager instance";
      return false;
    }
  }
  return true;
}

// static
URLSecurityManager* URLSecurityManager::Create(
    const HttpAuthFilter* whitelist_default,
    const HttpAuthFilter* whitelist_delegate) {
  // If we have a whitelist, just use that.
  if (whitelist_default)
    return new URLSecurityManagerWhitelist(whitelist_default,
                                           whitelist_delegate);
  return new URLSecurityManagerWin(whitelist_delegate);
}

}  //  namespace net
