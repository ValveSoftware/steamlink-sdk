// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/aes_decryptor.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor.h"
#include "media/base/mock_filters.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_adapter.h"
#include "media/cdm/cdm_file_io.h"
#include "media/cdm/external_clear_key_test_helper.h"
#include "media/cdm/simple_cdm_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrNe;
using ::testing::Unused;

MATCHER(IsEmpty, "") { return arg.empty(); }
MATCHER(IsNotEmpty, "") { return !arg.empty(); }
MATCHER(IsJSONDictionary, "") {
  std::string result(arg.begin(), arg.end());
  std::unique_ptr<base::Value> root(base::JSONReader().ReadToValue(result));
  return (root.get() && root->GetType() == base::Value::TYPE_DICTIONARY);
}

namespace media {

const uint8_t kOriginalData[] = "Original subsample data.";
const int kOriginalDataSize = 24;

// In the examples below, 'k'(key) has to be 16 bytes, and will always require
// 2 bytes of padding. 'kid'(keyid) is variable length, and may require 0, 1,
// or 2 bytes of padding.

const uint8_t kKeyId[] = {
    // base64 equivalent is AAECAw
    0x00, 0x01, 0x02, 0x03};

// Key is 0x0405060708090a0b0c0d0e0f10111213,
// base64 equivalent is BAUGBwgJCgsMDQ4PEBESEw.
const char kKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
    "    }"
    "  ],"
    "  \"type\": \"temporary\""
    "}";

// Same kid as kKeyAsJWK, key to decrypt kEncryptedData2
const char kKeyAlternateAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
    "    }"
    "  ]"
    "}";

const char kWrongKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"7u7u7u7u7u7u7u7u7u7u7g\""
    "    }"
    "  ]"
    "}";

const char kWrongSizedKeyAsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAw\","
    "      \"k\": \"AAECAw\""
    "    }"
    "  ]"
    "}";

const uint8_t kIv[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// kOriginalData encrypted with kKey and kIv but without any subsamples (or
// equivalently using kSubsampleEntriesCypherOnly).
const uint8_t kEncryptedData[] = {
    0x2f, 0x03, 0x09, 0xef, 0x71, 0xaf, 0x31, 0x16, 0xfa, 0x9d, 0x18, 0x43,
    0x1e, 0x96, 0x71, 0xb5, 0xbf, 0xf5, 0x30, 0x53, 0x9a, 0x20, 0xdf, 0x95};

// kOriginalData encrypted with kSubsampleKey and kSubsampleIv using
// kSubsampleEntriesNormal.
const uint8_t kSubsampleEncryptedData[] = {
    0x4f, 0x72, 0x09, 0x16, 0x09, 0xe6, 0x79, 0xad, 0x70, 0x73, 0x75, 0x62,
    0x09, 0xbb, 0x83, 0x1d, 0x4d, 0x08, 0xd7, 0x78, 0xa4, 0xa7, 0xf1, 0x2e};

const uint8_t kOriginalData2[] = "Changed Original data.";

const uint8_t kIv2[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t kKeyId2[] = {
    // base64 equivalent is AAECAwQFBgcICQoLDA0ODxAREhM=
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13};

const char kKey2AsJWK[] =
    "{"
    "  \"keys\": ["
    "    {"
    "      \"kty\": \"oct\","
    "      \"alg\": \"A128KW\","
    "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
    "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
    "    }"
    "  ]"
    "}";

// 'k' in bytes is x14x15x16x17x18x19x1ax1bx1cx1dx1ex1fx20x21x22x23

const uint8_t kEncryptedData2[] = {
    0x57, 0x66, 0xf4, 0x12, 0x1a, 0xed, 0xb5, 0x79, 0x1c, 0x8e, 0x25,
    0xd7, 0x17, 0xe7, 0x5e, 0x16, 0xe3, 0x40, 0x08, 0x27, 0x11, 0xe9};

// Subsample entries for testing. The sum of |cypher_bytes| and |clear_bytes| of
// all entries must be equal to kOriginalDataSize to make the subsample entries
// valid.

const SubsampleEntry kSubsampleEntriesNormal[] = {
  { 2, 7 },
  { 3, 11 },
  { 1, 0 }
};

const SubsampleEntry kSubsampleEntriesWrongSize[] = {
  { 3, 6 }, // This entry doesn't match the correct entry.
  { 3, 11 },
  { 1, 0 }
};

const SubsampleEntry kSubsampleEntriesInvalidTotalSize[] = {
  { 1, 1000 }, // This entry is too large.
  { 3, 11 },
  { 1, 0 }
};

const SubsampleEntry kSubsampleEntriesClearOnly[] = {
  { 7, 0 },
  { 8, 0 },
  { 9, 0 }
};

const SubsampleEntry kSubsampleEntriesCypherOnly[] = {
  { 0, 6 },
  { 0, 8 },
  { 0, 10 }
};

static scoped_refptr<DecoderBuffer> CreateEncryptedBuffer(
    const std::vector<uint8_t>& data,
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& iv,
    const std::vector<SubsampleEntry>& subsample_entries) {
  DCHECK(!data.empty());
  scoped_refptr<DecoderBuffer> encrypted_buffer(new DecoderBuffer(data.size()));
  memcpy(encrypted_buffer->writable_data(), &data[0], data.size());
  CHECK(encrypted_buffer.get());
  std::string key_id_string(
      reinterpret_cast<const char*>(key_id.empty() ? NULL : &key_id[0]),
      key_id.size());
  std::string iv_string(
      reinterpret_cast<const char*>(iv.empty() ? NULL : &iv[0]), iv.size());
  encrypted_buffer->set_decrypt_config(std::unique_ptr<DecryptConfig>(
      new DecryptConfig(key_id_string, iv_string, subsample_entries)));
  return encrypted_buffer;
}

enum ExpectedResult { RESOLVED, REJECTED };

// These tests only test decryption logic (no decoding). Parameter to this
// test specifies how the CDM should be loaded.
//  "AesDecryptor" - use AesDecryptor directly.
//  "CdmAdapter" - load ExternalClearKey using CdmAdapter.
class AesDecryptorTest : public testing::TestWithParam<std::string> {
 public:
  AesDecryptorTest()
      : decrypt_cb_(base::Bind(&AesDecryptorTest::BufferDecrypted,
                               base::Unretained(this))),
        original_data_(kOriginalData, kOriginalData + kOriginalDataSize),
        encrypted_data_(kEncryptedData,
                        kEncryptedData + arraysize(kEncryptedData)),
        subsample_encrypted_data_(
            kSubsampleEncryptedData,
            kSubsampleEncryptedData + arraysize(kSubsampleEncryptedData)),
        key_id_(kKeyId, kKeyId + arraysize(kKeyId)),
        iv_(kIv, kIv + arraysize(kIv)),
        normal_subsample_entries_(
            kSubsampleEntriesNormal,
            kSubsampleEntriesNormal + arraysize(kSubsampleEntriesNormal)) {}

 protected:
  void SetUp() override {
    if (GetParam() == "AesDecryptor") {
      OnCdmCreated(
          new AesDecryptor(GURL::EmptyGURL(),
                           base::Bind(&AesDecryptorTest::OnSessionMessage,
                                      base::Unretained(this)),
                           base::Bind(&AesDecryptorTest::OnSessionClosed,
                                      base::Unretained(this)),
                           base::Bind(&AesDecryptorTest::OnSessionKeysChange,
                                      base::Unretained(this))),
          std::string());
    } else if (GetParam() == "CdmAdapter") {
      CdmConfig cdm_config;  // default settings of false are sufficient.

      helper_.reset(new ExternalClearKeyTestHelper());
      std::unique_ptr<CdmAllocator> allocator(new SimpleCdmAllocator());
      CdmAdapter::Create(
          helper_->KeySystemName(), helper_->LibraryPath(), cdm_config,
          std::move(allocator), base::Bind(&AesDecryptorTest::CreateCdmFileIO,
                                           base::Unretained(this)),
          base::Bind(&AesDecryptorTest::OnSessionMessage,
                     base::Unretained(this)),
          base::Bind(&AesDecryptorTest::OnSessionClosed,
                     base::Unretained(this)),
          base::Bind(&AesDecryptorTest::OnLegacySessionError,
                     base::Unretained(this)),
          base::Bind(&AesDecryptorTest::OnSessionKeysChange,
                     base::Unretained(this)),
          base::Bind(&AesDecryptorTest::OnSessionExpirationUpdate,
                     base::Unretained(this)),
          base::Bind(&AesDecryptorTest::OnCdmCreated, base::Unretained(this)));

      base::RunLoop().RunUntilIdle();
    }
  }

  void TearDown() override {
    if (GetParam() == "CdmAdapter")
      helper_.reset();
  }

  void OnCdmCreated(const scoped_refptr<MediaKeys>& cdm,
                    const std::string& error_message) {
    EXPECT_EQ(error_message, "");
    cdm_ = cdm;
    decryptor_ = cdm_->GetCdmContext()->GetDecryptor();
  }

  void OnResolveWithSession(ExpectedResult expected_result,
                            const std::string& session_id) {
    EXPECT_EQ(expected_result, RESOLVED) << "Unexpectedly resolved.";
    EXPECT_GT(session_id.length(), 0ul);
    session_id_ = session_id;
  }

  void OnResolve(ExpectedResult expected_result) {
    EXPECT_EQ(expected_result, RESOLVED) << "Unexpectedly resolved.";
  }

  void OnReject(ExpectedResult expected_result,
                MediaKeys::Exception exception_code,
                uint32_t system_code,
                const std::string& error_message) {
    EXPECT_EQ(expected_result, REJECTED)
        << "Unexpectedly rejected with message: " << error_message;
  }

  std::unique_ptr<SimpleCdmPromise> CreatePromise(
      ExpectedResult expected_result) {
    std::unique_ptr<SimpleCdmPromise> promise(new CdmCallbackPromise<>(
        base::Bind(&AesDecryptorTest::OnResolve, base::Unretained(this),
                   expected_result),
        base::Bind(&AesDecryptorTest::OnReject, base::Unretained(this),
                   expected_result)));
    return promise;
  }

  std::unique_ptr<NewSessionCdmPromise> CreateSessionPromise(
      ExpectedResult expected_result) {
    std::unique_ptr<NewSessionCdmPromise> promise(
        new CdmCallbackPromise<std::string>(
            base::Bind(&AesDecryptorTest::OnResolveWithSession,
                       base::Unretained(this), expected_result),
            base::Bind(&AesDecryptorTest::OnReject, base::Unretained(this),
                       expected_result)));
    return promise;
  }

  // Creates a new session using |key_id|. Returns the session ID.
  std::string CreateSession(const std::vector<uint8_t>& key_id) {
    DCHECK(!key_id.empty());
    EXPECT_CALL(*this, OnSessionMessage(IsNotEmpty(), _, IsJSONDictionary(),
                                        GURL::EmptyGURL()));
    cdm_->CreateSessionAndGenerateRequest(MediaKeys::TEMPORARY_SESSION,
                                          EmeInitDataType::WEBM, key_id,
                                          CreateSessionPromise(RESOLVED));
    // This expects the promise to be called synchronously, which is the case
    // for AesDecryptor.
    return session_id_;
  }

  // Closes the session specified by |session_id|.
  void CloseSession(const std::string& session_id) {
    EXPECT_CALL(*this, OnSessionClosed(session_id));
    cdm_->CloseSession(session_id, CreatePromise(RESOLVED));
  }

  // Removes the session specified by |session_id|. This should simply do a
  // CloseSession().
  // TODO(jrummell): Clean this up when the prefixed API is removed.
  // http://crbug.com/249976.
  void RemoveSession(const std::string& session_id) {
    if (GetParam() == "AesDecryptor") {
      EXPECT_CALL(*this, OnSessionClosed(session_id));
      cdm_->RemoveSession(session_id, CreatePromise(RESOLVED));
    } else {
      // CdmAdapter fails as only persistent sessions can be removed.
      cdm_->RemoveSession(session_id, CreatePromise(REJECTED));
    }
  }

  MOCK_METHOD2(OnSessionKeysChangeCalled,
               void(const std::string& session_id,
                    bool has_additional_usable_key));

  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info) {
    keys_info_.swap(keys_info);
    OnSessionKeysChangeCalled(session_id, has_additional_usable_key);
  }

  // Updates the session specified by |session_id| with |key|. |result|
  // tests that the update succeeds or generates an error.
  void UpdateSessionAndExpect(std::string session_id,
                              const std::string& key,
                              ExpectedResult expected_result,
                              bool new_key_expected) {
    DCHECK(!key.empty());

    if (expected_result == RESOLVED) {
      EXPECT_CALL(*this,
                  OnSessionKeysChangeCalled(session_id, new_key_expected));
    } else {
      EXPECT_CALL(*this, OnSessionKeysChangeCalled(_, _)).Times(0);
    }

    cdm_->UpdateSession(session_id,
                        std::vector<uint8_t>(key.begin(), key.end()),
                        CreatePromise(expected_result));
  }

  bool KeysInfoContains(std::vector<uint8_t> expected) {
    for (auto* key_id : keys_info_) {
      if (key_id->key_id == expected)
        return true;
    }
    return false;
  }

  MOCK_METHOD2(BufferDecrypted, void(Decryptor::Status,
                                     const scoped_refptr<DecoderBuffer>&));

  enum DecryptExpectation {
    SUCCESS,
    DATA_MISMATCH,
    DATA_AND_SIZE_MISMATCH,
    DECRYPT_ERROR,
    NO_KEY
  };

  void DecryptAndExpect(const scoped_refptr<DecoderBuffer>& encrypted,
                        const std::vector<uint8_t>& plain_text,
                        DecryptExpectation result) {
    scoped_refptr<DecoderBuffer> decrypted;

    switch (result) {
      case SUCCESS:
      case DATA_MISMATCH:
      case DATA_AND_SIZE_MISMATCH:
        EXPECT_CALL(*this, BufferDecrypted(Decryptor::kSuccess, NotNull()))
            .WillOnce(SaveArg<1>(&decrypted));
        break;
      case DECRYPT_ERROR:
        EXPECT_CALL(*this, BufferDecrypted(Decryptor::kError, IsNull()))
            .WillOnce(SaveArg<1>(&decrypted));
        break;
      case NO_KEY:
        EXPECT_CALL(*this, BufferDecrypted(Decryptor::kNoKey, IsNull()))
            .WillOnce(SaveArg<1>(&decrypted));
        break;
    }

    if (GetParam() == "CdmAdapter") {
      ANNOTATE_SCOPED_MEMORY_LEAK;  // http://crbug.com/569736
      decryptor_->Decrypt(Decryptor::kVideo, encrypted, decrypt_cb_);
    } else {
      decryptor_->Decrypt(Decryptor::kVideo, encrypted, decrypt_cb_);
    }

    std::vector<uint8_t> decrypted_text;
    if (decrypted.get() && decrypted->data_size()) {
      decrypted_text.assign(
        decrypted->data(), decrypted->data() + decrypted->data_size());
    }

    switch (result) {
      case SUCCESS:
        EXPECT_EQ(plain_text, decrypted_text);
        break;
      case DATA_MISMATCH:
        EXPECT_EQ(plain_text.size(), decrypted_text.size());
        EXPECT_NE(plain_text, decrypted_text);
        break;
      case DATA_AND_SIZE_MISMATCH:
        EXPECT_NE(plain_text.size(), decrypted_text.size());
        break;
      case DECRYPT_ERROR:
      case NO_KEY:
        EXPECT_TRUE(decrypted_text.empty());
        break;
    }
  }

  std::unique_ptr<CdmFileIO> CreateCdmFileIO(cdm::FileIOClient* client) {
    ADD_FAILURE() << "Should never be called";
    return nullptr;
  }

  MOCK_METHOD4(OnSessionMessage,
               void(const std::string& session_id,
                    MediaKeys::MessageType message_type,
                    const std::vector<uint8_t>& message,
                    const GURL& legacy_destination_url));
  MOCK_METHOD1(OnSessionClosed, void(const std::string& session_id));
  MOCK_METHOD4(OnLegacySessionError,
               void(const std::string& session_id,
                    MediaKeys::Exception exception,
                    uint32_t system_code,
                    const std::string& error_message));
  MOCK_METHOD2(OnSessionExpirationUpdate,
               void(const std::string& session_id,
                    const base::Time& new_expiry_time));

  scoped_refptr<MediaKeys> cdm_;
  Decryptor* decryptor_;
  Decryptor::DecryptCB decrypt_cb_;
  std::string session_id_;
  CdmKeysInfo keys_info_;

  // Helper class to load/unload External Clear Key Library, if necessary.
  std::unique_ptr<ExternalClearKeyTestHelper> helper_;

  base::MessageLoop message_loop_;

  // Constants for testing.
  const std::vector<uint8_t> original_data_;
  const std::vector<uint8_t> encrypted_data_;
  const std::vector<uint8_t> subsample_encrypted_data_;
  const std::vector<uint8_t> key_id_;
  const std::vector<uint8_t> iv_;
  const std::vector<SubsampleEntry> normal_subsample_entries_;
  const std::vector<SubsampleEntry> no_subsample_entries_;
};

TEST_P(AesDecryptorTest, CreateSessionWithNullInitData) {
  EXPECT_CALL(*this,
              OnSessionMessage(IsNotEmpty(), _, IsEmpty(), GURL::EmptyGURL()));
  cdm_->CreateSessionAndGenerateRequest(
      MediaKeys::TEMPORARY_SESSION, EmeInitDataType::WEBM,
      std::vector<uint8_t>(), CreateSessionPromise(RESOLVED));
}

TEST_P(AesDecryptorTest, MultipleCreateSession) {
  EXPECT_CALL(*this,
              OnSessionMessage(IsNotEmpty(), _, IsEmpty(), GURL::EmptyGURL()));
  cdm_->CreateSessionAndGenerateRequest(
      MediaKeys::TEMPORARY_SESSION, EmeInitDataType::WEBM,
      std::vector<uint8_t>(), CreateSessionPromise(RESOLVED));

  EXPECT_CALL(*this,
              OnSessionMessage(IsNotEmpty(), _, IsEmpty(), GURL::EmptyGURL()));
  cdm_->CreateSessionAndGenerateRequest(
      MediaKeys::TEMPORARY_SESSION, EmeInitDataType::WEBM,
      std::vector<uint8_t>(), CreateSessionPromise(RESOLVED));

  EXPECT_CALL(*this,
              OnSessionMessage(IsNotEmpty(), _, IsEmpty(), GURL::EmptyGURL()));
  cdm_->CreateSessionAndGenerateRequest(
      MediaKeys::TEMPORARY_SESSION, EmeInitDataType::WEBM,
      std::vector<uint8_t>(), CreateSessionPromise(RESOLVED));
}

TEST_P(AesDecryptorTest, CreateSessionWithCencInitData) {
  const uint8_t init_data[] = {
      0x00, 0x00, 0x00, 0x44,                          // size = 68
      0x70, 0x73, 0x73, 0x68,                          // 'pssh'
      0x01,                                            // version
      0x00, 0x00, 0x00,                                // flags
      0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // SystemID
      0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
      0x00, 0x00, 0x00, 0x02,                          // key count
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,  // key1
      0x7E, 0x57, 0x1D, 0x03, 0x7E, 0x57, 0x1D, 0x03,
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,  // key2
      0x7E, 0x57, 0x1D, 0x04, 0x7E, 0x57, 0x1D, 0x04,
      0x00, 0x00, 0x00, 0x00  // datasize
  };

#if defined(USE_PROPRIETARY_CODECS)
  EXPECT_CALL(*this, OnSessionMessage(IsNotEmpty(), _, IsJSONDictionary(),
                                      GURL::EmptyGURL()));
  cdm_->CreateSessionAndGenerateRequest(
      MediaKeys::TEMPORARY_SESSION, EmeInitDataType::CENC,
      std::vector<uint8_t>(init_data, init_data + arraysize(init_data)),
      CreateSessionPromise(RESOLVED));
#else
  cdm_->CreateSessionAndGenerateRequest(
      MediaKeys::TEMPORARY_SESSION, EmeInitDataType::CENC,
      std::vector<uint8_t>(init_data, init_data + arraysize(init_data)),
      CreateSessionPromise(REJECTED));
#endif
}

TEST_P(AesDecryptorTest, CreateSessionWithKeyIdsInitData) {
  const char init_data[] =
      "{\"kids\":[\"AQI\",\"AQIDBA\",\"AQIDBAUGBwgJCgsMDQ4PEA\"]}";

  EXPECT_CALL(*this, OnSessionMessage(IsNotEmpty(), _, IsJSONDictionary(),
                                      GURL::EmptyGURL()));
  cdm_->CreateSessionAndGenerateRequest(
      MediaKeys::TEMPORARY_SESSION, EmeInitDataType::KEYIDS,
      std::vector<uint8_t>(init_data, init_data + arraysize(init_data) - 1),
      CreateSessionPromise(RESOLVED));
}

TEST_P(AesDecryptorTest, NormalDecryption) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_P(AesDecryptorTest, UnencryptedFrame) {
  // An empty iv string signals that the frame is unencrypted.
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      original_data_, key_id_, std::vector<uint8_t>(), no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_P(AesDecryptorTest, WrongKey) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kWrongKeyAsJWK, RESOLVED, true);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_P(AesDecryptorTest, NoKey) {
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);
  EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kNoKey, IsNull()));
  decryptor_->Decrypt(Decryptor::kVideo, encrypted_buffer, decrypt_cb_);
}

