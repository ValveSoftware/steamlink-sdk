// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_RESOURCES_BLIMP_STRINGS_H_
#define BLIMP_CLIENT_PUBLIC_RESOURCES_BLIMP_STRINGS_H_

#include "base/macros.h"
#include "blimp/client/public/session/assignment.h"

namespace blimp {
namespace string {

// Utilities to access internal Blimp strings.
//
// 1. Conditional compile based on |enable_blimp_client| GN flag.
// if true, compiled with core/resources/blimp_strings.cc.
// if false, compiled with core/resources/dummy_blimp_strings.cc, which returns
// empty strings and doesn't build with internal resources.
//
// 2. Grit file generation.
// The |grit/blimp_strings.h| header defines the string id,
// and is auto-generated based on configuration in
// |blimp/client/core/resources/blimp_strings.grd|, and
// |tools/gritsettings/resource_ids|.
// The actual strings are defined in |blimp_strings.grd|.
//
// 3. Public external strings, can be directly included for
// |blimp/client/support/resources/blimp_strings.h|, and is generated from
// |blimp/client/support/resources| build target.

// Converts an AssignmentRequestResult to human readable string.
base::string16 AssignmentResultErrorToString(
    blimp::client::AssignmentRequestResult result);

// Converts EndConnectionMessage::Reason to human readable string.
base::string16 EndConnectionMessageToString(int reason);

}  // namespace string
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_RESOURCES_BLIMP_STRINGS_H_
