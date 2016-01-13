// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SERVER_BOUND_CERT_STORE_H_
#define NET_SSL_SERVER_BOUND_CERT_STORE_H_

#include <list>
#include <string>

#include "base/callback.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

// An interface for storing and retrieving server bound certs.
// There isn't a domain bound certs spec yet, but the old origin bound
// certificates are specified in
// http://balfanz.github.com/tls-obc-spec/draft-balfanz-tls-obc-01.html.

// Owned only by a single ServerBoundCertService object, which is responsible
// for deleting it.
class NET_EXPORT ServerBoundCertStore
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // The ServerBoundCert class contains a private key in addition to the server
  // cert.
  class NET_EXPORT ServerBoundCert {
   public:
    ServerBoundCert();
    ServerBoundCert(const std::string& server_identifier,
                    base::Time creation_time,
                    base::Time expiration_time,
                    const std::string& private_key,
                    const std::string& cert);
    ~ServerBoundCert();

    // Server identifier.  For domain bound certs, for instance "verisign.com".
    const std::string& server_identifier() const { return server_identifier_; }
    // The time the certificate was created, also the start of the certificate
    // validity period.
    base::Time creation_time() const { return creation_time_; }
    // The time after which this certificate is no longer valid.
    base::Time expiration_time() const { return expiration_time_; }
    // The encoding of the private key depends on the type.
    // rsa_sign: DER-encoded PrivateKeyInfo struct.
    // ecdsa_sign: DER-encoded EncryptedPrivateKeyInfo struct.
    const std::string& private_key() const { return private_key_; }
    // DER-encoded certificate.
    const std::string& cert() const { return cert_; }

   private:
    std::string server_identifier_;
    base::Time creation_time_;
    base::Time expiration_time_;
    std::string private_key_;
    std::string cert_;
  };

  typedef std::list<ServerBoundCert> ServerBoundCertList;

  typedef base::Callback<void(
      int,
      const std::string&,
      base::Time,
      const std::string&,
      const std::string&)> GetCertCallback;
  typedef base::Callback<void(const ServerBoundCertList&)> GetCertListCallback;

  virtual ~ServerBoundCertStore() {}

  // GetServerBoundCert may return the result synchronously through the
  // output parameters, in which case it will return either OK if a cert is
  // found in the store, or ERR_FILE_NOT_FOUND if none is found.  If the
  // result cannot be returned synchronously, GetServerBoundCert will
  // return ERR_IO_PENDING and the callback will be called with the result
  // asynchronously.
  virtual int GetServerBoundCert(
      const std::string& server_identifier,
      base::Time* expiration_time,
      std::string* private_key_result,
      std::string* cert_result,
      const GetCertCallback& callback) = 0;

  // Adds a server bound cert and the corresponding private key to the store.
  virtual void SetServerBoundCert(
      const std::string& server_identifier,
      base::Time creation_time,
      base::Time expiration_time,
      const std::string& private_key,
      const std::string& cert) = 0;

  // Removes a server bound cert and the corresponding private key from the
  // store.
  virtual void DeleteServerBoundCert(
      const std::string& server_identifier,
      const base::Closure& completion_callback) = 0;

  // Deletes all of the server bound certs that have a creation_date greater
  // than or equal to |delete_begin| and less than |delete_end|.  If a
  // base::Time value is_null, that side of the comparison is unbounded.
  virtual void DeleteAllCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& completion_callback) = 0;

  // Removes all server bound certs and the corresponding private keys from
  // the store.
  virtual void DeleteAll(const base::Closure& completion_callback) = 0;

  // Returns all server bound certs and the corresponding private keys.
  virtual void GetAllServerBoundCerts(const GetCertListCallback& callback) = 0;

  // Helper function that adds all certs from |list| into this instance.
  void InitializeFrom(const ServerBoundCertList& list);

  // Returns the number of certs in the store.  May return 0 if the backing
  // store is not loaded yet.
  // Public only for unit testing.
  virtual int GetCertCount() = 0;

  // When invoked, instructs the store to keep session related data on
  // destruction.
  virtual void SetForceKeepSessionState() = 0;
};

}  // namespace net

#endif  // NET_SSL_SERVER_BOUND_CERT_STORE_H_
