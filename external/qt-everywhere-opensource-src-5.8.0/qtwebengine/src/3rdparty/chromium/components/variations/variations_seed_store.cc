// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "crypto/signature_verifier.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/zlib/google/compression_utils.h"

#if defined(OS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // OS_ANDROID

namespace variations {

namespace {

// Signature verification is disabled on mobile platforms for now, since it
// adds about ~15ms to the startup time on mobile (vs. a couple ms on desktop).
bool SignatureVerificationEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return false;
#else
  return true;
#endif
}

// The ECDSA public key of the variations server for verifying variations seed
// signatures.
const uint8_t kPublicKey[] = {
  0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
  0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
  0x04, 0x51, 0x7c, 0x31, 0x4b, 0x50, 0x42, 0xdd, 0x59, 0xda, 0x0b, 0xfa, 0x43,
  0x44, 0x33, 0x7c, 0x5f, 0xa1, 0x0b, 0xd5, 0x82, 0xf6, 0xac, 0x04, 0x19, 0x72,
  0x6c, 0x40, 0xd4, 0x3e, 0x56, 0xe2, 0xa0, 0x80, 0xa0, 0x41, 0xb3, 0x23, 0x7b,
  0x71, 0xc9, 0x80, 0x87, 0xde, 0x35, 0x0d, 0x25, 0x71, 0x09, 0x7f, 0xb4, 0x15,
  0x2b, 0xff, 0x82, 0x4d, 0xd3, 0xfe, 0xc5, 0xef, 0x20, 0xc6, 0xa3, 0x10, 0xbf,
};

// Note: UMA histogram enum - don't re-order or remove entries.
enum VariationSeedEmptyState {
  VARIATIONS_SEED_NOT_EMPTY,
  VARIATIONS_SEED_EMPTY,
  VARIATIONS_SEED_CORRUPT,
  VARIATIONS_SEED_INVALID_SIGNATURE,
  VARIATIONS_SEED_CORRUPT_BASE64,
  VARIATIONS_SEED_CORRUPT_PROTOBUF,
  VARIATIONS_SEED_CORRUPT_GZIP,
  VARIATIONS_SEED_EMPTY_ENUM_SIZE,
};

void RecordVariationSeedEmptyHistogram(VariationSeedEmptyState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedEmpty", state,
                            VARIATIONS_SEED_EMPTY_ENUM_SIZE);
}

enum VariationsSeedStoreResult {
  VARIATIONS_SEED_STORE_SUCCESS,
  VARIATIONS_SEED_STORE_FAILED_EMPTY,
  VARIATIONS_SEED_STORE_FAILED_PARSE,
  VARIATIONS_SEED_STORE_FAILED_SIGNATURE,
  VARIATIONS_SEED_STORE_FAILED_GZIP,
  // DELTA_COUNT is not so much a result of the seed store, but rather counting
  // the number of delta-compressed seeds the SeedStore() function saw. Kept in
  // the same histogram for convenience of comparing against the other values.
  VARIATIONS_SEED_STORE_DELTA_COUNT,
  VARIATIONS_SEED_STORE_FAILED_DELTA_READ_SEED,
  VARIATIONS_SEED_STORE_FAILED_DELTA_APPLY,
  VARIATIONS_SEED_STORE_FAILED_DELTA_STORE,
  VARIATIONS_SEED_STORE_FAILED_UNGZIP,
  VARIATIONS_SEED_STORE_FAILED_EMPTY_GZIP_CONTENTS,
  VARIATIONS_SEED_STORE_FAILED_UNSUPPORTED_SEED_FORMAT,
  VARIATIONS_SEED_STORE_RESULT_ENUM_SIZE,
};

void RecordSeedStoreHistogram(VariationsSeedStoreResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedStoreResult", result,
                            VARIATIONS_SEED_STORE_RESULT_ENUM_SIZE);
}

// Note: UMA histogram enum - don't re-order or remove entries.
enum VariationsSeedDateChangeState {
  SEED_DATE_NO_OLD_DATE,
  SEED_DATE_NEW_DATE_OLDER,
  SEED_DATE_SAME_DAY,
  SEED_DATE_NEW_DAY,
  SEED_DATE_ENUM_SIZE,
};

