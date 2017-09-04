// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/color/gfx_param_traits.h"

#include "ui/gfx/color_space.h"
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"

namespace IPC {

void ParamTraits<gfx::ColorSpace>::GetSize(base::PickleSizer* s,
                                           const gfx::ColorSpace& p) {
  GetParamSize(s, p.primaries_);
  GetParamSize(s, p.transfer_);
  GetParamSize(s, p.matrix_);
  GetParamSize(s, p.range_);
  GetParamSize(s, p.icc_profile_id_);
  if (p.primaries_ == gfx::ColorSpace::PrimaryID::CUSTOM) {
    for (int i = 0; i < 12; i++)
      GetParamSize(s, p.custom_primary_matrix_[i]);
  }
}

void ParamTraits<gfx::ColorSpace>::Write(base::Pickle* m,
                                         const gfx::ColorSpace& p) {
  WriteParam(m, p.primaries_);
  WriteParam(m, p.transfer_);
  WriteParam(m, p.matrix_);
  WriteParam(m, p.range_);
  WriteParam(m, p.icc_profile_id_);
  if (p.primaries_ == gfx::ColorSpace::PrimaryID::CUSTOM) {
    for (int i = 0; i < 12; i++)
      WriteParam(m, p.custom_primary_matrix_[i]);
  }
}

bool ParamTraits<gfx::ColorSpace>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        gfx::ColorSpace* r) {
  if (!ReadParam(m, iter, &r->primaries_))
    return false;
  if (!ReadParam(m, iter, &r->transfer_))
    return false;
  if (!ReadParam(m, iter, &r->matrix_))
    return false;
  if (!ReadParam(m, iter, &r->range_))
    return false;
  if (!ReadParam(m, iter, &r->icc_profile_id_))
    return false;

  if (r->primaries_ == gfx::ColorSpace::PrimaryID::CUSTOM) {
    for (int i = 0; i < 12; i++) {
      if (!ReadParam(m, iter, r->custom_primary_matrix_ + i))
        return false;
    }
  }
  return true;
}

void ParamTraits<gfx::ColorSpace>::Log(const gfx::ColorSpace& p,
                                       std::string* l) {
  l->append("<gfx::ColorSpace>");
}

}  // namespace IPC

// Generate param traits size methods.
#include "ipc/param_traits_size_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"
}  // namespace IPC
