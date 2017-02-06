// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_
#define CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "content/public/common/origin_trial_policy.h"

namespace content {

class ShellOriginTrialPolicy : public OriginTrialPolicy {
 public:
  ShellOriginTrialPolicy();
  ~ShellOriginTrialPolicy() override;

  // OriginTrialPolicy interface
  base::StringPiece GetPublicKey() const override;
  bool IsFeatureDisabled(base::StringPiece feature) const override;

 private:
  base::StringPiece public_key_;

  DISALLOW_COPY_AND_ASSIGN(ShellOriginTrialPolicy);
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_
