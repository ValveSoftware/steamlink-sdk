// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojo/icc_profile_struct_traits.h"

#include <algorithm>

#include "mojo/public/cpp/bindings/string_data_view.h"

namespace mojo {

using Traits = StructTraits<gfx::mojom::ICCProfileDataView, gfx::ICCProfile>;

// static
bool Traits::Read(gfx::mojom::ICCProfileDataView data, gfx::ICCProfile* out) {
  if (!data.ReadType(&out->type_))
    return false;
  if (!data.ReadColorSpace(&out->color_space_))
    return false;
  out->id_ = data.id();

  mojo::StringDataView view;
  data.GetDataDataView(&view);
  out->data_.resize(view.size());
  std::copy(view.storage(), view.storage() + view.size(), out->data_.begin());
  return true;
}

}  // namespace mojo
