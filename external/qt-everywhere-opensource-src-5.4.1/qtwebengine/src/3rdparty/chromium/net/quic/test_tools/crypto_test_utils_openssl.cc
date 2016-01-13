// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>

#include "crypto/openssl_util.h"
#include "crypto/secure_hash.h"
#include "net/quic/crypto/channel_id.h"

using base::StringPiece;
using std::string;

namespace {

void EvpMdCtxCleanUp(EVP_MD_CTX* ctx) {
  (void)EVP_MD_CTX_cleanup(ctx);
}

} // namespace anonymous

namespace net {

namespace test {

class TestChannelIDKey : public ChannelIDKey {
 public:
  explicit TestChannelIDKey(EVP_PKEY* ecdsa_key) : ecdsa_key_(ecdsa_key) {}
  virtual ~TestChannelIDKey() OVERRIDE {}

  // ChannelIDKey implementation.

  virtual bool Sign(StringPiece signed_data,
                    string* out_signature) const OVERRIDE {
    EVP_MD_CTX md_ctx;
    EVP_MD_CTX_init(&md_ctx);
    crypto::ScopedOpenSSL<EVP_MD_CTX, EvpMdCtxCleanUp>
        md_ctx_cleanup(&md_ctx);

    if (EVP_DigestSignInit(&md_ctx, NULL, EVP_sha256(), NULL,
                           ecdsa_key_.get()) != 1) {
      return false;
    }

    EVP_DigestUpdate(&md_ctx, ChannelIDVerifier::kContextStr,
                     strlen(ChannelIDVerifier::kContextStr) + 1);
    EVP_DigestUpdate(&md_ctx, ChannelIDVerifier::kClientToServerStr,
                     strlen(ChannelIDVerifier::kClientToServerStr) + 1);
    EVP_DigestUpdate(&md_ctx, signed_data.data(), signed_data.size());

    size_t sig_len;
    if (!EVP_DigestSignFinal(&md_ctx, NULL, &sig_len)) {
      return false;
    }

    scoped_ptr<uint8[]> der_sig(new uint8[sig_len]);
    if (!EVP_DigestSignFinal(&md_ctx, der_sig.get(), &sig_len)) {
      return false;
    }

    uint8* derp = der_sig.get();
    crypto::ScopedOpenSSL<ECDSA_SIG, ECDSA_SIG_free> sig(
        d2i_ECDSA_SIG(NULL, const_cast<const uint8**>(&derp), sig_len));
    if (sig.get() == NULL) {
      return false;
    }

    // The signature consists of a pair of 32-byte numbers.
    static const size_t kSignatureLength = 32 * 2;
    scoped_ptr<uint8[]> signature(new uint8[kSignatureLength]);
    memset(signature.get(), 0, kSignatureLength);
    BN_bn2bin(sig.get()->r, signature.get() + 32 - BN_num_bytes(sig.get()->r));
    BN_bn2bin(sig.get()->s, signature.get() + 64 - BN_num_bytes(sig.get()->s));

    *out_signature = string(reinterpret_cast<char*>(signature.get()),
                            kSignatureLength);

    return true;
  }

  virtual string SerializeKey() const OVERRIDE {
    // i2d_PublicKey will produce an ANSI X9.62 public key which, for a P-256
    // key, is 0x04 (meaning uncompressed) followed by the x and y field
    // elements as 32-byte, big-endian numbers.
    static const int kExpectedKeyLength = 65;

    int len = i2d_PublicKey(ecdsa_key_.get(), NULL);
    if (len != kExpectedKeyLength) {
      return "";
    }

    uint8 buf[kExpectedKeyLength];
    uint8* derp = buf;
    i2d_PublicKey(ecdsa_key_.get(), &derp);

    return string(reinterpret_cast<char*>(buf + 1), kExpectedKeyLength - 1);
  }

 private:
  crypto::ScopedOpenSSL<EVP_PKEY, EVP_PKEY_free> ecdsa_key_;
};

class TestChannelIDSource : public ChannelIDSource {
 public:
  virtual ~TestChannelIDSource() {}

  // ChannelIDSource implementation.

  virtual QuicAsyncStatus GetChannelIDKey(
      const string& hostname,
      scoped_ptr<ChannelIDKey>* channel_id_key,
      ChannelIDSourceCallback* /*callback*/) OVERRIDE {
    channel_id_key->reset(new TestChannelIDKey(HostnameToKey(hostname)));
    return QUIC_SUCCESS;
  }

 private:
  static EVP_PKEY* HostnameToKey(const string& hostname) {
    // In order to generate a deterministic key for a given hostname the
    // hostname is hashed with SHA-256 and the resulting digest is treated as a
    // big-endian number. The most-significant bit is cleared to ensure that
    // the resulting value is less than the order of the group and then it's
    // taken as a private key. Given the private key, the public key is
    // calculated with a group multiplication.
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, hostname.data(), hostname.size());

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &sha256);

    // Ensure that the digest is less than the order of the P-256 group by
    // clearing the most-significant bit.
    digest[0] &= 0x7f;

    crypto::ScopedOpenSSL<BIGNUM, BN_free> k(BN_new());
    CHECK(BN_bin2bn(digest, sizeof(digest), k.get()) != NULL);

    crypto::ScopedOpenSSL<EC_GROUP, EC_GROUP_free> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    CHECK(p256.get());

    crypto::ScopedOpenSSL<EC_KEY, EC_KEY_free> ecdsa_key(EC_KEY_new());
    CHECK(ecdsa_key.get() != NULL &&
          EC_KEY_set_group(ecdsa_key.get(), p256.get()));

    crypto::ScopedOpenSSL<EC_POINT, EC_POINT_free> point(
        EC_POINT_new(p256.get()));
    CHECK(EC_POINT_mul(p256.get(), point.get(), k.get(), NULL, NULL, NULL));

    EC_KEY_set_private_key(ecdsa_key.get(), k.get());
    EC_KEY_set_public_key(ecdsa_key.get(), point.get());

    crypto::ScopedOpenSSL<EVP_PKEY, EVP_PKEY_free> pkey(EVP_PKEY_new());
    // EVP_PKEY_set1_EC_KEY takes a reference so no |release| here.
    EVP_PKEY_set1_EC_KEY(pkey.get(), ecdsa_key.get());

    return pkey.release();
  }
};

// static
ChannelIDSource* CryptoTestUtils::ChannelIDSourceForTesting() {
  return new TestChannelIDSource();
}

}  // namespace test

}  // namespace net