TEST_P(AesDecryptorTest, KeyReplacement) {
  std::string session_id = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);

  UpdateSessionAndExpect(session_id, kWrongKeyAsJWK, RESOLVED, true);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpect(
      encrypted_buffer, original_data_, DATA_MISMATCH));

  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, false);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));
}

TEST_P(AesDecryptorTest, WrongSizedKey) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kWrongSizedKeyAsJWK, REJECTED, true);
}

TEST_P(AesDecryptorTest, MultipleKeysAndFrames) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  UpdateSessionAndExpect(session_id, kKey2AsJWK, RESOLVED, true);

  // The first key is still available after we added a second key.
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  // The second key is also available.
  encrypted_buffer = CreateEncryptedBuffer(
      std::vector<uint8_t>(kEncryptedData2,
                           kEncryptedData2 + arraysize(kEncryptedData2)),
      std::vector<uint8_t>(kKeyId2, kKeyId2 + arraysize(kKeyId2)),
      std::vector<uint8_t>(kIv2, kIv2 + arraysize(kIv2)),
      no_subsample_entries_);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpect(
      encrypted_buffer,
      std::vector<uint8_t>(kOriginalData2,
                           kOriginalData2 + arraysize(kOriginalData2) - 1),
      SUCCESS));
}

TEST_P(AesDecryptorTest, CorruptedIv) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);

  std::vector<uint8_t> bad_iv = iv_;
  bad_iv[1]++;

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, bad_iv, no_subsample_entries_);

  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_P(AesDecryptorTest, CorruptedData) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);

  std::vector<uint8_t> bad_data = encrypted_data_;
  bad_data[1]++;

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      bad_data, key_id_, iv_, no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_P(AesDecryptorTest, EncryptedAsUnencryptedFailure) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, std::vector<uint8_t>(), no_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_P(AesDecryptorTest, SubsampleDecryption) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_, normal_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

