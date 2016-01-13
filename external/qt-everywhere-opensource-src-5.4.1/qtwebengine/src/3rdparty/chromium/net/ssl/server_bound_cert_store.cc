// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/server_bound_cert_store.h"

namespace net {

ServerBoundCertStore::ServerBoundCert::ServerBoundCert() {
}

ServerBoundCertStore::ServerBoundCert::ServerBoundCert(
    const std::string& server_identifier,
    base::Time creation_time,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert)
    : server_identifier_(server_identifier),
      creation_time_(creation_time),
      expiration_time_(expiration_time),
      private_key_(private_key),
      cert_(cert) {}

ServerBoundCertStore::ServerBoundCert::~ServerBoundCert() {}

void ServerBoundCertStore::InitializeFrom(const ServerBoundCertList& list) {
  for (ServerBoundCertList::const_iterator i = list.begin(); i != list.end();
      ++i) {
    SetServerBoundCert(i->server_identifier(), i->creation_time(),
                       i->expiration_time(), i->private_key(), i->cert());
  }
}

}  // namespace net
