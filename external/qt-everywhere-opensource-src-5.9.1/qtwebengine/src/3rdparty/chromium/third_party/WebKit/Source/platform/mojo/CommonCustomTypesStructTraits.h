// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CommonCustomTypesStructTraits_h
#define CommonCustomTypesStructTraits_h

#include "base/strings/string16.h"
#include "mojo/common/common_custom_types.mojom-blink.h"
#include "platform/PlatformExport.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT
    StructTraits<common::mojom::String16DataView, WTF::String> {
  static bool IsNull(const WTF::String& input) { return input.isNull(); }
  static void SetToNull(WTF::String* output) { *output = WTF::String(); }

  static void* SetUpContext(const WTF::String& input);
  static void TearDownContext(const WTF::String& input, void* context) {
    delete static_cast<base::string16*>(context);
  }

  static ConstCArray<uint16_t> data(const WTF::String& input, void* context);
  static bool Read(common::mojom::String16DataView, WTF::String* out);
};

}  // namespace mojo

#endif  // CommonCustomTypesStructTraits_h
