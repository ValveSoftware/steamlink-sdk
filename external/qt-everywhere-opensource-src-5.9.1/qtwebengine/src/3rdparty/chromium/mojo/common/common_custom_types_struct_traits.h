// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_COMMON_CUSTOM_TYPES_STRUCT_TRAITS_H_
#define MOJO_COMMON_COMMON_CUSTOM_TYPES_STRUCT_TRAITS_H_

#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/version.h"
#include "mojo/common/common_custom_types.mojom-shared.h"
#include "mojo/common/mojo_common_export.h"

namespace mojo {

template <>
struct StructTraits<common::mojom::String16DataView, base::string16> {
  static ConstCArray<uint16_t> data(const base::string16& str) {
    return ConstCArray<uint16_t>(str.size(),
                                 reinterpret_cast<const uint16_t*>(str.data()));
  }

  static bool Read(common::mojom::String16DataView data, base::string16* out);
};

template <>
struct StructTraits<common::mojom::VersionDataView, base::Version> {
  static bool IsNull(const base::Version& version) {
    return !version.IsValid();
  }
  static void SetToNull(base::Version* out) {
    *out = base::Version(std::string());
  }
  static const std::vector<uint32_t>& components(const base::Version& version);
  static bool Read(common::mojom::VersionDataView data, base::Version* out);
};

// If base::UnguessableToken is no longer 128 bits, the logic below and the
// mojom::UnguessableToken type should be updated.
static_assert(sizeof(base::UnguessableToken) == 2 * sizeof(uint64_t),
              "base::UnguessableToken should be of size 2 * sizeof(uint64_t).");

template <>
struct StructTraits<common::mojom::UnguessableTokenDataView,
                    base::UnguessableToken> {
  static uint64_t high(const base::UnguessableToken& token) {
    return token.GetHighForSerialization();
  }

  static uint64_t low(const base::UnguessableToken& token) {
    return token.GetLowForSerialization();
  }

  static bool Read(common::mojom::UnguessableTokenDataView data,
                   base::UnguessableToken* out);
};

template <>
struct StructTraits<common::mojom::TimeDeltaDataView, base::TimeDelta> {
  static int64_t microseconds(const base::TimeDelta& delta) {
    return delta.InMicroseconds();
  }

  static bool Read(common::mojom::TimeDeltaDataView data,
                   base::TimeDelta* delta) {
    *delta = base::TimeDelta::FromMicroseconds(data.microseconds());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_COMMON_COMMON_CUSTOM_TYPES_STRUCT_TRAITS_H_
