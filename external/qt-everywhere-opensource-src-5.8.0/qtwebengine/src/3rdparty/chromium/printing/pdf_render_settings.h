// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_RENDER_SETTINGS_H_
#define PRINTING_PDF_RENDER_SETTINGS_H_

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "printing/printing_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

namespace printing {

// Defining PDF rendering settings.
class PdfRenderSettings {
 public:
  PdfRenderSettings() {}
  PdfRenderSettings(gfx::Rect area, int dpi, bool autorotate)
      : area_(area), dpi_(dpi), autorotate_(autorotate) {}
  ~PdfRenderSettings() {}

  const gfx::Rect& area() const { return area_; }
  int dpi() const { return dpi_; }
  bool autorotate() const { return autorotate_; }

  gfx::Rect area_;
  int dpi_;
  bool autorotate_;
};

}  // namespace printing

namespace IPC {
template <>
struct ParamTraits<printing::PdfRenderSettings> {
  typedef printing::PdfRenderSettings param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.area_);
    WriteParam(m, p.dpi_);
    WriteParam(m, p.autorotate_);
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return ReadParam(m, iter, &r->area_) &&
        ReadParam(m, iter, &r->dpi_) &&
        ReadParam(m, iter, &r->autorotate_);
  }

  static void Log(const param_type& p, std::string* l) {
    LogParam(p.area_, l);
    l->append(", ");
    LogParam(p.dpi_, l);
    l->append(", ");
    LogParam(p.autorotate_, l);
  }
};

}  // namespace IPC

#endif  // PRINTING_PDF_RENDER_SETTINGS_H_
