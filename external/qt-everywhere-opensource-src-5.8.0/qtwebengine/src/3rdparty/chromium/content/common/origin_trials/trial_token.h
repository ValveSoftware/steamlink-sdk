// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_H_
#define CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace blink {
enum class WebOriginTrialTokenStatus;
}

namespace content {

// The Experimental Framework (EF) provides limited access to experimental
// features, on a per-origin basis (origin trials). This class defines the trial
// token data structure, used to securely provide access to an experimental
// feature.
//
// Features are defined by string names, provided by the implementers. The EF
// code does not maintain an enum or constant list for feature names. Instead,
// the EF validates the name provided by the feature implementation against any
// provided tokens.
//
// More documentation on the token format can be found at
// https://docs.google.com/document/d/1v5fi0EUV_QHckVHVF2K4P72iNywnrJtNhNZ6i2NPt0M

class CONTENT_EXPORT TrialToken {
 public:
  ~TrialToken();

  // If the string represents a signed well-formed token, a token object is
  // returned, and success is returned in the |out_status| parameter. Otherwise,
  // the |out_status| parameter indicates what was wrong with the string, and
  // nullptr is returned.
  // Note that success does not mean that the token is currently valid, or
  // appropriate for a given origin / feature. It only means that it is
  // correctly formatted and signed by the supplied public key, and can be
  // parsed.
  static std::unique_ptr<TrialToken> From(
      const std::string& token_text,
      base::StringPiece public_key,
      blink::WebOriginTrialTokenStatus* out_status);

  // Returns success if this token is appropriate for use by the given origin
  // and feature name, and has not yet expired. Otherwise, the return value
  // indicates why the token is not valid.
  blink::WebOriginTrialTokenStatus IsValidForFeature(
      const url::Origin& origin,
      base::StringPiece feature_name,
      const base::Time& now) const;

  url::Origin origin() { return origin_; }
  std::string feature_name() { return feature_name_; }
  base::Time expiry_time() { return expiry_time_; }

 protected:
  friend class TrialTokenTest;

  // If the string represents a properly signed and well-formed token, the token
  // payload is returned in the |out_token_payload| parameter and success is
  // returned. Otherwise,the return code indicates what was wrong with the
  // string, and |out_token_payload| is unchanged.
  static blink::WebOriginTrialTokenStatus Extract(
      const std::string& token_text,
      base::StringPiece public_key,
      std::string* out_token_payload);

  // Returns a token object if the string represents a well-formed JSON token
  // payload, or nullptr otherwise.
  static std::unique_ptr<TrialToken> Parse(const std::string& token_payload);

  bool ValidateOrigin(const url::Origin& origin) const;
  bool ValidateFeatureName(base::StringPiece feature_name) const;
  bool ValidateDate(const base::Time& now) const;

  static bool ValidateSignature(base::StringPiece signature_text,
                                const std::string& data,
                                base::StringPiece public_key);

 private:
  TrialToken(const url::Origin& origin,
             const std::string& feature_name,
             uint64_t expiry_timestamp);

  // The origin for which this token is valid. Must be a secure origin.
  url::Origin origin_;

  // The name of the experimental feature which this token enables.
  std::string feature_name_;

  // The time until which this token should be considered valid.
  base::Time expiry_time_;
};

}  // namespace content

#endif  // CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_H_
