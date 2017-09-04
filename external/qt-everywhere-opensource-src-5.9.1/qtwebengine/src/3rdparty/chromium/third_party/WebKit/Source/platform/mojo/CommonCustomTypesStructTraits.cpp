// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mojo/CommonCustomTypesStructTraits.h"

#include <cstring>

namespace mojo {

// static
void* StructTraits<common::mojom::String16DataView, WTF::String>::SetUpContext(
    const WTF::String& input) {
  // If it is null (i.e., StructTraits<>::IsNull() returns true), this method is
  // guaranteed not to be called.
  DCHECK(!input.isNull());

  if (!input.is8Bit())
    return nullptr;

  return new base::string16(input.characters8(),
                            input.characters8() + input.length());
}

// static
ConstCArray<uint16_t> StructTraits<common::mojom::String16DataView,
                                   WTF::String>::data(const WTF::String& input,
                                                      void* context) {
  auto contextObject = static_cast<base::string16*>(context);
  DCHECK_EQ(input.is8Bit(), !!contextObject);

  if (contextObject) {
    return ConstCArray<uint16_t>(
        contextObject->size(),
        reinterpret_cast<const uint16_t*>(contextObject->data()));
  }

  return ConstCArray<uint16_t>(
      input.length(), reinterpret_cast<const uint16_t*>(input.characters16()));
}

// static
bool StructTraits<common::mojom::String16DataView, WTF::String>::Read(
    common::mojom::String16DataView data,
    WTF::String* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  *out = WTF::String(reinterpret_cast<const UChar*>(view.data()), view.size());
  return true;
}

}  // namespace mojo