// Ensures noninterference of data offset and subsample mechanisms. We never
// expect to encounter this in the wild, but since the DecryptConfig doesn't
// disallow such a configuration, it should be covered.
TEST_P(AesDecryptorTest, SubsampleDecryptionWithOffset) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_, normal_subsample_entries_);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_P(AesDecryptorTest, SubsampleWrongSize) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);

  std::vector<SubsampleEntry> subsample_entries_wrong_size(
      kSubsampleEntriesWrongSize,
      kSubsampleEntriesWrongSize + arraysize(kSubsampleEntriesWrongSize));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_, subsample_entries_wrong_size);
  DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH);
}

TEST_P(AesDecryptorTest, SubsampleInvalidTotalSize) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);

  std::vector<SubsampleEntry> subsample_entries_invalid_total_size(
      kSubsampleEntriesInvalidTotalSize,
      kSubsampleEntriesInvalidTotalSize +
          arraysize(kSubsampleEntriesInvalidTotalSize));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      subsample_encrypted_data_, key_id_, iv_,
      subsample_entries_invalid_total_size);
  DecryptAndExpect(encrypted_buffer, original_data_, DECRYPT_ERROR);
}

// No cypher bytes in any of the subsamples.
TEST_P(AesDecryptorTest, SubsampleClearBytesOnly) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);

  std::vector<SubsampleEntry> clear_only_subsample_entries(
      kSubsampleEntriesClearOnly,
      kSubsampleEntriesClearOnly + arraysize(kSubsampleEntriesClearOnly));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      original_data_, key_id_, iv_, clear_only_subsample_entries);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

