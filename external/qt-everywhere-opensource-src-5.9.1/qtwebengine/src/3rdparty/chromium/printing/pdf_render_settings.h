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
struct PdfRenderSettings {
  PdfRenderSettings() : dpi(0), autorotate(false) {}
  PdfRenderSettings(gfx::Rect area, int dpi, bool autorotate)
      : area(area), dpi(dpi), autorotate(autorotate) {}
  ~PdfRenderSettings() {}

  gfx::Rect area;
  int dpi;
  bool autorotate;
};

}  // namespace printing

namespace IPC {
template <>
struct ParamTraits<printing::PdfRenderSettings> {
  typedef printing::PdfRenderSettings param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.area);
    WriteParam(m, p.dpi);
    WriteParam(m, p.autorotate);
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return ReadParam(m, iter, &r->area) &&
        ReadParam(m, iter, &r->dpi) &&
        ReadParam(m, iter, &r->autorotate);
  }

  static void Log(const param_type& p, std::string* l) {
    LogParam(p.area, l);
    l->append(", ");
    LogParam(p.dpi, l);
    l->append(", ");
    LogParam(p.autorotate, l);
  }
};

}  // namespace IPC

#endif  // PRINTING_PDF_RENDER_SETTINGS_H_
