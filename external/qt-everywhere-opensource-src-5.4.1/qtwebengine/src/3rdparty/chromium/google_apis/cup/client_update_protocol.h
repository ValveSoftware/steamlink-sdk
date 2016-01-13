// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CUP_CLIENT_UPDATE_PROTOCOL_H_
#define GOOGLE_APIS_CUP_CLIENT_UPDATE_PROTOCOL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"

// Forward declare types for NSS.
#if defined(USE_NSS) || defined(OS_WIN) || defined(OS_MACOSX)
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;
#endif

// Client Update Protocol, or CUP, is used by Google Update (Omaha) servers to
// ensure freshness and authenticity of update checks over HTTP, without the
// overhead of HTTPS -- namely, no PKI, no guarantee of privacy, and no request
// replay protection (since update checks are idempotent).
//
// http://omaha.googlecode.com/svn/wiki/cup.html
//
// Each ClientUpdateProtocol represents a single update check in flight -- a
// call to SignRequest() generates internal state used by ValidateResponse().
//
// This implementation does not persist client proofs by design.
class ClientUpdateProtocol {
 public:
  ~ClientUpdateProtocol();

  // Initializes this instance of CUP with a versioned public key. |key_version|
  // must be non-negative. |public_key| is expected to be a DER-encoded ASN.1
  // Subject Public Key Info.  Returns a NULL pointer on failure.
  static scoped_ptr<ClientUpdateProtocol> Create(
      int key_version, const base::StringPiece& public_key);

  // Returns a versioned encrypted secret (v|w) in a URL-safe Base64 encoding.
  // Add to your URL before calling SignRequest().
  std::string GetVersionedSecret() const;

  // Generates freshness/authentication data for an outgoing update check.
  // |url| contains the the URL that the request will be sent to; it should
  // include GetVersionedSecret() in its query string. This needs to be
  // formatted in the way that the Omaha server expects: omit the scheme and
  // any port number. (e.g. "//tools.google.com/service/update2?w=1:abcdef")
  // |request_body| contains the body of the update check request in UTF-8.
  //
  // On success, returns true, and |client_proof| receives a Base64-encoded
  // client proof, which should be sent in the If-Match HTTP header. On
  // failure, returns false, and |client_proof| is not modified.
  //
  // This method will store internal state in this instance used by calls to
  // ValidateResponse(); if you need to have multiple update checks in flight,
  // initialize a separate CUP instance for each one.
  bool SignRequest(const base::StringPiece& url,
                   const base::StringPiece& request_body,
                   std::string* client_proof);

  // Validates a response given to a update check request previously signed
  // with SignRequest(). |request_body| contains the body of the response in
  // UTF-8. |server_cookie| contains the persisted credential cookie provided
  // by the server. |server_proof| contains the Base64-encoded server proof,
  // which is passed in the ETag HTTP header. Returns true if the response is
  // valid.
  // This method uses internal state that is set by a prior SignRequest() call.
  bool ValidateResponse(const base::StringPiece& response_body,
                        const base::StringPiece& server_cookie,
                        const base::StringPiece& server_proof);

 private:
  friend class CupTest;

  explicit ClientUpdateProtocol(int key_version);

  // Decodes |public_key| into the appropriate internal structures. Returns
  // the length of the public key (modulus) in bytes, or 0 on failure.
  bool LoadPublicKey(const base::StringPiece& public_key);

  // Returns the size of the public key in bytes, or 0 on failure.
  size_t PublicKeyLength();

  // Helper function for BuildSharedKey() -- encrypts |key_source| (r) using
  // the loaded public key, filling out |encrypted_key_source_| (w).
  // Returns true on success.
  bool EncryptKeySource(const std::vector<uint8>& key_source);

  // Generates a random key source and passes it to DeriveSharedKey().
  // Returns true on success.
  bool BuildRandomSharedKey();

  // Sets a fixed key source from a character string and passes it to
  // DeriveSharedKey(). Used for unit testing only. Returns true on success.
  bool SetSharedKeyForTesting(const base::StringPiece& fixed_key_source);

  // Given a key source (r), derives the values of |shared_key_| (sk') and
  // encrypted_key_source_ (w).  Returns true on success.
  bool DeriveSharedKey(const std::vector<uint8>& source);

  // The server keeps multiple private keys; a version must be sent so that
  // the right private key is used to decode the versioned secret.  (The CUP
  // specification calls this "v".)
  int pub_key_version_;

  // Holds the shared key, which is used to generate an HMAC signature for both
  // the update check request and the update response. The client builds it
  // locally, but sends the server an encrypted copy of the key source to
  // synthesize it on its own. (The CUP specification calls this "sk'".)
  std::vector<uint8> shared_key_;

  // Holds the original contents of key_source_ that have been encrypted with
  // the server's public key. The client sends this, along with the version of
  // the keypair that was used, to the server. The server decrypts it using its
  // private key to get the contents of key_source_, from which it recreates the
  // shared key. (The CUP specification calls this "w".)
  std::vector<uint8> encrypted_key_source_;

  // Holds the hash of the update check request, the URL that it was sent to,
  // and the versioned secret. This is filled out by a successful call to
  // SignRequest(), and used by ValidateResponse() to confirm that the server
  // has successfully decoded the versioned secret and signed the response using
  // the same shared key as our own. (The CUP specification calls this "hw".)
  std::vector<uint8> client_challenge_hash_;

  // The public key used to encrypt the key source.  (The CUP specification
  // calls this "pk[v]".)
#if defined(USE_NSS) || defined(OS_WIN) || defined(OS_MACOSX)
  SECKEYPublicKey* public_key_;
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(ClientUpdateProtocol);
};

#endif  // GOOGLE_APIS_CUP_CLIENT_UPDATE_PROTOCOL_H_

