// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_response_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_tree_head.h"

namespace net {

namespace ct {

namespace {

// Structure for making JSON decoding easier. The string fields
// are base64-encoded so will require further decoding.
struct JsonSignedTreeHead {
  int tree_size;
  double timestamp;
  std::string sha256_root_hash;
  DigitallySigned signature;

  static void RegisterJSONConverter(
      base::JSONValueConverter<JsonSignedTreeHead>* converted);
};

bool ConvertSHA256RootHash(const base::StringPiece& s, std::string* result) {
  if (!base::Base64Decode(s, result)) {
    DVLOG(1) << "Failed decoding sha256_root_hash";
    return false;
  }

  if (result->length() != kSthRootHashLength) {
    DVLOG(1) << "sha256_root_hash is expected to be 32 bytes, but is "
             << result->length() << " bytes.";
    return false;
  }

  return true;
}

bool ConvertTreeHeadSignature(const base::StringPiece& s,
                              DigitallySigned* result) {
  std::string tree_head_signature;
  if (!base::Base64Decode(s, &tree_head_signature)) {
    DVLOG(1) << "Failed decoding tree_head_signature";
    return false;
  }

  base::StringPiece sp(tree_head_signature);
  if (!DecodeDigitallySigned(&sp, result)) {
    DVLOG(1) << "Failed decoding signature to DigitallySigned";
    return false;
  }
  return true;
}

void JsonSignedTreeHead::RegisterJSONConverter(
    base::JSONValueConverter<JsonSignedTreeHead>* converter) {
  converter->RegisterIntField("tree_size", &JsonSignedTreeHead::tree_size);
  converter->RegisterDoubleField("timestamp", &JsonSignedTreeHead::timestamp);
  converter->RegisterCustomField("sha256_root_hash",
                                 &JsonSignedTreeHead::sha256_root_hash,
                                 &ConvertSHA256RootHash);
  converter->RegisterCustomField<DigitallySigned>(
      "tree_head_signature",
      &JsonSignedTreeHead::signature,
      &ConvertTreeHeadSignature);
}

bool IsJsonSTHStructurallyValid(const JsonSignedTreeHead& sth) {
  if (sth.tree_size < 0) {
    DVLOG(1) << "Tree size in Signed Tree Head JSON is negative: "
             << sth.tree_size;
    return false;
  }

  if (sth.timestamp < 0) {
    DVLOG(1) << "Timestamp in Signed Tree Head JSON is negative: "
             << sth.timestamp;
    return false;
  }

  if (sth.sha256_root_hash.empty()) {
    DVLOG(1) << "Missing SHA256 root hash from Signed Tree Head JSON.";
    return false;
  }

  if (sth.signature.signature_data.empty()) {
    DVLOG(1) << "Missing SHA256 root hash from Signed Tree Head JSON.";
    return false;
  }

  return true;
}

}  // namespace

bool FillSignedTreeHead(const base::StringPiece& json_signed_tree_head,
                        SignedTreeHead* signed_tree_head) {
  base::JSONReader json_reader;
  scoped_ptr<base::Value> json(json_reader.Read(json_signed_tree_head));
  if (json.get() == NULL) {
    DVLOG(1) << "Empty Signed Tree Head JSON.";
    return false;
  }

  JsonSignedTreeHead parsed_sth;
  base::JSONValueConverter<JsonSignedTreeHead> converter;
  if (!converter.Convert(*json.get(), &parsed_sth)) {
    DVLOG(1) << "Invalid Signed Tree Head JSON.";
    return false;
  }

  if (!IsJsonSTHStructurallyValid(parsed_sth))
    return false;

  signed_tree_head->version = SignedTreeHead::V1;
  signed_tree_head->tree_size = parsed_sth.tree_size;
  signed_tree_head->timestamp =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(parsed_sth.timestamp);
  signed_tree_head->signature = parsed_sth.signature;
  memcpy(signed_tree_head->sha256_root_hash,
         parsed_sth.sha256_root_hash.c_str(),
         kSthRootHashLength);
  return true;
}

}  // namespace ct

}  // namespace net
