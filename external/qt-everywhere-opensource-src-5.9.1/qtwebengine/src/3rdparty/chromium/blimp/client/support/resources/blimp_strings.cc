// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/support/resources/blimp_strings.h"

namespace blimp {
namespace string {

base::string16 BlimpPrefix(const base::string16& str) {
  return l10n_util::GetStringFUTF16(IDS_BLIMP_TAG, str);
}

}  // namespace string
}  // namespace blimp
