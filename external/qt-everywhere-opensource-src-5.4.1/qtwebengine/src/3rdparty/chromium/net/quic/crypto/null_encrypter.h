// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_NULL_ENCRYPTER_H_
#define NET_QUIC_CRYPTO_NULL_ENCRYPTER_H_

#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/quic_encrypter.h"

namespace net {

// A NullEncrypter is a QuicEncrypter used before a crypto negotiation
// has occurred.  It does not actually encrypt the payload, but does
// generate a MAC (fnv128) over both the payload and associated data.
class NET_EXPORT_PRIVATE NullEncrypter : public QuicEncrypter {
 public:
  NullEncrypter();
  virtual ~NullEncrypter() {}

  // QuicEncrypter implementation
  virtual bool SetKey(base::StringPiece key) OVERRIDE;
  virtual bool SetNoncePrefix(base::StringPiece nonce_prefix) OVERRIDE;
  virtual bool Encrypt(base::StringPiece nonce,
                       base::StringPiece associated_data,
                       base::StringPiece plaintext,
                       unsigned char* output) OVERRIDE;
  virtual QuicData* EncryptPacket(QuicPacketSequenceNumber sequence_number,
                                  base::StringPiece associated_data,
                                  base::StringPiece plaintext) OVERRIDE;
  virtual size_t GetKeySize() const OVERRIDE;
  virtual size_t GetNoncePrefixSize() const OVERRIDE;
  virtual size_t GetMaxPlaintextSize(size_t ciphertext_size) const OVERRIDE;
  virtual size_t GetCiphertextSize(size_t plaintext_size) const OVERRIDE;
  virtual base::StringPiece GetKey() const OVERRIDE;
  virtual base::StringPiece GetNoncePrefix() const OVERRIDE;

 private:
  size_t GetHashLength() const;

  DISALLOW_COPY_AND_ASSIGN(NullEncrypter);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_NULL_ENCRYPTER_H_