// Truncates a time to the start of the day in UTC. If given a time representing
// 2014-03-11 10:18:03.1 UTC, it will return a time representing
// 2014-03-11 00:00:00.0 UTC.
base::Time TruncateToUTCDay(const base::Time& time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  return base::Time::FromUTCExploded(exploded);
}

VariationsSeedDateChangeState GetSeedDateChangeState(
    const base::Time& server_seed_date,
    const base::Time& stored_seed_date) {
  if (server_seed_date < stored_seed_date)
    return SEED_DATE_NEW_DATE_OLDER;

  if (TruncateToUTCDay(server_seed_date) !=
      TruncateToUTCDay(stored_seed_date)) {
    // The server date is earlier than the stored date, and they are from
    // different UTC days, so |server_seed_date| is a valid new day.
    return SEED_DATE_NEW_DAY;
  }
  return SEED_DATE_SAME_DAY;
}

#if defined(OS_ANDROID)
enum FirstRunResult {
  FIRST_RUN_SEED_IMPORT_SUCCESS,
  FIRST_RUN_SEED_IMPORT_FAIL_NO_CALLBACK,
  FIRST_RUN_SEED_IMPORT_FAIL_NO_FIRST_RUN_SEED,
  FIRST_RUN_SEED_IMPORT_FAIL_STORE_FAILED,
  FIRST_RUN_RESULT_ENUM_SIZE,
};

void RecordFirstRunResult(FirstRunResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.FirstRunResult", result,
                            FIRST_RUN_RESULT_ENUM_SIZE);
}
#endif  // OS_ANDROID

}  // namespace

VariationsSeedStore::VariationsSeedStore(PrefService* local_state)
    : local_state_(local_state), seed_has_country_code_(false) {
}

VariationsSeedStore::~VariationsSeedStore() {
}

bool VariationsSeedStore::LoadSeed(variations::VariationsSeed* seed) {
  invalid_base64_signature_.clear();

#if defined(OS_ANDROID)
  if (!local_state_->HasPrefPath(prefs::kVariationsSeedSignature))
    ImportFirstRunJavaSeed();
#endif  // OS_ANDROID

  std::string seed_data;
  if (!ReadSeedData(&seed_data))
    return false;

  const std::string base64_seed_signature =
      local_state_->GetString(prefs::kVariationsSeedSignature);
  const VerifySignatureResult result =
      VerifySeedSignature(seed_data, base64_seed_signature);
  if (result != VARIATIONS_SEED_SIGNATURE_ENUM_SIZE) {
    UMA_HISTOGRAM_ENUMERATION("Variations.LoadSeedSignature", result,
                              VARIATIONS_SEED_SIGNATURE_ENUM_SIZE);
    if (result != VARIATIONS_SEED_SIGNATURE_VALID) {
      ClearPrefs();
      RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_INVALID_SIGNATURE);
      // Record the invalid signature.
      invalid_base64_signature_ = base64_seed_signature;
      return false;
    }
  }

  if (!seed->ParseFromString(seed_data)) {
    ClearPrefs();
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_CORRUPT_PROTOBUF);
    return false;
  }

  // Migrate any existing country code from the seed to the pref, if the pref is
  // empty. TODO(asvitkine): Clean up the code in M50+ when sufficient number
  // of clients have migrated.
  if (seed->has_country_code() &&
      local_state_->GetString(prefs::kVariationsCountry).empty()) {
    local_state_->SetString(prefs::kVariationsCountry, seed->country_code());
  }
  variations_serial_number_ = seed->serial_number();
  seed_has_country_code_ = seed->has_country_code();
  RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_NOT_EMPTY);
  return true;
}