// No clear bytes in any of the subsamples.
TEST_P(AesDecryptorTest, SubsampleCypherBytesOnly) {
  std::string session_id = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);

  std::vector<SubsampleEntry> cypher_only_subsample_entries(
      kSubsampleEntriesCypherOnly,
      kSubsampleEntriesCypherOnly + arraysize(kSubsampleEntriesCypherOnly));

  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, cypher_only_subsample_entries);
  DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS);
}

TEST_P(AesDecryptorTest, CloseSession) {
  std::string session_id = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);

  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  CloseSession(session_id);
}

TEST_P(AesDecryptorTest, RemoveSession) {
  // TODO(jrummell): Clean this up when the prefixed API is removed.
  // http://crbug.com/249976.
  std::string session_id = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);

  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  RemoveSession(session_id);
}

TEST_P(AesDecryptorTest, NoKeyAfterCloseSession) {
  std::string session_id = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);

  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  CloseSession(session_id);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, NO_KEY));
}

TEST_P(AesDecryptorTest, LatestKeyUsed) {
  std::string session_id1 = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);

  // Add alternate key, buffer should not be decoded properly.
  UpdateSessionAndExpect(session_id1, kKeyAlternateAsJWK, RESOLVED, true);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH));

  // Create a second session with a correct key value for key_id_.
  std::string session_id2 = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id2, kKeyAsJWK, RESOLVED, true);

  // Should be able to decode with latest key.
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));
}

