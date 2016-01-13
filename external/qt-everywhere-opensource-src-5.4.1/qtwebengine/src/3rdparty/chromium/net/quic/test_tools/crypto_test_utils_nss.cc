// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include <keyhi.h>
#include <pk11pub.h>
#include <sechash.h>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "crypto/ec_private_key.h"
#include "net/quic/crypto/channel_id.h"

using base::StringPiece;
using std::string;

namespace net {

namespace test {

// TODO(rtenneti): Convert Sign() to be asynchronous using a completion
// callback.
class TestChannelIDKey : public ChannelIDKey {
 public:
  explicit TestChannelIDKey(crypto::ECPrivateKey* ecdsa_keypair)
      : ecdsa_keypair_(ecdsa_keypair) {}
  virtual ~TestChannelIDKey() {}

  // ChannelIDKey implementation.

  virtual bool Sign(StringPiece signed_data,
                    string* out_signature) const OVERRIDE {
    unsigned char hash_buf[SHA256_LENGTH];
    SECItem hash_item = { siBuffer, hash_buf, sizeof(hash_buf) };

    HASHContext* sha256 = HASH_Create(HASH_AlgSHA256);
    if (!sha256) {
      return false;
    }
    HASH_Begin(sha256);
    HASH_Update(sha256,
                reinterpret_cast<const unsigned char*>(
                    ChannelIDVerifier::kContextStr),
                strlen(ChannelIDVerifier::kContextStr) + 1);
    HASH_Update(sha256,
                reinterpret_cast<const unsigned char*>(
                    ChannelIDVerifier::kClientToServerStr),
                strlen(ChannelIDVerifier::kClientToServerStr) + 1);
    HASH_Update(sha256,
                reinterpret_cast<const unsigned char*>(signed_data.data()),
                signed_data.size());
    HASH_End(sha256, hash_buf, &hash_item.len, sizeof(hash_buf));
    HASH_Destroy(sha256);

    // The signature consists of a pair of 32-byte numbers.
    static const unsigned int kSignatureLength = 32 * 2;
    string signature;
    SECItem sig_item = {
        siBuffer,
        reinterpret_cast<unsigned char*>(
            WriteInto(&signature, kSignatureLength + 1)),
        kSignatureLength
    };

    if (PK11_Sign(ecdsa_keypair_->key(), &sig_item, &hash_item) != SECSuccess) {
      return false;
    }
    *out_signature = signature;
    return true;
  }

  virtual string SerializeKey() const OVERRIDE {
    const SECKEYPublicKey* public_key = ecdsa_keypair_->public_key();

    // public_key->u.ec.publicValue is an ANSI X9.62 public key which, for
    // a P-256 key, is 0x04 (meaning uncompressed) followed by the x and y field
    // elements as 32-byte, big-endian numbers.
    static const unsigned int kExpectedKeyLength = 65;

    const unsigned char* const data = public_key->u.ec.publicValue.data;
    const unsigned int len = public_key->u.ec.publicValue.len;
    if (len != kExpectedKeyLength || data[0] != 0x04) {
      return "";
    }

    string key(reinterpret_cast<const char*>(data + 1), kExpectedKeyLength - 1);
    return key;
  }

 private:
  crypto::ECPrivateKey* ecdsa_keypair_;
};

class TestChannelIDSource : public ChannelIDSource {
 public:
  virtual ~TestChannelIDSource() {
    STLDeleteValues(&hostname_to_key_);
  }

  // ChannelIDSource implementation.

  virtual QuicAsyncStatus GetChannelIDKey(
      const string& hostname,
      scoped_ptr<ChannelIDKey>* channel_id_key,
      ChannelIDSourceCallback* /*callback*/) OVERRIDE {
    channel_id_key->reset(new TestChannelIDKey(HostnameToKey(hostname)));
    return QUIC_SUCCESS;
  }

 private:
  typedef std::map<string, crypto::ECPrivateKey*> HostnameToKeyMap;

  crypto::ECPrivateKey* HostnameToKey(const string& hostname) {
    HostnameToKeyMap::const_iterator it = hostname_to_key_.find(hostname);
    if (it != hostname_to_key_.end()) {
      return it->second;
    }

    crypto::ECPrivateKey* keypair = crypto::ECPrivateKey::Create();
    if (!keypair) {
      return NULL;
    }
    hostname_to_key_[hostname] = keypair;
    return keypair;
  }

  HostnameToKeyMap hostname_to_key_;
};

// static
ChannelIDSource* CryptoTestUtils::ChannelIDSourceForTesting() {
  return new TestChannelIDSource();
}

}  // namespace test

}  // namespace net