bool VariationsSeedStore::StoreSeedData(
    const std::string& data,
    const std::string& base64_seed_signature,
    const std::string& country_code,
    const base::Time& date_fetched,
    bool is_delta_compressed,
    bool is_gzip_compressed,
    variations::VariationsSeed* parsed_seed) {
  // If the data is gzip compressed, first uncompress it.
  std::string ungzipped_data;
  if (is_gzip_compressed) {
    if (compression::GzipUncompress(data, &ungzipped_data)) {
      if (ungzipped_data.empty()) {
        RecordSeedStoreHistogram(
            VARIATIONS_SEED_STORE_FAILED_EMPTY_GZIP_CONTENTS);
        return false;
      }

      int size_reduction = ungzipped_data.length() - data.length();
      UMA_HISTOGRAM_PERCENTAGE("Variations.StoreSeed.GzipSize.ReductionPercent",
                               100 * size_reduction / ungzipped_data.length());
      UMA_HISTOGRAM_COUNTS_1000("Variations.StoreSeed.GzipSize",
                                data.length() / 1024);
    } else {
      RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_UNGZIP);
      return false;
    }
  } else {
    ungzipped_data = data;
  }

  if (!is_delta_compressed) {
    const bool result =
        StoreSeedDataNoDelta(ungzipped_data, base64_seed_signature,
                             country_code, date_fetched, parsed_seed);
    if (result) {
      UMA_HISTOGRAM_COUNTS_1000("Variations.StoreSeed.Size",
                                ungzipped_data.length() / 1024);
    }
    return result;
  }

  // If the data is delta compressed, first decode it.
  DCHECK(invalid_base64_signature_.empty());
  RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_DELTA_COUNT);

  std::string existing_seed_data;
  std::string updated_seed_data;
  if (!ReadSeedData(&existing_seed_data)) {
    RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_DELTA_READ_SEED);
    return false;
  }
  if (!ApplyDeltaPatch(existing_seed_data, ungzipped_data,
                       &updated_seed_data)) {
    RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_DELTA_APPLY);
    return false;
  }

  const bool result =
      StoreSeedDataNoDelta(updated_seed_data, base64_seed_signature,
                           country_code, date_fetched, parsed_seed);
  if (result) {
    // Note: |updated_seed_data.length()| is guaranteed to be non-zero, else
    // result would be false.
    int size_reduction = updated_seed_data.length() - ungzipped_data.length();
    UMA_HISTOGRAM_PERCENTAGE("Variations.StoreSeed.DeltaSize.ReductionPercent",
                             100 * size_reduction / updated_seed_data.length());
    UMA_HISTOGRAM_COUNTS_1000("Variations.StoreSeed.DeltaSize",
                              ungzipped_data.length() / 1024);
  } else {
    RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_DELTA_STORE);
  }
  return result;
}

void VariationsSeedStore::UpdateSeedDateAndLogDayChange(
    const base::Time& server_date_fetched) {
  VariationsSeedDateChangeState date_change = SEED_DATE_NO_OLD_DATE;

  if (local_state_->HasPrefPath(prefs::kVariationsSeedDate)) {
    const int64_t stored_date_value =
        local_state_->GetInt64(prefs::kVariationsSeedDate);
    const base::Time stored_date =
        base::Time::FromInternalValue(stored_date_value);

    date_change = GetSeedDateChangeState(server_date_fetched, stored_date);
  }

  UMA_HISTOGRAM_ENUMERATION("Variations.SeedDateChange", date_change,
                            SEED_DATE_ENUM_SIZE);

  local_state_->SetInt64(prefs::kVariationsSeedDate,
                         server_date_fetched.ToInternalValue());
}

std::string VariationsSeedStore::GetInvalidSignature() const {
  return invalid_base64_signature_;
}

// static
void VariationsSeedStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kVariationsCompressedSeed, std::string());
  registry->RegisterStringPref(prefs::kVariationsSeed, std::string());
  registry->RegisterInt64Pref(prefs::kVariationsSeedDate,
                              base::Time().ToInternalValue());
  registry->RegisterStringPref(prefs::kVariationsSeedSignature, std::string());
  registry->RegisterStringPref(prefs::kVariationsCountry, std::string());
}