TEST_P(AesDecryptorTest, LatestKeyUsedAfterCloseSession) {
  std::string session_id1 = CreateSession(key_id_);
  scoped_refptr<DecoderBuffer> encrypted_buffer = CreateEncryptedBuffer(
      encrypted_data_, key_id_, iv_, no_subsample_entries_);
  UpdateSessionAndExpect(session_id1, kKeyAsJWK, RESOLVED, true);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));

  // Create a second session with a different key value for key_id_.
  std::string session_id2 = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id2, kKeyAlternateAsJWK, RESOLVED, true);

  // Should not be able to decode with new key.
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, DATA_MISMATCH));

  // Close second session, should revert to original key.
  CloseSession(session_id2);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpect(encrypted_buffer, original_data_, SUCCESS));
}

TEST_P(AesDecryptorTest, JWKKey) {
  std::string session_id = CreateSession(key_id_);

  // Try a simple JWK key (i.e. not in a set)
  const std::string kJwkSimple =
      "{"
      "  \"kty\": \"oct\","
      "  \"alg\": \"A128KW\","
      "  \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "  \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "}";
  UpdateSessionAndExpect(session_id, kJwkSimple, REJECTED, true);

  // Try a key list with multiple entries.
  const std::string kJwksMultipleEntries =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4_QA\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksMultipleEntries, RESOLVED, true);

  // Try a key with no spaces and some \n plus additional fields.
  const std::string kJwksNoSpaces =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\",\"alg\":\"A128KW\","
      "\"kid\":\"AQIDBAUGBwgJCgsMCg4PAA\",\"k\":\"GawgguFyGrWKav7AX4VKUg"
      "\",\"foo\":\"bar\"}]}\n\n";
  UpdateSessionAndExpect(session_id, kJwksNoSpaces, RESOLVED, true);

  // Try some non-ASCII characters.
  UpdateSessionAndExpect(session_id,
                         "This is not ASCII due to \xff\xfe\xfd in it.",
                         REJECTED, true);

  // Try a badly formatted key. Assume that the JSON parser is fully tested,
  // so we won't try a lot of combinations. However, need a test to ensure
  // that the code doesn't crash if invalid JSON received.
  UpdateSessionAndExpect(session_id, "This is not a JSON key.", REJECTED, true);

  // Try passing some valid JSON that is not a dictionary at the top level.
  UpdateSessionAndExpect(session_id, "40", REJECTED, true);

  // Try an empty dictionary.
  UpdateSessionAndExpect(session_id, "{ }", REJECTED, true);

  // Try an empty 'keys' dictionary.
  UpdateSessionAndExpect(session_id, "{ \"keys\": [] }", REJECTED, true);

  // Try with 'keys' not a dictionary.
  UpdateSessionAndExpect(session_id, "{ \"keys\":\"1\" }", REJECTED, true);

  // Try with 'keys' a list of integers.
  UpdateSessionAndExpect(session_id, "{ \"keys\": [ 1, 2, 3 ] }", REJECTED,
                         true);

  // Try padding(=) at end of 'k' base64 string.
  const std::string kJwksWithPaddedKey =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAw\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw==\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithPaddedKey, REJECTED, true);

  // Try padding(=) at end of 'kid' base64 string.
  const std::string kJwksWithPaddedKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"AAECAw==\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithPaddedKeyId, REJECTED, true);

  // Try a key with invalid base64 encoding.
  const std::string kJwksWithInvalidBase64 =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"!@#$%^&*()\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithInvalidBase64, REJECTED, true);

  // Try a 3-byte 'kid' where no base64 padding is required.
  // |kJwksMultipleEntries| above has 2 'kid's that require 1 and 2 padding
  // bytes. Note that 'k' has to be 16 bytes, so it will always require padding.
  const std::string kJwksWithNoPadding =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"Kiss\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithNoPadding, RESOLVED, true);

  // Empty key id.
  const std::string kJwksWithEmptyKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"alg\": \"A128KW\","
      "      \"kid\": \"\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  UpdateSessionAndExpect(session_id, kJwksWithEmptyKeyId, REJECTED, true);
  CloseSession(session_id);
}

