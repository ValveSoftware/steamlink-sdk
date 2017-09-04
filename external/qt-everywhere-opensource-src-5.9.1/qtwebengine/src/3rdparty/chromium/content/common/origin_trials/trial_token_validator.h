// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_
#define CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace blink {
enum class WebOriginTrialTokenStatus;
}

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

namespace content {

namespace TrialTokenValidator {

using FeatureToTokensMap = std::map<std::string /* feature_name */,
                                    std::vector<std::string /* token */>>;

// If token validates, |*feature_name| is set to the name of the feature the
// token enables.
// This method is thread-safe.
CONTENT_EXPORT blink::WebOriginTrialTokenStatus ValidateToken(
    const std::string& token,
    const url::Origin& origin,
    std::string* feature_name);

CONTENT_EXPORT bool RequestEnablesFeature(const net::URLRequest* request,
                                          base::StringPiece feature_name);

CONTENT_EXPORT bool RequestEnablesFeature(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers,
    base::StringPiece feature_name);

// Returns all valid tokens in |headers|.
CONTENT_EXPORT std::unique_ptr<FeatureToTokensMap> GetValidTokensFromHeaders(
    const url::Origin& origin,
    const net::HttpResponseHeaders* headers);

// Returns all valid tokens in |tokens|. This method is used to re-validate
// previously stored tokens.
CONTENT_EXPORT std::unique_ptr<FeatureToTokensMap> GetValidTokens(
    const url::Origin& origin,
    const FeatureToTokensMap& tokens);

}  // namespace TrialTokenValidator

}  // namespace content

#endif  // CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_TOKEN_VALIDATOR_H_