VariationsSeedStore::VerifySignatureResult
VariationsSeedStore::VerifySeedSignature(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature) {
  if (!SignatureVerificationEnabled())
    return VARIATIONS_SEED_SIGNATURE_ENUM_SIZE;

  if (base64_seed_signature.empty())
    return VARIATIONS_SEED_SIGNATURE_MISSING;

  std::string signature;
  if (!base::Base64Decode(base64_seed_signature, &signature))
    return VARIATIONS_SEED_SIGNATURE_DECODE_FAILED;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                           reinterpret_cast<const uint8_t*>(signature.data()),
                           signature.size(), kPublicKey,
                           arraysize(kPublicKey))) {
    return VARIATIONS_SEED_SIGNATURE_INVALID_SIGNATURE;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(seed_bytes.data()),
                        seed_bytes.size());
  if (verifier.VerifyFinal())
    return VARIATIONS_SEED_SIGNATURE_VALID;
  return VARIATIONS_SEED_SIGNATURE_INVALID_SEED;
}

void VariationsSeedStore::ClearPrefs() {
  local_state_->ClearPref(prefs::kVariationsCompressedSeed);
  local_state_->ClearPref(prefs::kVariationsSeed);
  local_state_->ClearPref(prefs::kVariationsSeedDate);
  local_state_->ClearPref(prefs::kVariationsSeedSignature);
}

#if defined(OS_ANDROID)
void VariationsSeedStore::ImportFirstRunJavaSeed() {
  DVLOG(1) << "Importing first run seed from Java preferences.";

  std::string seed_data;
  std::string seed_signature;
  std::string seed_country;
  std::string response_date;
  bool is_gzip_compressed;

  android::GetVariationsFirstRunSeed(&seed_data, &seed_signature, &seed_country,
                                     &response_date, &is_gzip_compressed);
  if (seed_data.empty()) {
    RecordFirstRunResult(FIRST_RUN_SEED_IMPORT_FAIL_NO_FIRST_RUN_SEED);
    return;
  }

  base::Time current_date;
  base::Time::FromUTCString(response_date.c_str(), &current_date);

  if (!StoreSeedData(seed_data, seed_signature, seed_country, current_date,
                     false, is_gzip_compressed, nullptr)) {
    RecordFirstRunResult(FIRST_RUN_SEED_IMPORT_FAIL_STORE_FAILED);
    LOG(WARNING) << "First run variations seed is invalid.";
    return;
  }
  RecordFirstRunResult(FIRST_RUN_SEED_IMPORT_SUCCESS);
}
#endif  // OS_ANDROID

bool VariationsSeedStore::ReadSeedData(std::string* seed_data) {
  std::string base64_seed_data =
      local_state_->GetString(prefs::kVariationsCompressedSeed);
  const bool is_compressed = !base64_seed_data.empty();
  // If there's no compressed seed, fall back to the uncompressed one.
  if (!is_compressed)
    base64_seed_data = local_state_->GetString(prefs::kVariationsSeed);

  if (base64_seed_data.empty()) {
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_EMPTY);
    return false;
  }

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string decoded_data;
  if (!base::Base64Decode(base64_seed_data, &decoded_data)) {
    ClearPrefs();
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_CORRUPT_BASE64);
    return false;
  }

  if (!is_compressed) {
    seed_data->swap(decoded_data);
  } else if (!compression::GzipUncompress(decoded_data, seed_data)) {
    ClearPrefs();
    RecordVariationSeedEmptyHistogram(VARIATIONS_SEED_CORRUPT_GZIP);
    return false;
  }

  return true;
}