TEST_P(AesDecryptorTest, GetKeyIds) {
  std::vector<uint8_t> key_id1(kKeyId, kKeyId + arraysize(kKeyId));
  std::vector<uint8_t> key_id2(kKeyId2, kKeyId2 + arraysize(kKeyId2));

  std::string session_id = CreateSession(key_id_);
  EXPECT_FALSE(KeysInfoContains(key_id1));
  EXPECT_FALSE(KeysInfoContains(key_id2));

  // Add 1 key, verify it is returned.
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  EXPECT_TRUE(KeysInfoContains(key_id1));
  EXPECT_FALSE(KeysInfoContains(key_id2));

  // Add second key, verify both IDs returned.
  UpdateSessionAndExpect(session_id, kKey2AsJWK, RESOLVED, true);
  EXPECT_TRUE(KeysInfoContains(key_id1));
  EXPECT_TRUE(KeysInfoContains(key_id2));
}

TEST_P(AesDecryptorTest, NoKeysChangeForSameKey) {
  std::vector<uint8_t> key_id(kKeyId, kKeyId + arraysize(kKeyId));

  std::string session_id = CreateSession(key_id_);
  EXPECT_FALSE(KeysInfoContains(key_id));

  // Add key, verify it is returned.
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, true);
  EXPECT_TRUE(KeysInfoContains(key_id));

  // Add key a second time.
  UpdateSessionAndExpect(session_id, kKeyAsJWK, RESOLVED, false);
  EXPECT_TRUE(KeysInfoContains(key_id));

  // Create a new session. Add key, should indicate key added for this session.
  std::string session_id2 = CreateSession(key_id_);
  UpdateSessionAndExpect(session_id2, kKeyAsJWK, RESOLVED, true);
}

INSTANTIATE_TEST_CASE_P(AesDecryptor,
                        AesDecryptorTest,
                        testing::Values("AesDecryptor"));

#if defined(ENABLE_PEPPER_CDMS)
INSTANTIATE_TEST_CASE_P(CdmAdapter,
                        AesDecryptorTest,
                        testing::Values("CdmAdapter"));
#endif

// TODO(jrummell): Once MojoCdm/MojoCdmService/MojoDecryptor/
// MojoDecryptorService are implemented, add a third version that tests the
// CDM via mojo.

}  // namespace media
