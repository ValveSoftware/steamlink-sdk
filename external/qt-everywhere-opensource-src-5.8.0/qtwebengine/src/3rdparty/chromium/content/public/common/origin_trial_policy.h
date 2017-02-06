// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ORIGIN_TRIAL_POLICY_H_
#define CONTENT_PUBLIC_COMMON_ORIGIN_TRIAL_POLICY_H_

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT OriginTrialPolicy {
 public:
  virtual ~OriginTrialPolicy() = default;

  virtual void Initialize() {}
  virtual base::StringPiece GetPublicKey() const = 0;
  virtual bool IsFeatureDisabled(base::StringPiece feature) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ORIGIN_TRIAL_POLICY_H_