bool VariationsSeedStore::StoreSeedDataNoDelta(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    const std::string& country_code,
    const base::Time& date_fetched,
    variations::VariationsSeed* parsed_seed) {
  if (seed_data.empty()) {
    RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_EMPTY_GZIP_CONTENTS);
    return false;
  }

  // Only store the seed data if it parses correctly.
  variations::VariationsSeed seed;
  if (!seed.ParseFromString(seed_data)) {
    RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_PARSE);
    return false;
  }

  const VerifySignatureResult result =
      VerifySeedSignature(seed_data, base64_seed_signature);
  if (result != VARIATIONS_SEED_SIGNATURE_ENUM_SIZE) {
    UMA_HISTOGRAM_ENUMERATION("Variations.StoreSeedSignature", result,
                              VARIATIONS_SEED_SIGNATURE_ENUM_SIZE);
    if (result != VARIATIONS_SEED_SIGNATURE_VALID) {
      RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_SIGNATURE);
      return false;
    }
  }

  // Compress the seed before base64-encoding and storing.
  std::string compressed_seed_data;
  if (!compression::GzipCompress(seed_data, &compressed_seed_data)) {
    RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_FAILED_GZIP);
    return false;
  }

  std::string base64_seed_data;
  base::Base64Encode(compressed_seed_data, &base64_seed_data);

  // TODO(asvitkine): This pref is no longer being used. Remove it completely
  // in M45+.
  local_state_->ClearPref(prefs::kVariationsSeed);

#if defined(OS_ANDROID)
  // If currently we do not have any stored pref then we mark seed storing as
  // successful on the Java side of Chrome for Android to avoid repeated seed
  // fetches and clear preferences on the Java side.
  if (local_state_->GetString(prefs::kVariationsCompressedSeed).empty()) {
    android::MarkVariationsSeedAsStored();
    android::ClearJavaFirstRunPrefs();
  }
#endif

  // Update the saved country code only if one was returned from the server.
  // Prefer the country code that was transmitted in the header over the one in
  // the seed (which is deprecated).
  if (!country_code.empty())
    local_state_->SetString(prefs::kVariationsCountry, country_code);
  else if (seed.has_country_code())
    local_state_->SetString(prefs::kVariationsCountry, seed.country_code());

  local_state_->SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
  UpdateSeedDateAndLogDayChange(date_fetched);
  local_state_->SetString(prefs::kVariationsSeedSignature,
                          base64_seed_signature);
  variations_serial_number_ = seed.serial_number();
  if (parsed_seed)
    seed.Swap(parsed_seed);

  RecordSeedStoreHistogram(VARIATIONS_SEED_STORE_SUCCESS);
  return true;
}

// static
bool VariationsSeedStore::ApplyDeltaPatch(const std::string& existing_data,
                                          const std::string& patch,
                                          std::string* output) {
  output->clear();

  google::protobuf::io::CodedInputStream in(
      reinterpret_cast<const uint8_t*>(patch.data()), patch.length());
  // Temporary string declared outside the loop so it can be re-used between
  // different iterations (rather than allocating new ones).
  std::string temp;

  const uint32_t existing_data_size =
      static_cast<uint32_t>(existing_data.size());
  while (in.CurrentPosition() != static_cast<int>(patch.length())) {
    uint32_t value;
    if (!in.ReadVarint32(&value))
      return false;

    if (value != 0) {
      // A non-zero value indicates the number of bytes to copy from the patch
      // stream to the output.

      // No need to guard against bad data (i.e. very large |value|) because the
      // call below will fail if |value| is greater than the size of the patch.
      if (!in.ReadString(&temp, value))
        return false;
      output->append(temp);
    } else {
      // Otherwise, when it's zero, it indicates that it's followed by a pair of
      // numbers - |offset| and |length| that specify a range of data to copy
      // from |existing_data|.
      uint32_t offset;
      uint32_t length;
      if (!in.ReadVarint32(&offset) || !in.ReadVarint32(&length))
        return false;

      // Check for |offset + length| being out of range and for overflow.
      base::CheckedNumeric<uint32_t> end_offset(offset);
      end_offset += length;
      if (!end_offset.IsValid() || end_offset.ValueOrDie() > existing_data_size)
        return false;
      output->append(existing_data, offset, length);
    }
  }
  return true;
}

void VariationsSeedStore::ReportUnsupportedSeedFormatError() {
  RecordSeedStoreHistogram(
      VARIATIONS_SEED_STORE_FAILED_UNSUPPORTED_SEED_FORMAT);
}

}  // namespace variations
