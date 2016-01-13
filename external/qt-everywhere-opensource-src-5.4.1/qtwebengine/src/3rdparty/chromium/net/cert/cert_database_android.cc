// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_database.h"

#include "base/logging.h"
#include "base/observer_list_threadsafe.h"
#include "net/base/net_errors.h"

namespace net {

CertDatabase::CertDatabase()
    : observer_list_(new ObserverListThreadSafe<Observer>) {
}

CertDatabase::~CertDatabase() {}

int CertDatabase::CheckUserCert(X509Certificate* cert) {
  // NOTE: This method shall never be called on Android.
  //
  // On other platforms, it is only used by the SSLAddCertHandler class
  // to handle veritication and installation of downloaded certificates.
  //
  // On Android, the certificate data is passed directly to the system's
  // CertInstaller activity, which handles verification, naming,
  // installation and UI (for success/failure).
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int CertDatabase::AddUserCert(X509Certificate* cert) {
  // This method is only used by the content SSLAddCertHandler which is
  // never used on Android.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

void CertDatabase::OnAndroidKeyStoreChanged() {
  NotifyObserversOfCertAdded(NULL);
}

void CertDatabase::OnAndroidKeyChainChanged() {
  observer_list_->Notify(&Observer::OnCACertChanged,
                         scoped_refptr<X509Certificate>());
}

}  // namespace net
