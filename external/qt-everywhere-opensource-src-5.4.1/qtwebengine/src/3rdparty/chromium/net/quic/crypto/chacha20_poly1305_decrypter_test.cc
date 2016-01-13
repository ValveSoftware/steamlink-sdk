// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/chacha20_poly1305_decrypter.h"

#include "net/quic/test_tools/quic_test_utils.h"

using base::StringPiece;

namespace {

// The test vectors come from draft-agl-tls-chacha20poly1305-04 Section 7.

// Each test vector consists of six strings of lowercase hexadecimal digits.
// The strings may be empty (zero length). A test vector with a NULL |key|
// marks the end of an array of test vectors.
struct TestVector {
  // Input:
  const char* key;
  const char* iv;
  const char* aad;
  const char* ct;

  // Expected output:
  const char* pt;  // An empty string "" means decryption succeeded and
                   // the plaintext is zero-length. NULL means decryption
                   // failed.
};

const TestVector test_vectors[] = {
  { "4290bcb154173531f314af57f3be3b5006da371ece272afa1b5dbdd110"
        "0a1007",
    "cd7cf67be39c794a",
    "87e229d4500845a079c0",
    "e3e446f7ede9a19b62a4677dabf4e3d24b876bb28475",  // "3896e1d6" truncated.
    "86d09974840bded2a5ca"
  },
  // Modify the ciphertext (ChaCha20 encryption output).
  { "4290bcb154173531f314af57f3be3b5006da371ece272afa1b5dbdd110"
        "0a1007",
    "cd7cf67be39c794a",
    "87e229d4500845a079c0",
    "f3e446f7ede9a19b62a4677dabf4e3d24b876bb28475",  // "3896e1d6" truncated.
    NULL  // FAIL
  },
  // Modify the ciphertext (Poly1305 authenticator).
  { "4290bcb154173531f314af57f3be3b5006da371ece272afa1b5dbdd110"
        "0a1007",
    "cd7cf67be39c794a",
    "87e229d4500845a079c0",
    "e3e446f7ede9a19b62a4677dabf4e3d24b876bb28476",  // "3896e1d6" truncated.
    NULL  // FAIL
  },
  // Modify the associated data.
  { "4290bcb154173531f314af57f3be3b5006da371ece272afa1b5dbdd110"
        "0a1007",
    "dd7cf67be39c794a",
    "87e229d4500845a079c0",
    "e3e446f7ede9a19b62a4677dabf4e3d24b876bb28475",  // "3896e1d6" truncated.
    NULL  // FAIL
  },
  { NULL }
};

}  // namespace

namespace net {
namespace test {

// DecryptWithNonce wraps the |Decrypt| method of |decrypter| to allow passing
// in an nonce and also to allocate the buffer needed for the plaintext.
QuicData* DecryptWithNonce(ChaCha20Poly1305Decrypter* decrypter,
                           StringPiece nonce,
                           StringPiece associated_data,
                           StringPiece ciphertext) {
  size_t plaintext_size = ciphertext.length();
  scoped_ptr<char[]> plaintext(new char[plaintext_size]);

  if (!decrypter->Decrypt(nonce, associated_data, ciphertext,
                          reinterpret_cast<unsigned char*>(plaintext.get()),
                          &plaintext_size)) {
    return NULL;
  }
  return new QuicData(plaintext.release(), plaintext_size, true);
}

TEST(ChaCha20Poly1305DecrypterTest, Decrypt) {
  if (!ChaCha20Poly1305Decrypter::IsSupported()) {
    LOG(INFO) << "ChaCha20+Poly1305 not supported. Test skipped.";
    return;
  }

  for (size_t i = 0; test_vectors[i].key != NULL; i++) {
    // If not present then decryption is expected to fail.
    bool has_pt = test_vectors[i].pt;

    // Decode the test vector.
    string key;
    string iv;
    string aad;
    string ct;
    string pt;
    ASSERT_TRUE(DecodeHexString(test_vectors[i].key, &key));
    ASSERT_TRUE(DecodeHexString(test_vectors[i].iv, &iv));
    ASSERT_TRUE(DecodeHexString(test_vectors[i].aad, &aad));
    ASSERT_TRUE(DecodeHexString(test_vectors[i].ct, &ct));
    if (has_pt) {
      ASSERT_TRUE(DecodeHexString(test_vectors[i].pt, &pt));
    }

    ChaCha20Poly1305Decrypter decrypter;
    ASSERT_TRUE(decrypter.SetKey(key));
    scoped_ptr<QuicData> decrypted(DecryptWithNonce(
        &decrypter, iv,
        // This deliberately tests that the decrypter can handle an AAD that
        // is set to NULL, as opposed to a zero-length, non-NULL pointer.
        StringPiece(aad.length() ? aad.data() : NULL, aad.length()), ct));
    if (!decrypted.get()) {
      EXPECT_FALSE(has_pt);
      continue;
    }
    EXPECT_TRUE(has_pt);

    ASSERT_EQ(pt.length(), decrypted->length());
    test::CompareCharArraysWithHexError("plaintext", decrypted->data(),
                                        pt.length(), pt.data(), pt.length());
  }
}

}  // namespace test
}  // namespace net
