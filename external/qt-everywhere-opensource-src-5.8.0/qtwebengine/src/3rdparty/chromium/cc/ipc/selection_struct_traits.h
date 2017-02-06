// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SELECTION_STRUCT_TRAITS_H_
#define CC_IPC_SELECTION_STRUCT_TRAITS_H_

#include "cc/input/selection.h"
#include "cc/ipc/selection.mojom.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::Selection, cc::Selection<gfx::SelectionBound>> {
  static const gfx::SelectionBound& start(
      const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.start;
  }

  static const gfx::SelectionBound& end(
      const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.end;
  }

  static bool is_editable(const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.is_editable;
  }

  static bool is_empty_text_form_control(
      const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.is_empty_text_form_control;
  }

  static bool Read(cc::mojom::SelectionDataView data,
                   cc::Selection<gfx::SelectionBound>* out) {
    if (!data.ReadStart(&out->start) || !data.ReadEnd(&out->end))
      return false;

    out->is_editable = data.is_editable();
    out->is_empty_text_form_control = data.is_empty_text_form_control();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_SELECTION_STRUCT_TRAITS_H_
