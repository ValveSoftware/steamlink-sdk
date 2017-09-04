// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SUPPORT_RESOURCES_BLIMP_STRINGS_H_
#define BLIMP_CLIENT_SUPPORT_RESOURCES_BLIMP_STRINGS_H_

#include "blimp/client/support/resources/grit/blimp_strings.h"

#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"

// Include this header to access blimp support string id.
// Do not directly include grit/blimp_strings.h.
namespace blimp {
namespace string {

base::string16 BlimpPrefix(const base::string16& str);

}  // namespace string
}  // namespace blimp

#endif  // BLIMP_CLIENT_SUPPORT_RESOURCES_BLIMP_STRINGS_H_
