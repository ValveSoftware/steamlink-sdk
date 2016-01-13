// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_RENDER_SETTINGS_H_
#define PRINTING_PDF_RENDER_SETTINGS_H_

#include "base/tuple.h"
#include "ipc/ipc_param_traits.h"
#include "printing/printing_export.h"
#include "ui/gfx/rect.h"

namespace printing {

// Defining PDF rendering settings here as a Tuple as following:
// gfx::Rect - render area
// int - render dpi
// bool - autorotate pages to fit paper
typedef Tuple3<gfx::Rect, int, bool> PdfRenderSettingsBase;

class PdfRenderSettings : public PdfRenderSettingsBase {
 public:
  PdfRenderSettings() : PdfRenderSettingsBase() {}
  PdfRenderSettings(gfx::Rect area, int dpi, bool autorotate)
      : PdfRenderSettingsBase(area, dpi, autorotate) {}
  ~PdfRenderSettings() {}

  const gfx::Rect& area() const { return a; }
  int dpi() const { return b; }
  bool autorotate() const { return c; }
};

}  // namespace printing

namespace IPC {
template <>
struct SimilarTypeTraits<printing::PdfRenderSettings> {
  typedef printing::PdfRenderSettingsBase Type;
};

}  // namespace IPC

#endif  // PRINTING_PDF_RENDER_SETTINGS_H_

